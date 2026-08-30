# Current Plan

## Working Rule

No coding without a concrete reason, discussion, and explicit confirmation
from the user. Documentation review or approval of one narrow change does not
authorize adjacent source changes.

Work on one agreed step at a time. Approval for one step does not authorize
starting a later step. Entries in `ai/improvements.md` are possibilities, not
active work, until the user promotes one into this plan.

If an agreed change appears to require a broader abstraction or materially
different design, stop and discuss it. Proposals for cleanup and refactoring
are welcome, but they also require discussion before implementation.

For the current Meadowlark push, the user makes all commits and runs the
regression tests. Agent verification is compile/build-only unless the user
explicitly requests a runtime experiment or test run.

## Current Checkpoint

The Meadowlark JSONL, TSV, text, and code ingestion work is complete. Durable
database and metadata conventions are recorded in `ai/meadowlark.md`; the
model-facing database bootstrap guide is `ai/exploring-meadowlark.md`.

Restartable parallel Hazel merging and foreground Bigwig consolidation are
complete. `Bigwig::consolidate(...)` implements the offline operation,
`consolidate` delegates to it (`finish-merging` remains a compatibility
symlink), and the user has verified the work through
the full regression suite, large MS MARCO consolidation, repeated ranking, and
interrupted background-merge recovery. The implementation, benchmarks,
recovery behavior, and focused coverage are recorded in
`ai/consolidation.md`.

The follow-up Bigwig merge-publication cleanup is committed as `63d70b8`. The
user reports that `apps/trec-example`, the regression tests, and
`./rank.sh a.meadow` all pass after that change.

The first narrow memory-pressure response is implemented for standalone
Hazels. `Warren::trim_memory()` is a public operation with a default no-op;
Hazel overrides it to clear the shared decoded-posting `OwslaCache`. It leaves
the Hazel text cache untouched. Repeated calls and calls through shallow Hazel
clones are semantically harmless. Agent verification was compile-only; the
user has since reported that the complete regression suite passes.

`Optimizer::estimate_memory(...)` now provides a deliberately rough preflight
estimate for a GCL string or parsed expression. It expands phrases,
deduplicates term features, and prices their full posting counts at three
address-sized fields per posting; a parse failure estimates zero. Agent
verification was compile-only; the user has since reported that the complete
regression suite, including the dedicated optimizer target, passes.

`ssr-server` now applies the agreed first-cut Linux admission policy when a new
query arrives. It trims all persistent collection Warrens above two-thirds RAM
occupancy and rejects queries whose combined estimate exceeds one eighth of
physical RAM. The checks fail open off Linux or when `/proc/meminfo` cannot be
read. Its snippet cover cap is now 512 tokens. This work supports the server for
the ClimbMix collection in TREC RAG 2026. Agent verification remained
compile-only; the user reports that the combined changes have been tested in
various ways and are ready to commit.

## Active Direction

The current push is the preliminary indexed string-matching work recorded in
`ai/regex.md`. Steps 1 through 4 are implemented: feature `-1` is virtual
universal position evidence with zero count; the `HashingFeaturizer` boundary
is fixed; append normalization is centralized with space, tab, carriage
return, and newline as separators; and `Tokenizer` now separates counting,
bag-of-words, phrase, and address-aligned split consumers while preserving the
existing tokenizers through defaults. GCL literal feature strings now decode
`|...|` into ordinary terms, quoted forms remain typed syntax for later
semantic expansion, and ordinary terms serialize canonically and safely.
Agent verification was compile-only through `bazel build //...`; user
basic tests pass, and no deeper testing is planned for this narrow parser
change.

Step 5 is now implemented. `NGramFeaturizer` has no recipe or gram-size
knowledge; the shared MurmurHash routine preserves existing hashed values, and
the typed marker protocol covers reversible grams, universal positions, hash
translation, and JSON structural nulls. `NGramTokenizer` indexes literal bytes
with a configurable width from one through seven, defaults to `five`, and
canonicalizes its recipe as a word. Every ordinary byte is a position; every
U+FDD0--U+FDEF reserved noncharacter is one atomic null position; grams stop at
those positions and structural-element ends. Complete grams are shortened to
the literal available suffix at those boundaries, preserving every ordinary
position for later dictionary-backed short matching. `split` remains
address-aligned, `bow` retains complete grams, and, until Warren-level phrase
expansion has dictionary access, `phrase` supplies universal positional tails
or returns empty when no complete-gram evidence exists. The GCL tree's internal
semantic `ERROR` node reports that last case cleanly. All 54 targets compile
successfully, and the user reports that basic testing works. The existing
phrase expander completes step 6 by compiling quoted strings into exact
positional GCL; no additional source change was required for that step. The
Meadowlark file-oriented metadata work remains complete, and the separate
Python wrapper still follows later.

Meadowlark creation now exposes the n-gram configuration. `--create ngram`
uses the canonical default `five`, while `--create ngram:n` accepts the
tokenizer's numeric or word recipes and stores the canonical word. The app
validates this before creating the burrow, then appends ordinary Bigwig recipe
overrides for the n-gram tokenizer and featurizer. Empty meadow names now mean
`a.meadow` consistently for creation and opening. All 54 targets compile; the
user's initial interactive checks over `ai/` and `src/` find exact
punctuation-heavy C++ substrings, compose them with structural containment to
recover source objects and filenames, and add fixed-width context across a
source newline. These checks are intentionally basic. Step 7, changing code
ingestion so a structural element can span source lines, and exhaustive
literal-matching testing remain outstanding.

The user then authorized the independent regexp machine foundation before the
indexed evaluator. `regexp/nfa.h` exports a complete lambda-free byte NFA as a
sorted vector of transitions with explicit 16-bit symbol sets, together with a
reference matcher returning shortest, overlapping, inclusive byte intervals. The parser
supports the agreed regular-language core and intersection. Virtual `START`
and `END` symbols give `^` and `$` complete-buffer semantics without becoming
ordinary bytes; `\R` covers LF, CRLF, U+2028, and U+2029. Lambda and
empty-language results are errors. Focused coverage is isolated in
`//test:nfa_test`, which passes, and all 57 Bazel targets compile. Indexed NFA
execution remains to be designed.

The first flat-search cut recorded in `ai/cgrep.md` is implemented without
altering the public NFA or any existing source file. `Haystack` exposes
arbitrary byte chunks and inclusive-offset translation; `Cgrep` compiles an
opaque shareable state-by-symbol dispatch machine and iterates shortest,
overlapping matches; and `apps/cgrep` emits streaming JSONL over files or
stdin. The focused C++ and end-to-end JSONL tests are registered as
`//test:cgrep_test` and `//test:cgrep_app_test`; the latter uses `rules_shell`
as a root-only development dependency. The app defaults to omitting match text
over 256 bytes while retaining the interval, provides an unlimited override,
and has command help. Both focused test targets pass.

## Completed Meadowlark Filename And Labeling Step

1. JSONL filename membership is now chunk-based. JSONL writes one `/.`
   envelope and one normalized-filename feature interval per nonempty worker
   transaction rather than one filename interval per `:` record. This preserves
   the important `(<< : filename)` query, which returns the ordinary objects
   from the named file.
2. Activity metadata is now outside file data containers. An `@` metadata
   record is not contained by `/.`; the canonical `/` filename is separate;
   and each nonempty data `/.` chunk contains one leading `//` filename and its
   data payload. Metadata, the canonical filename, and all data chunks form a
   coordinated recoverable commit set, subject to the short sequential
   visibility window recorded in `ai/improvements.md`.
3. New `/` and `//` filename text is framed with the internal JSON string
   tokens. This preserves leading `./` and `/` in display without changing the
   normalized filename feature. Restart recognition tolerates historical raw
   names. Tokenless files deliberately publish `/`, `@`, and `//`, but no
   address-dependent `/.`, `:`, or filename feature.
4. JSON handling now separates lossy arbitrary-interval display through
   `json_translate(...)` from validating full-value conversion through
   `json_convert(...)`; machine parsing uses the latter. The unused
   `Txt::raw(...)` interface has been removed.

The user authorized this package after the semantics discussion spanning
2026-08-20 and 2026-08-21. The first user regression run exposed eager trailing
spaces in display translation and a legacy-restart test that queried an old
read epoch. Both narrow corrections compile successfully. The user subsequently
tested the change in several ways, including against indices dating from 2022,
newer Hazel/Fiver indices, and a build from scratch, and reports that it looks
good. The existing 1.3 TB ClimbMix index also booted and passed extensive use
without observed problems. The commit remains with the user.

The broader long-running-server memory discussion remains deferred. Bigwig
trimming, Hazel text-cache eviction, and service pressure policy are preserved
in `ai/memory.md` for possible later return; they are not the current project.

## Completed File-Oriented Foraging Step

Implementation was authorized on 2026-08-22 and is complete. Its implemented
model and rationale are recorded in `ai/forager.md`. The logical file is the
unit for derived annotations; one global `(name, tag)` definition is separate
from per-file completion records; the query remains top-level; `Forager` is a
transaction-neutral interval worker; and older TF-IDF metadata remains
readable but cannot be extended by the current writer.

Focused cases cover immutable definitions, validation before publication,
default-tag and primary-record selection, multi-worker TSV file scoping and
aggregate placement, literal legacy TF-IDF ranking/refusal, restart/skip
behavior, and write-free NullForager transactions. The user reports that the
complete regression suite and additional tests pass. The MS MARCO build and
ranking path also work, with parallel worker readiness restoring forage time
from the observed 11:23 regression to 3:12.

## Completed Empty And Tokenless Fiver Readiness Step

Write-free and tokenless Fiver transactions now serialize a commit artifact.
Focused coverage exercises direct activation, empty/tokenless/tokenful flat and
tree merges, and empty/tokenless/mixed Fiver-to-Hazel conversion. Hazel text
serialization begins at raw byte zero when tokenless Fivers precede the first
token chunk, while retaining the later token-chunk anchor and normal dust
ownership semantics. The user reports that the full regression suite and
additional tests pass after these changes.
