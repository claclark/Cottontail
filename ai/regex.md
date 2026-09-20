# Indexed String Matching: Checkpoint and Preliminary Plan

Updated 2026-09-20. The tokenizer/featurizer foundations, basic indexed literal
matching, and independent byte-NFA compiler are implemented. General indexed
regexp execution is not. The flat-file scanner is described in
[cgrep.md](cgrep.md).

Regexp development is paused while the active direction returns to shipping
Meadowlark; see [plan.md](plan.md). This document preserves implemented
contracts and unfinished reasoning, not authorization for source changes.

## Return Plan

1. Keep the current scanner stable and collect real agent searches through
   opt-in CGREP_LOG instrumentation. Its private development contract is in
   cgrep.md; no logging is enabled by default.
2. When the collection is useful, return to general regexp execution over
   inverted lists. Establish the transition-seeking algorithm and its
   correctness before optimizing it. Resolve dictionary access, short
   matches, structural boundaries, and the Warren/GCL entry points below.
3. Use the collection to select useful follow (... r0 r1) and union
   (+ r0 r1) special cases for both indexed execution and the scanner. Check
   that optimizations preserve all shortest, overlapping matches.
4. Keep cross-line Meadowlark code ingestion and exhaustive indexed literal
   tests in the indexed-matching workstream. They remain unfinished, not the
   next automatic task or a newly imposed Meadowlark release requirement.

Do not begin by decomposing every regexp into follow components, deleting
self-loops, replacing the existing machine vector, or optimizing dot-only
paths. Those directions were explored, not implemented. The latest preference
is to get the general algorithm working first.

## Matching Semantics

Let G select the accepted intervals having no accepted proper subinterval.
These are shortest-substring matches, not a single globally shortest match.
Results may overlap but do not nest. The rule is local, independent of lines,
documents, tokenization, or leftmost-longest conventions.

The flat scanner consumes literal bytes. Newline is ordinary input; ^ and $
refer to the complete input. Indexed regexp matches are intended to remain
within supplied structural elements; GCL can express wider relationships.
How to enforce that boundary for every indexed regexp remains part of the
implementation design.

A regexp accepting lambda is rejected by the public NFA compiler. Nullable
subexpressions are nevertheless essential while constructing a larger,
nonnullable expression. An empty language is also rejected.

## Implemented Index Foundations

These correspond to steps 1--6 of the original workplan. The reusable NFA
foundation (original step 8) was implemented independently, before the
deferred code-ingestion change (step 7).

### Feature and Append Contracts

- HashingFeaturizer maps the INT64_MIN normalization boundary to the fixed
  positive hashed feature 0x6f2d4b18a937c5e1. Other feature values are unchanged.
  The shared MurmurHash routine is also used by NGramFeaturizer.
- Feature 0 is null: it can account for a token position without a posting.
  Index construction ignores non-positive features.
- Feature -1 is universal_feature. Idx::hopper(-1) returns UniversalHopper,
  equivalent to (# 1); Idx::count(-1) is zero. It has no stored posting list.
  GCL compilation also uses UniversalHopper for (# 1); (# 0) is a parse error.
- Appender::append and Builder::add_text append LF to nonempty input only
  when its final byte is not space, tab, CR, or LF. Concrete implementations
  store and tokenize the same normalized bytes; the old Fiver discrepancy is
  fixed. Empty appends stay empty.
- Those four append separators do not imply whitespace normalization in the
  n-gram tokenizer. Spaces, tabs, CR, LF, and repeated separators stay distinct.
  Big-endian feature portability remains deferred.

### Tokenizer Interface

Tokenizer provides tokenize, skip, count, bow, phrase, and split.

- tokenize supplies feature/address/byte-offset records for indexing.
- skip advances through logical positions for text translation.
- count returns the number of logical positions.
- bow supplies feature strings for bag-of-words/ranking consumers.
- phrase supplies feature strings for phrase expansion, not GCL.
- split supplies the address-aligned feature-string sequence without
  annotation metadata.

Default count/bow/phrase implementations use split, preserving existing
tokenizers. Their callers were separated by purpose; address-sensitive
consumers retain split. NGramTokenizer overrides all relevant operations.

### Literal Features and Quoted GCL

The implemented |...| syntax denotes exactly one raw feature string. The
parser decodes it; the active featurizer later decides its feature value.

Supported escapes are `\\` and `\|`; named controls `\a`, `\b`, `\f`, `\n`, `\r`, `\t`,
`\v`; exact-width `\xHH` bytes; and `\uHHHH` / `\UHHHHHHHH` Unicode values encoded
as UTF-8. Unknown escapes drop the backslash. Incomplete numeric escapes
follow that unknown-escape rule; a trailing unpaired backslash is an error.
Unicode escapes reject surrogates and out-of-range values. Physical LF and
NUL are not supported in literal-feature syntax; escaped LF is supported.

Quoted spelling, including its delimiter, is retained in a QUOTE node.
Thus "foo bar" is a QUOTE while |"foo bar"| is a TERM with the same bytes.
Phrase expansion conventionally interprets double quotes. Single-quoted
regexp terms and Warren-level expansion remain future work; backticks remain
reserved.

term_to_gcl serializes TERM values canonically, using raw spelling only when
it reparses as the same TERM. phrase_to_string normalizes quoted phrase
spelling before Tokenizer::phrase. The phrase expander still constructs GCL
text and reparses it, but escapes generated features through term_to_gcl.
Replacing that construction with direct tree building is not an active task.

An internal ERROR node with message_ propagates semantic expansion failures
before optimization or hopper construction. An empty phrase result reports
Cannot expand phrase rather than becoming a meaningless positional query.
Optimizer::estimate_memory returns zero for an uncompilable query.

### NGramFeaturizer

The tokenizer and featurizer share a typed-string protocol:

| Prefix | Payload and feature |
| --- | --- |
| U+FDDA, NGRAM_MARKER | Fewer than eight payload bytes are packed literally into an addr; longer payloads map to 0. |
| U+FDDB, UNIVERSAL_MARKER | Maps to -1 regardless of payload. |
| U+FDDC, TRANSLATE_MARKER | Hexadecimal hashed feature; malformed or out-of-namespace input maps to 0. |
| No marker | Nonempty ordinary strings are hashed. |
| Empty string or a JSON structural token U+FDD0--U+FDD9 | Maps to 0. |

The actual C++ constants are ngram_marker, universal_marker, and
translate_marker in src/ngram_featurizer.h. Markers are dispatch prefixes,
not part of the gram payload. They can be entered via escaped |...| syntax.

Whenever HashingFeaturizer and NGramFeaturizer hash the same bytes, their
hash values agree. NGramFeaturizer does not apply HashingFeaturizer's ordinary
short-string shortcut: reversible grams require the explicit marker.
Its recipe must be empty, and it has no knowledge of the gram size.

translate returns the universal marker for negative features, the n-gram
marker plus string bytes for a reversible feature, or the translation marker
plus lowercase hexadecimal for a hash. Null translates as an empty marked
gram. The encoding is zero-padded and machine-local.

NUL caveat: literal packing accepts byte payloads, but translation reconstructs
a C string and loses bytes after an embedded NUL. Zero padding also cannot
distinguish a payload from the same payload with trailing NULs. Do not promise
unrestricted binary dictionary round-tripping or infer it from cgrep's binary
support. Resolve the supported indexed domain before dictionary-backed binary
matching; no encoding change is authorized here.

### NGramTokenizer

Exactly one width is configured per index: 1 <= n <= 7. Recipes accept the
digits 1--7 or words one--seven; empty defaults to five, and recipe() returns
the canonical word. Four may suit short identifiers; five may offer greater
selectivity. The supported range and default are separate from future workload
tuning.

Every ordinary byte is one position, including whitespace and malformed UTF-8.
A complete UTF-8 encoding in U+FDD0--U+FDEF is instead one atomic structural
position. This whole reserved block is treated structurally, not just the ten
currently assigned JSON tokens.

For each ordinary byte, tokenize generates the marked gram containing up to
n bytes of right context. It stops at the supplied element end or before a
structural token. Short suffixes are stored literally, not padded and not
replaced by universal features. Structural positions receive null feature 0.
There is one token record per logical position. Whether a posting is stored
depends on its feature; null-valued byte encodings share the caveat above.

With n = 4, hello followed by LF produces:

    0  hell
    1  ello
    2  llo\n
    3  lo\n
    4  o\n
    5  \n

Newline does not itself stop a gram. Structural tokens have byte length three
but consume one address, so skip and count scan boundaries rather than simply
adding byte offsets.

- split mirrors tokenize as marked complete/short grams and empty structural
  entries, preserving address alignment.
- bow retains only complete grams.
- phrase retains complete grams and replaces short/structural entries with
  the universal marker. It returns empty if there is no complete-gram evidence.

Short suffix storage prepares for dictionary-backed queries shorter than n;
it does not make those queries work through the current phrase expander.

### Indexed Literal Matching and Ingestion

Meadowlark --create ngram selects the paired tokenizer/featurizer with width
five; --create ngram:n accepts numeric or word widths. The app validates the
choice, appends ordinary Bigwig recipe overrides, and records the canonical
recipe. Bare --create keeps the ordinary configuration. Empty meadow names
resolve to the default a.meadow for creation and opening.

Double-quoted phrases already lower to exact positional GCL through
Tokenizer::phrase. Spaces and punctuation remain literal, not word-tokenized.
Basic interactive checks over ai/ and src/ found exact C++ fragments and
composed them with source/filename containment and fixed-width context.

Grams remain local to individual append elements. The deferred Meadowlark
code change will make a code element span source lines, retaining line metadata
through annotations or later foraging. Do not claim this ingestion change is
already implemented because cgrep can scan multiline files.

Before indexed regexp integration, revisit the deferred exhaustive literal
cases: separator runs, append/structural boundaries, short queries, UTF-8 and
malformed bytes, exact extents, and translation consistency. The existing
smoke tests do not establish those cases exhaustively.

## Implemented NFA Foundation

regexp/nfa.h exports:

    std::vector<transition> nfa(const std::string &regexp, std::string *error);
    std::vector<std::pair<std::size_t, std::size_t>>
    match(const std::vector<transition> &machine, const std::string &text);

A transition holds from/to states and std::set<symbol>. State 0 is the start;
final_state is a sentinel, not an ordinary numbered state. Symbols use
std::uint16_t: 0--255 are bytes, followed by START and END. The vector is
normalized, compactly numbered, and sorted by decreasing from then to state.

Construction keeps lambda separately while combining expressions. Closure
adds restart transitions; concatenation explicitly adds nullable bypasses.
It does not minimize states or preserve a deterministic shape merely because
one exists for the regexp.

For example, ab*c compiles to:

    <0,1,a>  <0,2,a>
    <1,1,b>  <1,2,b>
    <2,F,c>

The reference matcher retains the latest start at each state and input
position, returning shortest overlapping inclusive intervals. START and END
are consumed just outside the complete input, and their virtual positions are
stripped from results. The reference matcher and cgrep handle embedded NUL.

### Supported Syntax

- Literal bytes and UTF-8 strings, concatenation, grouping, and alternation.
- Intersection &, with precedence below concatenation and above alternation.
- `*`, `+`, and `?` quantifiers.
- Dot accepts every ordinary byte, including LF, but not START or END.
- Classes/ranges such as `[abc]`, `[a-z]`, and `[^a-z]`, expanded to byte sets.
- `^` and `$` are complete-input boundaries, not line anchors.
- `\R` accepts LF, CRLF, U+2028, and U+2029; it is not valid inside a class.
- Escaped metacharacters, `\\`, `\n`, `\r`, `\t`, `\f`, `\v`, and exact-width `\xHH`.
  `\d`, `\s`, `\w` and their complements have ASCII meanings. An otherwise unknown
  escape quotes the next character; a trailing backslash is an error.

A class consumes one byte; multibyte Unicode characters are not class members
or range endpoints. Use ordinary alternatives for UTF-8 literals. Unicode
predicates could later compile to byte automata without changing the index.
`\R` recognizes alternatives under G as usual: on CRLF, LF alone is a proper
matching subinterval, so shortest-match selection can report just LF.

Counted repetition, word-boundary assertions, and Unicode class semantics are
deferred. There are no captures, backreferences, lookaround, lazy/possessive
quantifiers, recursion, or embedded code. This is not POSIX/PCRE compatibility.

## Indexed Execution: Still to Design

The goal is execution over n-gram postings, not merely identifying documents
and rescanning their text.

- Give Warrens phrase and regexp operations. The proposed default phrase path
  preserves today's expansion; the proposed default regexp path is unsupported.
  Signatures, dictionary access, and GCL integration have not been implemented.
- Expand labels/classes against the actual reversible gram dictionary.
  A candidate was discussed as (gram context, NFA state), carrying start/end
  positions; for five-gram evidence, the context would contain four bytes.
  This remains a design sketch, not a settled state representation.
- During dictionary traversal, discover matches shorter than n, including
  those represented at element ends by short suffix grams. Lazily merge their
  adjusted postings with longer-machine results. A large expansion may need
  limits; do not promise efficient one-byte queries merely because possible.
- Seek forward/backward in posting lists using tau/rho and their reverse
  counterparts. Exactness must come from gram content, overlap, positions, and
  boundary evidence. For flat files, verification can run the original NFA on
  candidate bytes; indexed verification must not silently become text scanning.
- Resolve structural-element boundaries and indexed START/END evidence.
  Cross-element GCL composition is distinct from one lexical regexp match.
- Latest-start merging is established for paths at the same state and input
  position. Proposed merges of asynchronously positioned candidates, including
  max(start)/min(end), still require an invariant and proof.
- General completeness, failed-candidate pruning, restart rules, intersection,
  and all-dot cases must be specified before performance claims.

### Frontier Proposal

Latest discussion, not implemented or proved for arbitrary NFAs:

1. From the active states, collect outgoing symbol labels, excluding only
   all-byte self-loops from the seek frontier.
2. Seek the next occurrence of a frontier symbol. A non-self-loop dot accepts
   every byte and therefore forces an ordinary one-byte advance.
3. Dot-self-loop states persist across the seek and participate in processing
   the found byte, including their other outgoing transitions. They are not
   deleted from the graph.
4. Restricted self-loops, such as the b loop in ab*c, remain ordinary
   transitions. There is no general "remove all self-loops" transformation.
5. Seek forward to an ending q, tighten backward to a starting p, and check
   the candidate against the original expression. Width alone is a shortcut
   only for fixed strings.
6. Restart at p + 1, not q + 1, to preserve overlaps, once the required
   completeness/pruning property is established.

Examples: abc suggests (... a b c) followed by exact checking; ab|bc suggests
(+ (... a b) (... b c)) followed by checking. These examples are not a proof
that arbitrary regexp composition commutes with G and verification.

For a.*c, the compiler's dot-loop state keeps reactivating the state waiting
for c. The useful seek frontier is {c}. For a.*ef, after an e the selective
frontier can be {e,f}, while the dot-loop state stays alive. Competing branches
make this persistence essential: in a(bc)*d | a.*e, processing b in the first
branch must not kill the second branch's ability to finish at e.

Ordinary dot transitions and dot-only prefixes/suffixes should be processed
normally in the first algorithm. Closure shortcuts, completion distances,
endpoint padding, and clever set-search dispatch are possible optimizations,
not prerequisites.

### Counterexamples and Superseded Directions

- For a(bdac)*d on abdacd, the whole string is a shortest exact match.
  Relaxing to (... a d) yields smaller candidates abd and acd, both invalid,
  and misses the real match. Preserve the repeated body's transitions.
- More generally, language inclusion after adding arbitrary gaps does not
  imply inclusion of the shortest intervals. Do not reuse that rejected
  argument as a proof of candidate completeness.
- The proposed operational frontier seeks are not simply ordinary NFA
  execution with arbitrary extra .* loops at every state. The semantics of
  skipped bytes and surviving states must be made explicit.
- Removing self-loops and discarding every non-firing state fails, for example,
  on ab*c | abbx against abbc. Retaining ordinary loops fixes that particular
  example; it does not by itself prove the full algorithm.
- We considered detecting true .* separators by choke points and decomposing
  a machine into follow components. Nullable bypasses complicate recognition,
  and the decomposition was set aside as premature. No follow() was added.
  Workload-driven follow special cases remain a later possibility.
- One-token-per-Unicode-character indexing was rejected in favor of byte
  positions. Its historical outline is retained below.

### Literature Leads

These are comparison material, not evidence that the proposed general runner
is correct or novel:

- Clarke and Cormack, [On the Use of Regular Expressions for Searching
  Text](https://cs.uwaterloo.ca/research/tr/1995/07/regexp.pdf), 1995 report
  (TOPLAS publication 1997): shortest-substring semantics and direct NFA
  execution. Clarke's [An Algebra for Structured Text
  Search](https://plg.uwaterloo.ca/~claclark/phd.pdf) supplies the structural
  algebra background.
- Das et al., [Episode
  Matching](https://www.researchgate.net/publication/2313349_Episode_Matching),
  1997: forward subsequence matching, backward tightening, restart after the
  tightened start; also discusses automata for more general patterns.
- Yamamoto, [A Faster Algorithm for Finding Shortest Substring Matches of a
  Regular Expression](https://doi.org/10.1016/j.ipl.2018.12.001), 2019:
  extends the scanning approach using Thompson NFAs and efficient epsilon
  handling. This is not the proposed posting-list seek algorithm.
- Qiu et al., [Efficient Regular Expression Matching Based on Positional
  Inverted Index](https://doi.org/10.1109/TKDE.2020.2992295), 2020 online /
  2022 issue: gram-driven NFAs and positional constraints evaluated through
  the index. Compare before making novelty or performance claims.

## Rejected Tokenization Plan

The earlier plan attempted to preserve Unicode characters as token positions
while indexing variable-width contextual byte grams. It divided Unicode input
into four classes:

- `TOKEN`: a character that created a position and could start an annotation;
- `WHITESPACE`: a maximal non-newline whitespace run represented as one token;
- `IGNORE`: presentation selectors and similar characters removed from token
  and annotation semantics; and
- `BREAK`: newline, which created no position and terminated context.

Annotations were required to contain at least four and at most seven UTF-8
bytes, end on a valid character boundary, and sometimes appear multiple times
at one position. The plan included special rules for Chinese bigrams,
four-byte emoji, variation selectors, combining context, incomplete UTF-8,
continuation bytes, normalized whitespace, and positions represented only by
the universal feature.

This plan was rejected. Preserving character positions forced the tokenizer to
solve Unicode interpretation, normalization, variable-width context, malformed
input, multiple annotations per position, and translation accounting all at
once. Those complications were not required by the index.

The replacement is deliberately mechanical: ordinary bytes are token
positions, reserved structural noncharacters are atomic positions, and full
grams have one configured byte length with shorter suffixes at structural
boundaries. Unicode search semantics—if wanted—belong above this representation.
