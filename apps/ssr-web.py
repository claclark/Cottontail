#!/usr/bin/env python3
"""Minimal web interface to Cottontail SSR. Standard library only."""

import argparse
import html
import json
import re
import socket
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PAGE = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Cottontail Boolean SRR agentic search ({{INFO}})</title>
<style>
  body {
    font: 16px/1.6 system-ui, sans-serif;
    max-width: 900px;
    margin: 48px auto;
    padding: 0 24px;
    color: #222;
  }
  h1 { margin-bottom: 20px; }
  form { display: flex; gap: 10px; }
  input {
    flex: 1; min-width: 0; padding: 11px;
    font: inherit; border: 1px solid #aaa;
    border-radius: 5px;
  }
  button {
    padding: 10px 18px; font: inherit;
    border: 1px solid #aaa; border-radius: 5px;
    background: #f7f7f7; cursor: pointer;
  }
  button:hover:not(:disabled) { background: #eaeaea; }
  button:disabled { opacity: .4; cursor: default; }
  #rawJson {
    margin-top: 20px; padding: 15px;
    background: #f6f6f6; border: 1px solid #ddd;
    white-space: pre-wrap; overflow-wrap: anywhere;
    max-height: 60vh; overflow-y: auto;
  }
  nav { display: flex; align-items: center; flex-wrap: wrap; gap: 12px; margin-top: 24px; }
  #error { color: #b00020; margin-top: 12px; }
  [hidden] { display: none !important; }
</style>
</head>
<body>
<h1>Cottontail Boolean SRR agentic search ({{INFO}})</h1>

<form id="search">
  <input id="query" placeholder="Enter a Boolean query, e.g. (^ black bear (+ attack attacks))" autocomplete="off" autofocus>
  <button type="submit">Search</button>
</form>

<nav>
  <span>Agent actions</span>
  <button id="fullButton" disabled>Full document</button>
  <button id="next" disabled>Next result</button>
</nav>

<div id="error"></div>

<section id="rawResponse" hidden>
  <pre id="rawJson"></pre>
</section>

<script>
const $ = id => document.getElementById(id);
const exampleQuery = "(^ black bear (+ attack attacks))";

let qid = null;
let currentResult = null;
let exhausted = false;
let busy = false;
let fullDocumentShown = false;

function showRecord(record) {
  const text = JSON.stringify(record, null, 2);
  const fragment = document.createDocumentFragment();
  let offset = 0;
  // Keep all JSON text and literal cover tags; only style the covered text.
  for (const match of text.matchAll(/<cover>([\s\S]*?)<\/cover>/gi)) {
    fragment.append(text.slice(offset, match.index + 7));
    const bold = document.createElement("strong");
    bold.textContent = match[1];
    fragment.append(bold, match[0].slice(-8));
    offset = match.index + match[0].length;
  }
  fragment.append(text.slice(offset));
  $("rawJson").replaceChildren(fragment);
  $("rawResponse").hidden = false;
}

async function api(message) {
  $("rawResponse").hidden = true;
  fullDocumentShown = false;
  const response = await fetch("/api", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(message)
  });
  const record = await response.json();
  showRecord(record);
  const data = record.response ?? record;
  if (!response.ok || data.ok === false)
    throw Error(data.error || data.message || "Request failed");
  fullDocumentShown = data.op === "document";
  return data;
}

function setBusy(value) {
  busy = value;
  $("search").querySelector("button").disabled = value;
  $("next").disabled = value || qid == null || exhausted;
  $("fullButton").disabled = value || currentResult == null || fullDocumentShown;
}

function setError(e) {
  $("error").textContent = e.message || String(e);
}

// Protocol response interpretation is concentrated here.
function isEnd(r) {
  return r.done === true || r.eof === true ||
         r.end === true || r.docno == null;
}

$("search").addEventListener("submit", async e => {
  e.preventDefault();
  if (busy) return;

  setBusy(true);
  $("error").textContent = "";

  // A new query invalidates the old browser-side query state.
  qid = null;
  currentResult = null;
  exhausted = false;

  const query = $("query").value.trim() || exampleQuery;
  $("query").value = query;
  try {
    const r = await api({op: "query", query});

    qid = r.qid;
    if (qid == null) throw Error("SSR did not return a qid");

    if (!isEnd(r)) {
      currentResult = r;
    } else {
      exhausted = true;
    }
  } catch (e) {
    setError(e);
  }
  setBusy(false);
});

$("next").addEventListener("click", async () => {
  if (busy || qid == null || exhausted) return;

  setBusy(true);
  $("error").textContent = "";

  try {
    const r = await api({op: "next", qid});
    if (isEnd(r)) {
      exhausted = true;
    } else {
      currentResult = r;
    }
  } catch (e) {
    setError(e);
  }
  setBusy(false);
});

$("fullButton").addEventListener("click", async () => {
  if (busy || currentResult == null || fullDocumentShown) return;
  setBusy(true);
  $("error").textContent = "";
  try {
    await api({op: "document", docno: currentResult.docno});
  } catch (e) {
    setError(e);
  }
  setBusy(false);
});
</script>
</body>
</html>"""


class SSRConnection:
    """One persistent, synchronized TCP connection to SSR."""

    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.sock = None
        self.reader = None
        self.lock = threading.Lock()

    def close(self):
        if self.reader:
            self.reader.close()
            self.reader = None
        if self.sock:
            self.sock.close()
            self.sock = None

    def request(self, message):
        with self.lock:
            # Reconnect only for a new query. Reconnecting for 'next'
            # could silently lose connection-local server state.
            if self.sock is None:
                if message.get("op") != "query":
                    raise RuntimeError("SSR connection lost; start a new query")
                self.sock = socket.create_connection(
                    (self.host, self.port), timeout=30
                )
                self.sock.settimeout(60)
                self.reader = self.sock.makefile("rb")

            try:
                self.sock.sendall(
                    (json.dumps(message) + "\n").encode("utf-8")
                )
                line = self.reader.readline(10_000_000)
                if not line:
                    raise ConnectionError("SSR closed the connection")
                return json.loads(line)
            except (OSError, ValueError):
                self.close()
                raise


class Handler(BaseHTTPRequestHandler):
    def send_data(self, code, data, content_type):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path != "/":
            self.send_error(404)
            return
        self.send_data(
            200, self.server.page, "text/html; charset=utf-8"
        )

    def do_POST(self):
        if self.path != "/api":
            self.send_error(404)
            return
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not 0 < size <= 65536:
                raise ValueError("Invalid request size")
            message = json.loads(self.rfile.read(size))
            if message.get("op") not in ("query", "next", "document"):
                raise ValueError("Invalid operation")
            result = self.server.ssr.request(message)
            code = 200
        except Exception as e:
            result = {"ok": False, "error": str(e)}
            code = 502

        self.send_data(
            code, json.dumps(result).encode("utf-8"),
            "application/json; charset=utf-8"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("web_port", type=int, nargs="?", default=8080)
    parser.add_argument("ssr_port", type=int)
    parser.add_argument("--ssr-host", default="127.0.0.1")
    parser.add_argument(
        "--info", default="raw interface demo",
        help="text in parentheses in the page heading and browser title"
    )
    args = parser.parse_args()

    server = ThreadingHTTPServer(
        ("127.0.0.1", args.web_port), Handler
    )
    server.page = PAGE.replace("{{INFO}}", html.escape(args.info)).encode("utf-8")
    server.ssr = SSRConnection(args.ssr_host, args.ssr_port)

    print(f"SSR web: http://127.0.0.1:{args.web_port}")
    print(f"SSR server: {args.ssr_host}:{args.ssr_port}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        server.ssr.close()


if __name__ == "__main__":
    main()
