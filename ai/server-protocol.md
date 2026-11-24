# Meadowlark Server Protocol

Version 0 of the Meadowlark protocol carries Warren calls between a
`ClientWarren` and its server session. The companion architecture document
covers startup, ownership, and concurrency. [Server architecture](server-architecture.md)

## Connection and greeting

ClientWarren connects to the burrow's `meadowlark.sock` Unix domain socket
and sends a single `0x00` byte to begin the protocol.

After the initial byte, each request or reply is a single-line UTF-8 JSON
object terminated by a literal LF byte (`0x0a`). A newline inside a string
is escaped as `\n`. The receiver buffers through LF even when a socket
read splits a message or contains several messages. A connection can carry
many exchanges and can stay idle between requests.

The client sends the first message. Both greetings include `protocol` and
`version`. `protocol` is the string `meadowlark`. `version` is a
nonnegative JSON integer giving the highest supported version. Each peer
supports versions from 0 through the one it advertises. Both use the lower
advertised version for the rest of the connection. This is the client
greeting for version 0.

```json
{"protocol":"meadowlark","version":0}
```

The server greeting includes `dna`, a JSON string containing the burrow's
DNA, `session`, an opaque identifier for this connection's Warren, and
`started`, a Boolean giving its read state. The Warren factory reads local
`dna` to decide whether to activate a server. `RemoteWarren` gets its
configuration from the greeting. The server sends the reply once the
session Warren is ready.

To clone a Warren, the client opens a new connection and adds `clone_of`
to its greeting, with the source Warren's `session` identifier as the
value. An unknown or closed source returns an error and closes the new
connection. Each successful clone gets its own session ID and uses the
`started` value from the cloned Warren.

An invalid greeting ends negotiation and closes the connection.

## Requests and replies

Requests carry `component`, `method`, and any named arguments. For example,
this request translates the inclusive address interval `[100, 200]`.

```json
{"component":"txt","method":"translate","p":"100","q":"200"}
```

```json
{"response":"this is some text..."}
```

Ordinary success replies carry `response`. Methods that return a hopper
carry `hopper` instead. A failed request returns `{"error":"..."}`. The
server sends one reply for each request except `hopper.complete`, which is
one-way. A successful call with no result uses `{"response":true}`.
For a request that expects a reply, an unknown method, invalid argument,
or invalid operation for the session state returns an error. A frame that
cannot be decoded as a JSON object closes the connection.

The server processes requests in order on each connection. The client
holds the `Connection` mutex across each send and its reply, or across the
send alone for `hopper.complete`. This gives both sides an unambiguous
reply order without request IDs.

The protocol uses a small JSON codec for its fixed message shapes. Object
field order is immaterial. Text uses ordinary JSON escaping
and must be valid UTF-8. If `txt.translate` returns bytes that cannot be
represented as UTF-8 text, the server returns an error.

## Warren operations

A greeting without `clone_of` clones the server's original, unstarted
Warren. A greeting with `clone_of` calls `Warren::clone()` on the source
session's Warren, serialized with that session's requests and cleanup.
The Warren implementation determines the inherited state. A started
Bigwig clone inherits the source read snapshot.

The client constructs a local `Tokenizer` from the greeting DNA to process
text returned by `txt.translate`. It also initializes the legacy default
container and local stemmer from the `container` and `stemmer` parameters.
The featurizer proxy uses the session Warren's server-side featurizer.
Its methods can be called before `warren.start`. The labels use the same
UTF-8 string encoding as other text.

The client reads Warren parameters from the `parameters` block in the
greeting DNA. This is a read-only copy of the server configuration.

`warren.start` establishes the session's read snapshot, `warren.end`
ends it, and `warren.started` reports whether it is active. The client
uses `start` before issuing index, text, or GCL queries. A subsequent
`start` after `end` takes a fresh read snapshot.

`transaction`, `ready`, `commit`, and `abort` refer to the Warren's write
transaction. `append`, `annotate`, and `erase` require an open transaction
before `ready`. `ready` prepares the transaction, `commit` requires a
successful `ready`, and `abort` discards an open transaction. The server
checks these preconditions before calling into Warren.

| Component | Method | Arguments | Success result |
| --- | --- | --- | --- |
| `warren` | `start`, `end` | | `true` |
| `warren` | `started` | | Boolean |
| `warren` | `transaction`, `ready`, `commit`, `abort` | | `true` |
| `warren` | `hopper_from_gcl` | `query` | Hopper handle |
| `featurizer` | `name`, `recipe` | | String |
| `featurizer` | `featurize` | `label` | Feature as an `addr` |
| `featurizer` | `translate` | `feature` | String |
| `idx` | `hopper` | `feature` | Hopper handle |
| `idx` | `count` | `feature` | Count as an `addr` |
| `idx` | `vocab` | | Vocabulary size as an `addr` |
| `txt` | `translate` | `p`, `q` | String |
| `txt` | `tokens` | | Token count as an `addr` |
| `txt` | `range` | | Object with `p` and `q`, or `null` if there is no range |
| `appender` | `append` | `text` | Object with `p` and `q` |
| `annotator` | `annotate` | `feature`, `p`, `q`, optional `v` | `true` |
| `annotator` | `erase` | `p`, `q` | `true` |
| `hopper` | `tau`, `rho`, `uat`, `ohr` | `hopper`, `k` | Object with `p`, `q`, and optional `v` |
| `hopper` | `L`, `R` | `hopper`, `k` | An `addr` |
| `hopper` | `complete` | `hopper` | One-way |

An append returns the addresses reported by the appender, including `q < p`
when it accepts text that contributes no tokens. Erasure is a method on
`annotator`, as in the local API. Stemmer, default-container and parameter
methods, `trim_memory`, and `clone` are outside the request set.

## Addresses and annotation values

Addresses, feature IDs, counts of type `addr`, and the hopper argument `k`
are signed 64-bit decimal strings. This preserves the full range of `addr`,
including `minfinity` (`"-9223372036854775808"`) and `maxfinity`
(`"9223372036854775807"`).

Annotation values are `fval` internally. On the wire, `v` is a signed
decimal `addr` string representing the IEEE 754 binary64 bit pattern of the
`fval`. Use `fval2addr` to encode it and `addr2fval` when the receiving
caller needs the `fval`. This preserves packed integer values and works
across byte orders. An omitted `v` means zero bits. Omit it only for that
bit pattern, so a floating-point negative zero retains its encoding.

## Hoppers

`idx.hopper` and `warren.hopper_from_gcl` return a connection-local handle.
The server keeps the resulting hopper until the client releases it or the
connection closes. Handles are opaque strings and stay unique for the
life of the connection.

```json
{"hopper":"1"}
```

The client uses that handle for `tau`, `rho`, `uat`, and `ohr`. Each seek
takes `k` and returns an interval with an optional `v`. This example
returns an interval with value zero.

```json
{"component":"hopper","method":"tau","hopper":"1","k":"100"}
```

```json
{"response":{"p":"105","q":"107"}}
```

`L` and `R` each take `k` and return a single address. They call the server
hopper's own `L` and `R` methods to preserve the behavior of composed
GCL hoppers.

A seek on an unknown or released handle follows `EmptyHopper` semantics.
The table gives the returned address for `L` and `R`, or the value of both
`p` and `q` for an interval seek. The reply omits `v`.

| Seek | With an ordinary `k` | At the starting sentinel |
| --- | --- | --- |
| `tau`, `rho`, `R` | `maxfinity` | `minfinity` when `k` is `minfinity` |
| `uat`, `ohr`, `L` | `minfinity` | `maxfinity` when `k` is `maxfinity` |

`complete` releases the handle. An unknown or already released handle is
harmless. Neither case sends a reply.

```json
{"component":"hopper","method":"complete","hopper":"1"}
```

`ClientWarren` and each `ClientHopper` share ownership of the `Connection`.
A hopper can keep using the connection after its originating ClientWarren
is destroyed. The last reference closes the socket.

## Connection failure

If the connection drops, the shared `Connection` enters a permanent failed
state. Calls that need the connection and have an error output report the
lost server. `txt.translate` returns an empty string. Hopper seeks that
need the connection return the `EmptyHopper` results above. Cached seek
results, cached `ready` votes, and client-local hoppers can still be used.
A failed featurizer call produces `null_feature` for `featurize` or an
empty string for the string-valued methods.
If the connection drops before a `commit` reply arrives, the transaction
may already have committed.

On disconnect, the server aborts any outstanding transaction, releases the
remaining hoppers, ends any active read snapshot, and drops the session
Warren. The connection's server thread then exits.
