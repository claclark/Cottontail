# cgrep: Implementation Checkpoint

Updated 2026-09-29. The scanner is a stable working tool, as far as current
testing and use establish. Matcher development is paused while the main
project returns to a Meadowlark release. This document describes current code;
the preliminary indexed-regexp design and later optimization sequence are in
[regex.md](regex.md). Neither document authorizes new source changes.

## Command and Reporting

    cgrep [--lines n | --raw n] regexp [file...]

- Default: --lines 4. Return the complete lines containing a match when it
  touches at most four lines, not a fixed window of surrounding context.
- --lines n changes that line limit. --raw n instead returns the exact match
  text when it occupies at most n bytes. Zero means unlimited in either mode.
  Both space-separated and --lines=n / --raw=n spellings work.
- Policies replace one another in command order; the last one wins.
- --help prints the summary; -- ends option parsing.
- Named inputs are searched sequentially. With no filenames, search stdin and
  omit the filename member from results. There is no special "-" stdin alias.
- The temporary --springy, --no-springy, and --no-match switches were removed.
  Literal specialization is selected automatically from the compiled NFA.
- Directory recursion, hidden-file policy, parallel searches, decompression,
  and a full flat-file GCL frontend are not implemented.

Output is JSON Lines. Every result includes the exact zero-based inclusive
byte interval p,q. Results use shortest-substring semantics: overlaps are
allowed; containing matches are not.

In line mode, an included report supplies lines and one-based start/end
objects containing line and position. Positions count bytes, not Unicode
characters or display columns. LF belongs to the line it terminates; an
unterminated final line is also reported. Oversized matches retain p,q but
omit presentation text and line coordinates. Raw mode names its text match.

For example:

    {"end":{"line":12,"position":35},"file":"src/foo.cc","lines":"...","p":120,"q":137,"start":{"line":11,"position":16}}

Ordinary JSON serialization handles valid UTF-8 and control-byte escaping.
If the selected report text is not valid UTF-8, omit that text and include
"binary":true, retaining any available coordinates. This is per report, not
a permanent switch for the source. No validation is needed for text already
omitted by the output limit. A malformed filename uses file_base64 instead of
file; match_base64 and lines_base64 are no longer emitted. Embedded NUL in
otherwise valid result text is JSON-escaped, not classified as invalid UTF-8.

Exit status is 0 for at least one match, 1 for no match, and 2 for an error.
An input failure is reported and remaining named inputs are still attempted.
Compilation failure stops before input loading. Output errors are failures.

## NFA and Shared Compilation

regexp/nfa.h exports the source-independent transition vector and a reference
matcher. See regex.md for syntax and shortest-substring semantics. Cgrep::compile
accepts either regexp text or an existing transition vector, validates it, and
returns a std::shared_ptr<const Cgrep::Machine>. Cgrep::Machine is an alias for
the explicit regexp::Machine in regexp/machine.h.

Machine owns the transitions and lazily caches typed immutable representations:

- HaystackMachine contains a flattened state-by-symbol dispatch table and
  contiguous destination lists.
- BufferMachine is either PHRASE (literal bytes and a self-overlap step) or
  STANDARD (a shared HaystackMachine).
- The buffer cache is a vector, currently populated with exactly one entry.
  It was introduced for possible follow decomposition; that decomposition was
  not implemented and is no longer a prerequisite. Leave the vector alone
  while collecting workloads.

A mutex protects cache lookup and first compilation. Each representation is
compiled once and shared by independent runners, which fetch it during setup
and do not lock per byte. Mutable matching state belongs to the runner.
Type-erased caches and LineCgrep::Impl were removed; there is no
cgrep_internal.h.

Dispatch has 258 columns: byte values 0--255 plus START and END. Each Cell
contains begin,end indices into the destinations vector. The table is indexed
by state * 258 + symbol. Dot and classes have already been expanded into
symbol sets; the ordinary runner has no special dot handling.

## Source Selection and Ownership

Cgrep supplies the public matching interface and source factories; concrete
runners live in haystack_cgrep.* and buffer_cgrep.*.

- An explicit Haystack or std::istream uses HaystackCgrep.
- A shared immutable byte buffer is offered to BufferCgrep. An unowned
  pointer/length is copied into owned shared storage first.
- Regular files up to 64 MiB are sized, allocated, and read into a buffer.
  This threshold is a provisional source-selection policy.
- Larger regular files use a file Haystack. Other filename sources use the
  stream path. Source factories own loading decisions, not the matcher loops.
- Cgrep's base holds the source storage. The buffer must remain immutable for
  the runner's lifetime.

Buffer compilation recognizes a complete singleton-byte linear chain by
inspecting transitions, not regexp spelling. PHRASE selects the literal
runner. STANDARD delegates to HaystackCgrep through a zero-copy buffer
Haystack retaining the same storage.

This is not yet bounded-memory streaming: the supplied concrete Haystacks
materialize the whole input. File Haystacks size and read into an uninitialized
allocation; unsized streams grow a string with 64 KiB reads. They publish one
whole-input chunk. The buffer adapter is a replayable zero-copy view.
The 64 MiB selection threshold therefore does not cap memory use.

## Matching Engines

### Buffer Literal Matching

BufferCgrep seeks successive literal bytes with memchr, then tightens backward
with memrchr on glibc (a reverse loop elsewhere). A candidate is exact when
its width equals the literal length. That test is sufficient for a fixed
string; it is not a general regexp verifier.

A failed candidate resumes at p + 1. A successful candidate resumes at p plus
the smallest precomputed self-overlap shift (aaa: 1, aba: 2, abc: 3).
Matches may overlap. This runner uses a complete retained buffer, not a
chunk-refilling springy engine.

### General Bytewise Matching

HaystackCgrep maintains current/next active-state vectors and latest-start
arrays. At each byte it introduces a new start and advances existing states
through the dispatch table. Collisions at the same state and input position
keep the greatest start address.

Acceptance selects the latest start for that endpoint, removes containing
results, and prunes paths starting at or before the reported start. Later
starts survive for overlapping matches. START is consumed at virtual position
-1, END at input length; their positions are stripped from returned intervals.
Anchors refer to the complete input, not a line or chunk.

No general springy-NFA runner is implemented.

### Line Matching

LineCgrep has two concrete implementations with the same reporting contract:

- BufferLineCgrep wraps a buffer literal runner. It obtains the raw match,
  then advances two cached LF-based line endpoints with memchr to find its
  enclosing lines and coordinates. The complete buffer remains available.
- HaystackLineCgrep has its own bytewise matching loop, sharing the immutable
  dispatch representation. It queues accepted matches and flushes reports
  on LF or EOF, maintaining line positions and coordinated retention.
  Streams and nonliteral buffer fallbacks use this engine.

Line reporting does not change which intervals match. Keeping concrete hot
loops separate is deliberate; it avoids reporting branches in the raw loop.

## Haystack Contract

The interface in regexp/haystack.h remains useful even though current concrete
sources materialize their inputs.

- chunk(&start,&end) returns a nonempty half-open byte range. False means no
  more input; success(error) distinguishes clean EOF from a sticky error.
- Chunk boundaries have no matching semantics. Test Haystacks exercise split,
  relocated, and trimmed chunks.
- Unreclaimed history is contiguous behind a newly published chunk. Pointers
  must be reacquired or rebased after chunk publication; logical positions
  remain absolute offsets.
- translate(p,q) returns an owning std::string for an inclusive interval.
  The pointer overload returns a half-open, short-lived view. Do not retain
  views across subsequent matching, translation, reset, or source relocation.
- limit(x) declares that no future translation will begin at or before x.
  Raw Haystack matching advances it after accepted matches, on active-to-empty
  transitions, and at inactive chunk boundaries. Line matching also retains
  required line prefixes and queued reports. Repeated watermarks are suppressed.
- FullHaystack currently records only a watermark; BufferHaystack ignores it.
  Neither reclaims stored bytes.
- reset(error) restarts replayable sources. A pristine one-shot stream can
  reset harmlessly; a consumed one-shot stream cannot. Reset does not recompile
  the matching machine.

Runner and source error strings are optional and written on failure through
safe_error. EOF is not represented by clearing a caller's error string.
Individual runners are not thread-safe; independent runners share immutable
machines safely.

## Agent Search Collection

CGREP_LOG is opt-in developer instrumentation, not a user-facing feature.
It need not appear in README or command help. With the variable unset, cgrep
does not log.

When set, startup appends argv[1..] to the named file before option parsing or
regexp compilation, including help and invalid invocations. Each argument is
POSIX-shell single-quoted, with embedded quotes escaped; paste the record after
cgrep to replay it. Literal newlines remain inside quoted arguments and may
span physical log lines. Log failures are silent and do not affect searches.

The log contains expanded arguments, not the original shell command: globs
have already expanded; working directory, redirections, and pipe input are
not recorded. Replay relative paths from the appropriate directory and supply
stdin separately. Treat collected commands as data, not as trusted scripts.
Do not automatically commit the local log; inspect git status before staging.
The repository-root `cgrep.log` is now ignored and no longer tracked. A
different log path is not automatically covered by that ignore rule.

Agents should continue using cgrep for suitable repository searches. Record
reasons for other tools in cgrep-improvements.md. Collection is intended to
identify useful workloads and missed cases, not to justify premature
optimization. The app test unsets inherited CGREP_LOG so regression searches
do not pollute the developer collection.

## Verification and Deferred Work

Focused targets, separate from the large aggregate test:

- //test:nfa_test: construction, syntax, shortest overlapping intervals,
  anchors, intersection, nullable subexpressions, arbitrary bytes, and UTF-8.
- //test:cgrep_test: reference agreement, chunk boundaries and retention,
  literals, buffer fallback/ownership, lazy cache sharing, concurrent first
  compilation, reset/errors, and line-engine agreement.
- //test:cgrep_app_test: actual JSONL output, stdin/files, limits and option
  order, invalid UTF-8 handling, exit statuses, and argument logging/replay.

The user considers the current matching code stable. The latest CGREP_LOG
change was compile-checked with the app and app-test targets using Makefile's
fast flags; shell syntax and diff checks passed. Its runtime cases were not
run by the agent. Do not read an old build/test count as verification of a
new change. User authorization governs runtime tests and experiments.

Near-term: keep dogfooding and collect searches while shipping Meadowlark.
On return, implement indexed regexp matching before adding workload-driven
follow (... r0 r1) and union (+ r0 r1) special cases to either engine.
The frontier proposal, counterexamples, and open correctness questions are in
regex.md. There is no pending authorization to implement follow decomposition,
remove machine-vector storage, or optimize dot paths.

Recursion, concurrent input/matching, mapped or bounded-buffer sources, and
a flat-file GCL frontend remain possible later work, not release prerequisites.
