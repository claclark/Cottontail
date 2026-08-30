# Preliminary cgrep Workplan

This document is a preliminary implementation plan for flat-input regular-
expression search. It records the current design discussion; it does not
authorize source changes beyond separately approved steps.

The first objective is a small, usable `apps/cgrep` that exercises the new
shortest-substring NFA over files and standard input. The implementation should
establish clean interfaces for fast matching and replaceable input strategies
before adding command-line features.

## Scope of the First Cut

The first command line is deliberately narrow:

    cgrep regexp [file...]

There are no flags. The first argument is always the regexp, even when it
begins with `-`. With no files, cgrep searches standard input. Every supplied
file is searched independently and its name is included in every result.
Standard-input results have no filename prefix.

The regexp is compiled with `regexp/nfa.h`. Consequently the initial syntax,
errors, buffer anchors, byte semantics, intersection, and shortest-substring
semantics are exactly those documented in `ai/regex.md`.

Matches are reported in discovery order as zero-based, inclusive byte
intervals. They may overlap but may not contain another accepted match. Output
is JSON Lines, with one complete object per match. A named-input result normally
has this form:

    {"file":"src/foo.cc","p":120,"q":137,"match":"first\nsecond"}

The `file` member is omitted for standard input. The existing
`nlohmann::json` serializer should be given the filename and match as ordinary
`std::string` values first; its normal `dump()` path supplies JSON quoting and
escaping. Do not pre-encode valid UTF-8 or introduce a separate JSON-string
renderer.

If strict serialization rejects malformed UTF-8 in either byte string, retry
with only that member represented losslessly as `file_base64` or
`match_base64`. Exactly one of the plain and Base64 forms is present for each
value. Base64 is a fallback for bytes that standard JSON cannot carry, not the
normal output representation.

Each object occupies one physical output line even when its match contains
newlines or NULs. This makes records self-delimiting and suitable for immediate
streaming, later parallel production, or machine consumption without a second
human-oriented presentation convention.

The first cut has no headings, line numbers, context, highlighting, filename
filtering, recursive traversal, encoding conversion, decompression options, or
alternate output modes. A supplied filename is literal; the conventional `-`
alias for standard input is deferred rather than hidden inside the no-flags
interface.

Use conventional grep exit status:

- `0` if at least one match was reported and no error occurred;
- `1` if the search completed without a match; and
- `2` for a regexp, input, matching, translation, or output error.

When several files are supplied, an input error should be reported to standard
error and the remaining files should still be attempted. Any such error makes
the final status `2`. A malformed command line prints the usage and returns
`2`.

## Layering

The implementation has three independent layers:

    regexp text
        -> public lambda-free transition-vector NFA
        -> immutable cgrep dispatch machine
        -> mutable Cgrep runner over a Haystack

`regexp/nfa.h` remains the public, syntax-independent machine description. It
does not acquire flat-file buffering or optimized-runner state. The existing
`regexp::match(...)` remains the small reference implementation and executable
specification.

The new library classes should live with the regexp implementation, initially
as `regexp/haystack.h`, `regexp/haystack.cc`, `regexp/cgrep.h`, and
`regexp/cgrep.cc`. The command-line main belongs in `apps/cgrep.cc` and is a
thin client of those classes.

## Haystack

`Haystack` is the byte source searched by cgrep. The name deliberately avoids
committing the interface to a file, memory map, rolling buffer, pipe, or
process.

The initial public shape is:

    class Haystack {
    public:
      static std::shared_ptr<Haystack> make(const std::string &filename,
                                            std::string *error = nullptr);
      static std::shared_ptr<Haystack>
      make_stdin(std::string *error = nullptr);

      bool chunk(const char **start, const char **end);
      std::string translate(addr p, addr q);
      bool translate(addr p, addr q,
                     const char **start, const char **end);
      void limit(addr x);

      bool reset(std::string *error = nullptr);
      bool success(std::string *error = nullptr);
    };

Factories return an object ready for its first pass. They report construction
errors only through `safe_error(error)` and never clear or otherwise write the
caller's optional error string on success.

### Chunks and Pointers

`chunk(&start, &end)` returns the next nonempty half-open byte range
`[start,end)`. It returns `false` on either clean EOF or a sticky internal
failure; `success(error)` distinguishes the two after the loop. On clean EOF,
`success` returns true without touching `error`. On failure it returns false
and copies the stored diagnostic through `safe_error(error)`.

Pointers from `chunk` remain valid until the next call to `chunk`, `reset`, or
destruction. `limit` may advance reclamation in the background but must not
invalidate the currently handed-out chunk. Any compaction or buffer exchange
becomes visible through the next pair of chunk pointers.

Chunks are not matching boundaries. The Cgrep state continues unchanged from
one chunk to the next. A buffered implementation should normally publish
chunks ending immediately after a real newline and may include many complete
lines in one chunk. The final chunk may end anywhere, preserving files without
a trailing newline. A whole mapped or owned file is also a valid single chunk.

Within a chunk, the runner touches no Haystack operation: it advances a raw
pointer until `current == end`. The Haystack maintains the relationship
between retained bytes and absolute offsets; the runner maintains its current
absolute byte offset separately.

### Translation and Reclamation

The two `translate` overloads expose the inclusive absolute byte interval
`[p,q]`. The string form returns an owning copy and supports embedded NULs. The
pointer form returns a non-owning half-open range `[start,end)`. C++ cannot
overload on return type alone, so the pointer output parameters distinguish the
dangerous form.

A pointer translation may refer directly to retained or mapped input. If that
interval is not contiguous, the Haystack may materialize it in internal scratch
storage. These pointers remain valid only until the next `chunk`, `limit`,
pointer-returning `translate`, `reset`, or destruction. The owning string is
independent of all subsequent Haystack activity.

Calling either form outside the retained range is a programming error that the
implementation should detect defensively and store as a sticky failure.

`limit(x)` is a monotonic declaration that no future translation will begin at
or before absolute offset `x`. A buffered implementation may reclaim those
bytes once doing so cannot invalidate the current chunk. A full-buffer or
mapped implementation may treat it as a no-op.

After returning an accepted match beginning at `p`, the runner retains the
bytes until the caller has had an opportunity to request its text or view. At
the beginning of the next `match()` call, it may call `limit(p)` after the NFA
has discarded all paths beginning at or before `p`. Whenever the active-state
set becomes empty without a pending match, it may advance the limit through
the current offset.

### Reset

`reset(error)` returns the Haystack to absolute offset zero, clears its sticky
internal status and reclamation watermark, and invalidates outstanding
pointers. It writes the caller's optional error string only on failure and
then only through `safe_error(error)`.

A newly constructed, untouched Haystack is pristine. Resetting it is a no-op
that returns true, including for a one-shot pipe. A replayable source that has
been touched rewinds normally. A non-replayable source that has been touched
returns false. The touched state begins with the first `chunk` attempt, even
when that attempt immediately reaches EOF.

The first implementation may own the complete file or standard-input contents
in a string and return them as one chunk. This is enough to validate the
interface and get a useful command. Later implementations can add memory
mapping and bounded or concurrent buffering without changing the matcher.

## Cgrep

`Cgrep` is a stateful iterator over matches. It owns a Haystack, an immutable
compiled dispatch machine, and mutable runner state. A preliminary interface
is:

    class Cgrep {
    public:
      static std::shared_ptr<Cgrep>
      make(const std::string &regexp,
           std::shared_ptr<Haystack> haystack,
           std::string *error = nullptr);
      static std::shared_ptr<Cgrep>
      make(const std::vector<regexp::transition> &nfa,
           std::shared_ptr<Haystack> haystack,
           std::string *error = nullptr);

      bool match(addr *p, addr *q);
      std::string translate(addr p, addr q);
      bool translate(addr p, addr q,
                     const char **start, const char **end);

      bool reset(std::string *error = nullptr);
      bool success(std::string *error = nullptr);
    };

The exact overload set may be simplified during implementation, but regexp
parsing must remain separate from dispatch-machine construction. The second
form is useful for tests and for callers that already hold a validated public
NFA.

`match(&p,&q)` returns the next shortest match as a zero-based inclusive byte
interval. It returns false at EOF or after a sticky error; `success(error)`
distinguishes the two using the same convention as Haystack. A successful
`reset` resets the Haystack and mutable runner but does not rebuild the
immutable dispatch machine.

The `translate` overloads delegate to the Haystack, so a caller can remain
entirely within the Cgrep interface and may request either the reported match
or retained surrounding context. The owning form returns an independent
string. The pointer form returns a half-open range and is invalidated by the
next `match`, pointer-returning `translate`, `reset`, or destruction. Passing an
interval that is no longer retained stores a sticky error.

The class is not initially thread-safe. Separate runners may be used in
separate threads. The immutable machine is deliberately shareable in design,
although exposing machine reuse between Cgrep instances is not required for
the first API.

## Immutable Dispatch Machine

The public transition NFA is compiled once per Cgrep construction. The
internal machine is immutable and contains a direct state-by-symbol dispatch
table. There are 258 input symbols: bytes `0` through `255`, followed by the
public `START` and `END` symbols.

Avoid one heap allocation per table cell. Flatten destination lists:

    struct cell {
      std::size_t begin;
      std::size_t end;
    };

    std::vector<cell> dispatch;       // state_count * 258
    std::vector<state> destinations;  // all cell contents, contiguous

For `(state,symbol)`, the corresponding cell identifies a contiguous range of
destination states. The row already supplies the source state and the column
supplies the symbol, so the original transition object is unnecessary during
execution. `final_state` may remain a destination sentinel or be represented
by equivalent per-cell final metadata, whichever keeps the inner loop
clearest.

All public symbol sets are expanded while constructing this table. A full
256-byte label is sigma without requiring an explicit sigma symbol. Dot and
complemented classes have no runtime special case. `START` and `END` occupy
their own columns and cannot be accepted by an ordinary-byte class.

The compiler must validate source states, destination states, and symbol
bounds defensively. Public NFAs are compactly numbered already, so a dense row
index is appropriate.

## Fast Runner

For each active NFA state, the runner records the greatest start offset of a
path reaching that state. It uses preallocated arrays indexed by compact state
and current/next active-state vectors rather than hash maps:

    starts[state]
    next_starts[state]
    active_states
    next_active_states

An unset sentinel or generation counter records whether a state has already
been placed in the next active vector. The first path reaching a destination
adds it; later paths update only:

    next_starts[to] = max(next_starts[to], start)

That maximum is the shortest-substring collision rule. No backtracking,
capture state, or competing path histories are retained.

For each input symbol:

1. Use the start-state dispatch cell to introduce a match beginning at the
   current offset.
2. For every currently active state, visit only the contiguous destinations
   in its dispatch cell for this symbol.
3. Record the greatest start reaching each ordinary destination.
4. Record the greatest start reaching `final_state` as the accepted candidate.
5. Swap the current and next active buffers. A state with no transition on the
   symbol is absent from `next` and disappears automatically.

The runner consumes virtual `START` at internal offset `-1` before the first
input byte and virtual `END` at the absolute size after clean EOF. These events
refer to the complete Haystack pass, never to chunk or line boundaries. Before
returning, their virtual positions are removed from the inclusive byte
interval.

When a candidate `[p,q]` is accepted, paths starting at or before `p` cannot
produce a noncontaining future result and are removed. Paths beginning after
`p` remain active, preserving overlapping matches. The runner saves `[p,q]`
and returns immediately. The next `match` resumes after the already-consumed
symbol with the retained active states.

The hot loop should contain only raw-pointer advancement, direct dispatch
indexing, contiguous destination iteration, and integer comparisons. Chunk
acquisition, translation, reclamation, output, and error formatting remain
outside it.

## First Haystack Implementations and Later Optimization

Start with the most ordinary correct implementation: own the complete input
in memory, expose it as one chunk, copy translations, and make `limit` a no-op.
This supports files and stdin and makes Cgrep immediately useful for its own
development.

After the command is usable and differential tests pass, add input strategies
behind the unchanged Haystack interface:

1. A mapped-file implementation for ordinary files. POSIX uses `mmap`; Windows
   uses its file-mapping API. Unsupported cases fall back to buffered input.
2. A bounded rolling implementation that publishes newline-aligned chunks,
   retains bytes needed for future translation, and reclaims through `limit`.
3. A process/decoder implementation for compressed files and other generated
   streams.
4. If measurements justify it, a producer thread that fills line-aligned
   chunks while the matching thread consumes raw pointers. `chunk` becomes the
   gate: it waits only when the matcher reaches the published frontier.

Memory mapping and producer buffering are complementary. Mapping is the direct
path for an ordinary file; producer buffering overlaps reading or decompression
with matching for streams that cannot be mapped.

Do not introduce heuristics, platform abstraction, or concurrency before the
single-buffer implementation and fast runner are working. Dogfood the command
first, then measure actual file-opening, reading, matching, translation, and
output costs independently.

## Application Mainline

`apps/cgrep.cc` should contain only command-line and output policy:

1. Require at least the regexp argument; otherwise print the exact usage.
2. Compile and validate the regexp before opening inputs.
3. With no filenames, create a standard-input Haystack and search it once.
4. Otherwise, for each filename in order, create its Haystack and Cgrep,
   enumerate matches, and write one JSON object per result.
5. After each enumeration loop, call `success(error)` and report any stored
   input or matching error with the filename when applicable.
6. Detect output failure and return status `2`.
7. Return `0`, `1`, or `2` according to the aggregate outcome above.

The app obtains match bytes with a length-aware operation, never treating them
as a NUL-terminated C string. It may use the pointer-returning
`translate(p,q,...)` to construct the ordinary `std::string` given to the JSON
serializer, and fall back to the owning `translate(p,q)` when necessary. It
first serializes a normal record with `nlohmann::json::dump()`. Only if strict
serialization rejects a filename or match does it Base64-encode the rejected
member and emit the corresponding fallback field. A completed JSON line is
written as one record; output failure returns status `2`.

Compiling the immutable dispatch table separately for every input is acceptable
for the first cut because machines are small. If startup measurements make it
visible, expose an opaque shared compiled-machine handle without changing the
public transition NFA or mutable runner semantics.

## Focused Verification

Add a separate small `//test:cgrep_test` C++ target rather than extending the
two-minute aggregate test binary. It should cover:

- exact agreement between Cgrep and reference `regexp::match` for the same NFA
  and byte string;
- shortest alternatives, intersections, overlaps, and active-state collision;
- `START` and `END` over the complete Haystack rather than each chunk;
- matches spanning every possible chunk boundary, including several lines;
- LF, CRLF, `\R`, U+2028, U+2029, UTF-8 literals, NULs, and an unterminated
  final line;
- empty input and patterns that produce no byte interval;
- both `translate` overloads, `limit`, dangerous-pointer lifetime, owning-copy
  independence, and defensive range errors;
- pristine reset, replayable reset, and failure to reset a touched one-shot
  source;
- clean EOF versus sticky read failure and optional-error handling; and
- exact first-cut JSONL records for named input, stdin, multiline matches,
  embedded NULs, no matches, and malformed regexps;
- ordinary JSON serialization of valid non-ASCII text, plus lossless Base64
  fallback only for malformed UTF-8 in a filename or match.

Also add a small Bazel `sh_test` for the actual `apps/cgrep` binary. It should
run the binary from Bazel runfiles against fixed fixtures, pipe a fixture into
standard input, exercise named and multiple files, and check statuses `0`, `1`,
and `2`. Compare stdout and stderr against golden JSONL files with a tool such
as `cmp`, and parse the emitted lines as an additional validity check. Include
escaped multiline and NUL-containing matches and a malformed-UTF-8 fixture for
the Base64 fallback. This is an end-to-end command regression, not a substitute
for the focused C++ runner tests.

A deterministic differential test should divide the same input into many
chunk layouts and require identical intervals from every layout. Small random
regular expressions and byte strings can later compare the fast runner against
the reference matcher, but this need not block the first usable command.

Verification during implementation remains focused: build all targets and run
only the dedicated cgrep/NFA tests as explicitly agreed. The user runs the
broader regression suite.
