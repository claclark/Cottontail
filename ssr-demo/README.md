# SSR Demo

This directory holds documentation and support material for the Cottontail
Boolean SSR agentic search demo. The web interface is
[`apps/ssr-web.py`](../apps/ssr-web.py); the backend is
[`apps/ssr-server.cc`](../apps/ssr-server.cc).

The commands below run from the repository root, not from `ssr-demo/`.

## Run the Web Interface

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
the web demo. After editing `apps/ssr-web.py`, restart it and reload the browser
page.
