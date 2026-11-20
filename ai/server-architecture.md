# Meadowlark Server Architecture

The Meadowlark server owns a local Warren and makes its read and write
operations available through `ClientWarren`. A single loopback TCP listener
serves both browser inspection and the persistent ClientWarren protocol.
The wire format and version 0 operation set are defined in the companion
specification. [Server protocol](server-protocol.md)

## Activation and discovery

The `server` block in a burrow's `dna` selects server activation. Its
`active` field is interpreted by `okay()`, and `command` gives the command
used to start the server. A configuration change takes the burrow lock
before updating `dna`. With server activation disabled, `Warren::make`
opens the local Warren in the usual way.

For server activation, `Warren::make(burrow)` reads the configuration and
looks for the endpoint in `meadowlark.url`. It uses the ClientWarren
greeting to confirm the connection. When the burrow is already locked and
the endpoint is absent or stale, it retries discovery briefly so a
starting server has time to publish its URL. If a server needs to be
started, the client releases any discovery lock it holds, launches the
configured command as a detached process, and waits for a usable session.
Connection and startup waits are bounded. Opening by endpoint connects
directly to a running server. An operator can start a server explicitly
with `meadowlark --meadow BURROW --server`.

The server takes the existing burrow lock before publishing an endpoint.
Concurrent launches can race to acquire it. The winner keeps the lock for
the lifetime of the server, and candidates that find the lock held exit
quietly. `burrow.pid` provides diagnostics. The OS lock on `burrow.lock`
determines ownership.

The server opens the listener and atomically publishes its loopback URL
before activating the Warren, which can take much longer. It activates
locally through `Warren::local(wlock)`, bypassing server discovery and
launch. A published URL identifies the listener. A completed greeting
establishes a usable Warren session. The server appends stdout and stderr
to `meadowlark.log`, beginning each run with a startup timestamp.

## Sessions and concurrency

The first byte selects the protocol for the life of the socket. The
listener handles an HTTP inspection request inline and closes that
connection. A leading `0x00` selects the ClientWarren protocol. The
server hands the persistent connection to a session thread. Different
sessions can run concurrently, while requests within one session run in
order.

Each session clones the server's original, unstarted Warren through the
Warren interface. The client's `start()` takes a read snapshot and
`end()` releases it. `ClientWarren::clone()` opens another connection
with its own unstarted Warren. Write transactions belong to the session
and use the underlying Warren's `transaction`, `ready`, `commit`, and
`abort` operations.

`ClientWarren` forwards the supported operations and constructs a local
tokenizer from the server's DNA in the greeting. Callers use that tokenizer
to work with translated text locally. Higher-level operations can use the
same connection when the work belongs on the server.

## Connection and hopper ownership

`Connection` owns the socket, frames messages, and records transport
failure. Its mutex covers a full synchronous request and reply. The
one-way hopper completion message uses the same mutex for its send. A
connection can stay idle between calls.

The session owns each server-side hopper through a `unique_ptr` in a
connection-local map. `idx.hopper` and `warren.hopper_from_gcl` create the
hopper on the server. The client wraps the returned handle in a
`ClientHopper`. Destroying the `ClientHopper` sends `hopper.complete` if
the connection is usable. The server erases the handle and destroys the
hopper.

Both `ClientWarren` and `ClientHopper` hold a `shared_ptr<Connection>`.
The last reference closes the socket. A hopper therefore remains usable
after its originating `ClientWarren` is destroyed. The connection mutex
serializes the wire exchanges. Each client hopper, like a local hopper,
is used by one caller at a time.

## Disconnect and recovery

When a client disconnects, the session aborts any outstanding transaction,
releases its hoppers and read snapshot, and exits its thread. The server
continues listening. A completed commit stands even if its reply was lost.

Once the client detects a broken socket, the shared connection stays
failed. Calls with an error channel report the lost server. Translation
returns an empty string, and hopper seeks return their empty-hopper
sentinels. A subsequent activation can establish a new session through
normal discovery. If the server died, the OS releases its lock and a new
server can replace the stale endpoint. The new session starts with its
own Warren and hopper handles.
