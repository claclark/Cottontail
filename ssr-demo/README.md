
# SSR Demo

This directory holds documentation and support material for the Cottontail
Boolean SSR agentic search demo. The web interface is
[`apps/ssr-web.py`](../apps/ssr-web.py); the backend is
[`apps/ssr-server.cc`](../apps/ssr-server.cc).
The same search service can also be used from the command line or directly by
an agent. The browser provides a view of the JSON exchanged with the service.

## Demo Video

[Watch the SSR demo](https://github.com/user-attachments/assets/d7a6f2b1-3ba5-4b1d-8e3b-40481ecf979e).

## Run the Web Interface

The commands below run from the repository root, not from `ssr-demo/`.

`ssr-web.py` uses only the Python standard library. Run it against an existing
SSR server, using the port printed by that server:

```sh
./apps/ssr-web.py 8080 37815 --info "Climbmix 1.6TB"
```

Here `8080` is the web port and `37815` is an example SSR port; replace it with
your server's port. Open `http://127.0.0.1:8080` on the same machine.
`--info` replaces the parenthetical text in the visible heading and browser
title; it defaults to `raw interface demo`. `--ssr-host` defaults to
`127.0.0.1`.

Search, Full document, and Next result each replace a single formatted JSON
display, including the server's reported time in milliseconds. Covered text is
bold, with the literal `<cover>` tags preserved. An empty search runs the example
query and puts it into the input for editing. Full document is disabled while
its successful response is displayed.

## Use from the Command Line

Two interactive clients are available in `apps/`. Both connect directly to the
SSR server on localhost, using its port rather than the web port.

Build and run the [C++ client](../apps/ssr-client.cc):

```sh
bazel build //apps:ssr-client
./bazel-bin/apps/ssr-client 37815
```

Or run the [Python client](../apps/ssr-client.py), which uses only the standard
library:

```sh
python3 apps/ssr-client.py 37815
```

At the `>>` prompt, enter a Boolean GCL query. Enter `@next` or press Enter on
an empty line for the next result, and enter `@full` for the current result's
full document. Press Ctrl-D to exit.

## Use from an Agent

An agent can use the same JSON-line TCP interface directly to submit a query,
request another result by query ID, or retrieve a full document by document ID.
The browser is not required. The Python client provides a compact example of
sending requests and reading the server's `query`, `response`, and `time`
envelope.

Examples of agent-driven use:

- [TheVole](https://github.com/claclark/TheVole): an LLM-based search agent
  that issues Boolean queries to SSR, judges results, and continues searching
  within a model-call budget.
- [RAG2026](https://github.com/claclark/RAG2026): the WaterlooClarke TREC RAG
  2026 workflow, in which Vole follows research plans, chooses GCL searches,
  reads results from the Cottontail SSR service over ClimbMix, and records
  evidence for cited reports.

## Connect from Another Machine

On the machine running your browser, forward the web port with SSH:

```sh
ssh -N -L 8080:127.0.0.1:8080 user@server
```

Replace `user@server` with your SSH login and leave the tunnel running. Then
open `http://127.0.0.1:8080` locally. Only the web port needs forwarding; the
web app connects to SSR on the server.

## Connections and Restarting

The web app holds one persistent SSR connection. The current SSR server handles
one connected client at a time, so exit an interactive SSR client before using
the web demo, and stop the web app before connecting a command-line client or
agent. After editing `apps/ssr-web.py`, restart it and reload the browser page.
