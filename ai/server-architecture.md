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

`meadowlark --meadow BURROW --config` replaces the entire `server` block
with an enabled command for the invoking executable and selected meadow.
Both paths are absolute and retain their symlinks. A bare invocation
name is resolved through `PATH`. The library `config` function uses
`DEFAULT_MEADOW` for an empty meadow and searches `PATH` for `meadowlark`
when the executable is empty. It verifies the replacement DNA before
publishing it.

For server activation, `Warren::make(burrow)` clears any stale
`meadowlark.url` while it holds the burrow lock, releases the lock, and
launches the configured command as a detached process. If another process
holds the lock, the factory proceeds to discovery. Both paths call
`RemoteWarren::make(burrow)`, which reads the loopback endpoint from
`meadowlark.url` and retries once after one second if it cannot read a
valid endpoint. The client connects, sends its greeting, and waits for
the server's reply. Opening by port connects directly to the listener.
An operator can start the server with `meadowlark --meadow BURROW --server`.

The server takes the existing burrow lock before publishing an endpoint.
Concurrent launches can race to acquire it. The winner keeps the lock for
the lifetime of the server, and candidates that find the lock held exit
quietly. `burrow.pid` provides diagnostics. The OS lock on `burrow.lock`
determines ownership.

The server opens the listener and atomically publishes its loopback URL
before activating the Warren, which can take much longer. It activates
locally through `Warren::local(wlock)`, bypassing server discovery and
launch. RPC session threads wait for activation to finish before replying
to the greeting. HTTP inspection can show the loading page in the
meantime. The server appends stdout and stderr to `meadowlark.log`,
beginning each run with a startup timestamp.

## Sessions and concurrency

The first byte selects the protocol for the life of the socket. The
listener handles an HTTP inspection request inline and closes that
connection. A leading `0x00` selects the ClientWarren protocol. The
server hands the persistent connection to a session thread. Different
sessions can run concurrently, while requests within one session run in
order.

A fresh session clones the server's original, unstarted Warren. The
client's `start()` takes a read snapshot and `end()` releases it.
`RemoteWarren::clone()` opens another connection and supplies the source
session ID in the greeting. The server uses a table of live sessions to
find that Warren and calls its `clone()` while holding the source session
mutex. The new session has its own connection and inherits the read state
of the local clone. For a started Bigwig, this includes the read snapshot.
Write transactions belong to the session and use the underlying Warren's
`transaction`, `ready`, `commit`, and `abort` operations.

`ClientWarren` constructs a local tokenizer and stemmer from the server's
DNA in the greeting, and reads the legacy default container from the same
configuration. Its featurizer proxy calls the session Warren's featurizer
on the server, so the burrow owns the mapping between labels and feature
addresses. Higher-level operations can use the same connection when the
work belongs on the server.

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

When a client disconnects, the server removes its session ID from the live
table. The session aborts any outstanding transaction, releases its
hoppers and read snapshot, and exits its thread. The server continues
listening. A completed commit stands even if its reply was lost.

Once the client detects a broken socket, the shared connection stays
failed. Calls that need the connection and have an error output report
the lost server. Translation returns an empty string. Remote hopper
seeks that need the connection return empty-hopper sentinels. Cached seek
results, cached `ready` votes, and client-local hoppers remain usable.
A subsequent activation can establish a new session through normal
discovery. If the server died, the OS releases its lock and a new server
can replace the stale endpoint. The new session starts with its own
Warren and hopper handles.

## Shutdown

The CLI exposes shutdown as `meadowlark --meadow BURROW --shutdown`.

`meadowlark::shutdown(burrow)` succeeds when it can acquire and release
the burrow lock. If the lock is held, it reads the host and PID from
`burrow.pid` and verifies the local process arguments against the
configured `server.command`. On Linux it uses a PID file descriptor to
send `SIGTERM` to that process. It then tries the lock immediately and
once a second for up to ten seconds. A verification failure or a lock
that remains held produces an error.
Legacy Simple burrows lack the lock guarantee used by shutdown.
