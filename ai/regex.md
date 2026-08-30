# Preliminary Workplan for Indexed String Matching

This is a preliminary, stepwise engineering plan. Literal byte-string matching
over an n-gram index is operational. Work has begun on the reusable byte-NFA
foundation for general regular-expression matching.

Work proceeds one approved step at a time. This document does not authorize
source changes.

## Current Sequence

1. Fix the `HashingFeaturizer` boundary case and define features `0` and `-1`.
2. Move append separator normalization into the public append operations.
3. Add `count`, `bow`, and `phrase` to the `Tokenizer` interface.
4. Add general literal feature strings to GCL with `|...|`.
5. Implement `NGramFeaturizer` and `NGramTokenizer` together.
6. Compile quoted phrases into literal byte-string matches.
7. Fix Meadowlark code ingestion so literal matching can cross source lines.
8. Build a reusable byte-NFA compiler and reference matcher.
9. Design and implement indexed regular-expression matching.

The first two steps were implemented together on 2026-08-27. Steps 3 and 4
were implemented separately later that day. The `NGramFeaturizer` half of step
5 was implemented on 2026-08-28. The GCL semantic-error prerequisite for the
tokenizer was then added, followed by the reviewed `NGramTokenizer`. Step 5 is
complete; all targets compile and the user's basic testing works. The existing
phrase-expansion path then supplied step 6 without further source changes:
quoted strings compile through `NGramTokenizer::phrase` into exact positional
GCL and work against an n-gram Meadowlark in basic interactive testing.

Meadowlark can create an n-gram collection with `--create ngram` or
`--create ngram:n`. The latter accepts the tokenizer's numeric and word forms,
validates before creating the burrow, and writes the canonical word recipe.
Creation appends standard Bigwig option overrides, replacing both the UTF-8
tokenizer and JSON-wrapped hashing featurizer with their n-gram counterparts.
Bare `--create` retains the existing configuration. Step 7, revising code
ingestion so one structural element spans source lines, is next. Exhaustive
testing is deliberately deferred until that change is complete.

### Implementation Checkpoint: Steps 1 Through 4

- `HashingFeaturizer` maps the `INT64_MIN` hash boundary to one stable positive
  hashed feature without changing any other hash value.
- Feature `-1` is named `universal_feature`. The public `Idx` operations return
  a `UniversalHopper`, equivalent to `FixedWidthHopper(1)`, and count zero, so
  the contract automatically applies to every concrete index and wrapper.
- Indexing paths continue to ignore non-positive features; in particular,
  direct `SimpleBuilder` use cannot store the virtual universal feature.
- Public `Appender::append(...)` and `Builder::add_text(...)` add a newline to
  nonempty input only when its final byte is not space, tab, carriage return,
  or newline.
- Concrete Fiver and Simple append/build paths now store and tokenize exactly
  the normalized input they receive. Redundant Fiver newline mutations during
  readiness and Hazel serialization were removed as part of that contract.
- `Tokenizer` now exposes `count`, `bow`, and `phrase`, with inherited defaults
  based on `split`. Phrase expansion, token accounting, and bag-of-words
  consumers use the corresponding operation. Address-aligned consumers retain
  `split`.
- GCL now parses quoted forms as semantic `QUOTE` nodes while `|...|` decodes
  to an ordinary `TERM`. Ordinary terms serialize raw when safe and otherwise
  through canonical literal syntax. Phrase expansion retains its current
  generated-GCL implementation but safely serializes generated feature terms.
- `bazel build //...` succeeds. The user reports that basic tests pass and
  considers deeper testing unnecessary for this narrow parser change.

## 1. Feature Foundations

### `HashingFeaturizer` Boundary

`HashingFeaturizer` intends to return non-negative features, but negating the
one hash value equal to `INT64_MIN` cannot produce a positive value. Correct
that case without changing any other existing feature:

- Detect `INT64_MIN` before negation.
- Map it to one fixed positive value chosen from the hashed-feature namespace.
- Exclude `0`, `-1`, other reserved values, and the reversible short-string
  namespace.
- Select the replacement once and store it as a constant; indexing must not
  involve runtime randomness.

The branch should have negligible indexing cost. Existing indices and every
non-boundary hash value must remain unchanged. Big-endian index portability is
deferred.

### Reserved Features

Feature `0` remains the null feature. A tokenizer may emit a token position
whose feature input is a designated internal noncharacter that the featurizer
maps to `0`. The position participates in address and text accounting, but no
posting is stored.

Feature `-1` is the universal positional feature:

- `Idx::hopper(-1)` returns a `UniversalHopper` equivalent to `(# 1)`.
- `Idx::count(-1)` returns `0`.
- Feature `-1` has no stored posting list.
- Index construction continues to store ordinary non-negative features.
- Bag-of-words and ranking consumers ignore `-1`.

The `-1` contract belongs to every index implementation and wrapper, not to
one concrete index.

## 2. Append Normalization

An append operation may add a newline to prevent a token splice. This is an
append contract, not a storage-implementation detail.

Move normalization into the public entry points:

- `Appender::append(...)` normalizes its nonempty input before calling
  `append_(...)`.
- `Builder::add_text(...)` applies the same rule before calling
  `add_text_(...)`, because builders are also used directly.
- A concrete appender stores and tokenizes exactly the bytes it receives.
- Remove duplicate newline insertion from `FiverAppender`, `SimpleAppender`,
  and `SimpleBuilder`.
- Empty appends remain empty.

Use exactly four append separators: space, tab, carriage return, and newline.
All four prevent token splices for the existing word tokenizers. Do not include
other controls such as vertical tab or form feed. The n-gram tokenizer treats
the stored separator byte literally; this append rule only establishes the
stored boundary byte when one must be added.

This makes Fiver consistent with Simple. Currently Fiver adds the newline to
stored text but tokenizes the original input, while Simple tokenizes the
extended stored buffer. The existing tokenizers do not make these separator
bytes tokens, so the change should not alter their ordinary term postings. An
append ending in carriage return will retain that carriage return rather than
acquiring an additional newline.

## 3. Tokenizer Interface

The current `split` operation is used for several unrelated purposes. Add
operations named for their consumers:

```text
count(text)  -> number of token positions
bow(text)    -> vector<string> for bag-of-words and ranking use
phrase(text) -> vector<string> for literal phrase expansion
```

Defaults preserve the current tokenizers:

```text
count(text)  = split(text).size()
bow(text)    = split(text)
phrase(text) = split(text)
```

`split` remains the lightweight form of tokenization: it returns one canonical
feature string per token position, preserving token order and address
alignment, without featurization or annotation metadata.

ASCII and UTF-8 tokenization therefore remain unchanged. `NGramTokenizer`
will override the operations.

`phrase` returns raw feature strings to be handed to the active featurizer.
It does not return GCL. The current phrase expander already consumes a
`vector<string>` from `split` and featurizes its terms later, so it can call
`phrase` instead.

The expander currently concatenates generated terms into GCL text and reparses
it. Arbitrary feature strings make that fragile. It should construct the
S-expression directly, or serialize each term through the literal feature
syntax described below.

Text token accounting and recovery use `count`, rather than constructing a
vector solely to inspect its size. Traditional ranking, feedback, and
foraging consumers use `bow`. Phrase expansion uses `phrase`. Ranking code
that relies on vector position for token addresses, and diagnostic code that
expects the canonical token at an address, continue to use `split`.

## 4. Literal Feature Strings in GCL

Add a general GCL term form:

```text
|literal feature string|
```

The contents denote exactly one feature string. The GCL parser unescapes the
contents and later passes the resulting bytes to the active featurizer. The
parser does not decide whether the string is hashed, reversibly encoded, or
otherwise interpreted.

Support a conservative C/C++-like escape set:

- `\\` for backslash and `\|` for the delimiter;
- named ASCII controls `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, and `\v`;
- the exact-width byte escape `\xHH`;
- the exact-width Unicode escapes `\uHHHH` and `\UHHHHHHHH`, encoded as
  UTF-8;
- an unknown escape drops the backslash, so `\q` denotes `q`;
- an incomplete byte or Unicode escape follows the same unknown-escape rule;
- a trailing unpaired backslash is an error.

A physical newline cannot occur inside a GCL term, but `\n` may represent a
newline byte. NUL support remains deferred. Serialization should choose a
canonical visible form and round-trip all supported byte strings. Unicode
escapes must reject surrogates and out-of-range values.

Quoted forms are represented separately from literal features. The parser
stores the complete quoted spelling, including its delimiter, in a `QUOTE`
node without assigning semantics to the delimiter. Thus `"foo bar"` is a
`QUOTE`, while `|"foo bar"|` is a `TERM` containing the same bytes. Phrase
expansion interprets `QUOTE` nodes marked with `"`; a later stage may interpret
single quotes as regular expressions. Backticks remain reserved.

`term_to_gcl(...)` is the canonical inverse of literal parsing. It emits a raw
term exactly when that spelling reparses as the same ordinary `TERM`, and uses
escaped `|...|` otherwise. Consequently `|foo|` serializes as `foo`, while
`|foo bar|` remains literal syntax. The existing phrase expander uses this
function when inserting tokenizer-produced features into generated GCL.

This syntax is independent of n-grams. It is useful whenever a feature string
contains whitespace, controls, delimiters, or other bytes awkward to express
as an ordinary GCL term.

### Semantic Expansion Errors

GCL has an internal `ERROR` expression for failures discovered after parsing.
It is not surface syntax and cannot be parsed from user input. An error stores
an explanatory message, propagates through parent expressions during phrase
expansion, and is reported before optimization or hopper construction.
`Optimizer::estimate_memory(...)` returns zero for such an uncompilable
expression.

In particular, a quoted phrase for which `Tokenizer::phrase(...)` returns no
terms becomes an `ERROR`. This provides the clean failure path needed for
n-gram queries shorter than the selected gram size and can later carry regexp
compilation errors without overloading `term_`.

## 5. N-Gram Featurizer and Tokenizer

`NGramTokenizer` and `NGramFeaturizer` implement one shared feature-string
protocol and must be designed and tested together.

### Shared Typed-String Protocol

Reserve internal Unicode noncharacters for protocol strings:

- U+FDDA is the n-gram marker. A payload shorter than eight bytes is packed as
  a reversible, NUL-terminated feature string; a longer payload maps to `0`.
- U+FDDB is the universal marker. Any token beginning with it maps to `-1`,
  with any remaining payload ignored.
- U+FDDC is the translation marker. Its payload is the hexadecimal value of a
  hashed feature; malformed encodings map to `0`.
- The empty string remains the null feature `0`.
- The ten JSON structural tokens U+FDD0--U+FDD9 also map to `0`, replacing the
  behavior supplied by `JsonFeaturizer` in an n-gram configuration.

`NGramTokenizer` will produce these marked strings. `NGramFeaturizer`
recognizes and converts them. An unmarked nonempty string is always hashed,
preserving a namespace distinct from reversible textual n-grams. Whenever
`HashingFeaturizer` and `NGramFeaturizer` hash the same bytes, they use the same
MurmurHash routine, seed, sign normalization, overflow replacement, and hashed
feature marker bit, and therefore produce the same feature value.

The marker is a type/dispatch prefix and is not part of the n-gram payload.
The same marked feature string can be written explicitly in GCL by escaping it
inside `|...|`.

`NGramFeaturizer::translate` returns the universal marker for a negative
feature, the n-gram marker plus literal bytes for a reversible feature, or the
translation marker plus lowercase hexadecimal for a hashed feature. Thus
`featurize(translate(feature))` preserves every supported feature class; null
translates as an n-gram marker with an empty payload. The featurizer has no
recipe and does not know the configured gram size.

### Gram Size and Encoding

Support exactly one configured gram size per index:

```text
1 <= n <= 7
```

Record `n` in the tokenizer DNA. The empty recipe defaults to `five`. Input
recipes accept `1` through `7` and
the words `one` through `seven`; `recipe()` always returns the word. Four makes
short code fragments such as `skip`, `case`, `bool`, and `addr` independently
searchable, while five should provide shorter posting lists on very large
collections. Smaller grams remain valid for experiments and specialized
collections, but their posting lists are likely to be long; one-grams in
particular are unlikely to be an efficient general-purpose configuration.

The reversible representation packs up to seven payload bytes into an `addr`.
Input bytes are preserved literally, including space, tab, carriage return,
newline, and malformed UTF-8. The existing encoding is machine-local;
big-endian index portability remains deferred.

### Token Positions and Translation

Every ordinary stored byte is a token position, including newline and malformed
UTF-8. The tokenizer performs no character decoding or classification. Each
complete UTF-8 encoding in the reserved U+FDD0--U+FDEF noncharacter block is
instead one atomic structural position. This covers the ten assigned JSON
tokens and leaves the rest of the block available for future structure.

For every returned token:

- `address` is the token's relative position;
- `offset` is the byte offset in the supplied buffer;
- `length` is `1` for an ordinary byte and `3` for a reserved structural token.

`skip(buffer, length, n)` scans token boundaries because structural tokens
consume three bytes while advancing one address. Translation can still return
the exact inclusive byte interval selected by the addresses.

This relies on append normalization giving the tokenizer exactly the bytes
stored, including an automatically inserted newline.

### Index Annotation Generation

An n-gram candidate begins at each ordinary byte position. It contains up to
`n` literal bytes, stopping before the next structural token or at the end of
the supplied structural element. A candidate near either boundary is emitted
as the literal short suffix that is actually available.

- A gram does not cross the end of the appended structural element.
- A gram does not include or cross a reserved structural token.
- Newline is an ordinary byte; grams may include it and cross it when the
  newline occurs inside one supplied element.
- A reserved structural position receives the empty string and therefore null
  feature `0`.
- There is exactly one token record and one stored textual annotation at each
  ordinary position.

For example, with `n = 4`, the bytes `hello\n` produce:

```text
0  hell
1  ello
2  llo\n
3  lo\n
4  o\n
5  \n            newline position
```

Lexical matching is local to a supplied structural element because the
tokenizer has no context outside it and matching across JSON elements is not
meaningful. A newline is not a boundary when it occurs inside that element.
Token positions remain globally composable: phrases and other GCL expressions
may relate local evidence across boundaries when desired.

Short suffix grams preserve the bytes at the end of every supplied element.
Dictionary-backed phrase and regexp expansion can combine them with complete
grams to discover expressions shorter than `n`; the initial phrase expander
does not yet provide that dictionary access.

### `count`, `bow`, and `phrase`

For `NGramTokenizer`:

- `count` scans and returns the number of logical positions.
- `split` mirrors tokenization without address metadata: complete and short
  marked grams plus empty structural positions remain aligned.
- `bow` returns only complete marked grams, omitting universal and null
  positions.
- Until phrase expansion has dictionary access, `phrase` retains only complete
  marked grams and converts short suffix and structural positions to the
  universal marker. If the input contains no complete gram, it returns an
  empty vector so phrase expansion reports a semantic error rather than
  constructing a query that only finds element-ending occurrences.

## 6. Literal Byte-String Matching

Before implementing general regular expressions, make quoted phrases mean this
exact sequence of bytes. Space, tab, carriage return, and newline are distinct,
as are repeated separator bytes. Stored text and returned intervals are
unchanged.

`NGramTokenizer::phrase` produces the raw marked feature strings. Phrase
expansion turns them into positional GCL over their featurized n-grams and
universal positions, then the existing optimizer may rewrite the result.

Individual n-gram evidence remains local. GCL supplies interval width,
ordering, containment, and any explicit composition across line or structural
boundaries. This stage should establish precise extent and translation behavior
before regular-expression automata add alternatives and repetition.

Examples must be checked in both directions: generate index tokens for the
candidate text, compile the literal query, and walk the resulting positional
constraints. Include all four literal separator bytes, separator runs,
append boundaries, arbitrary UTF-8 bytes, malformed input, and strings shorter
than `n`.

The basic end-to-end path is working without additional implementation beyond
the completed tokenizer, featurizer, and phrase-expansion work. Interactive
checks over `ai/` and `src/` found exact punctuation-heavy C++ fragments,
including fragments beginning and ending inside identifiers and C++ escape
spellings. Ordinary GCL containment recovered the containing source object and
its `//` filename annotation, while fixed-width context composed around the
literal match and crossed a source newline. These are smoke tests only; the
systematic cases above remain for the exhaustive pass after step 7.

## 7. Meadowlark Code Line Boundaries

Before general regular-expression work, revise the Meadowlark `code` input
path so one code structural element may span source lines. The n-gram tokenizer
can then see and index grams across embedded newline bytes instead of treating
each source line as a structural boundary. Preserve line-oriented metadata
through annotations or later foraging rather than by splitting the indexed
text into line-sized elements. This is a deferred Meadowlark ingestion change,
not part of the tokenizer/featurizer implementation. It remains outstanding;
after it is complete, run the exhaustive literal-matching tests before indexed
regular-expression integration.

## 8. Regular Expressions: Preliminary Direction

General indexed regular-expression matching is not yet fully planned. The
following surface is a rough implementation guide, not a compatibility promise.

### NFA Construction Checkpoint

`regexp/nfa.h` now provides the independent machine-construction layer:

    vector<transition> nfa(const string &regexp, string *error)
    vector<pair<size_t, size_t>> match(const vector<transition> &nfa,
                                      const string &text)

The exported transition vector is the complete machine description, so an
indexed evaluator or another external consumer can reorganize it without using
the parser. State `0` is the start and `final_state` is the final state. Each
transition holds a `set<unsigned char>` and a `complement` flag; it accepts the
listed bytes normally or all unlisted bytes when complemented. The vector is
sorted by decreasing source state and then decreasing destination state.

Construction internally preserves whether an expression accepts lambda while
combining union, concatenation, closure, and intersection. A successful public
machine is nonempty and lambda-free. A regexp accepting lambda or no strings is
reported as an error rather than represented by an empty vector.

Shortest-substring matching is locally defined: an accepted interval is a
result exactly when none of its proper subintervals is accepted. This semantic
contract is independent of tokenization. Byte n-grams and postings are future
index-access mechanisms for running the same machine, not part of the
definition of a match.

The reference `match` implementation directly executes the NFA. It retains the
largest start position when paths collide in one state and removes containing
solutions, producing shortest, overlapping, inclusive byte intervals. It is
primarily executable specification and focused test support, but is also
usable for flat strings. Its coverage is the separate `//test:nfa_test` target
rather than part of the large aggregate test binary.

### Initial Syntax

The initial compiler provides the widely used regular-language core:

- literal bytes and literal UTF-8 strings;
- concatenation and `|` alternation;
- grouping with `(...)`;
- the `*`, `+`, and `?` quantifiers;
- dot as any ordinary byte, including newline; and
- byte classes such as `[abc]`, `[a-z]`, and `[^a-z]`.

Forms such as `.*`, `.+`, and `.?` are ordinary applications of a quantifier
to dot, not additional operators. Parentheses group but do not capture. The
precedence is quantification, concatenation, intersection, then alternation.

Character-class labels are byte sets, represented in the NFA as an explicit
set plus a complement flag. Ranges are numeric byte ranges and are normally
ASCII-shaped, for example `[0-9]`, `[a-z]`, and `[A-Fa-f0-9]`;
`\xHH` permits arbitrary byte values. A class consumes exactly one byte.
Multibyte UTF-8 literals are therefore not class members or range endpoints;
Unicode alternatives can be written with ordinary grouping and alternation.
A later Unicode-aware surface could compile character predicates into byte
automata without changing the indexed matcher.

The useful initial escapes are escaped metacharacters, `\\`, `\n`, `\r`,
`\t`, `\f`, `\v`, and `\xHH`. The common shorthand byte classes `\d`, `\s`,
and `\w`, together with their complements, have explicit ASCII meanings.
Consistent with literal feature strings, an otherwise unknown
escape quotes the following character; a trailing backslash is an error.

Intersection `&` is included in the compiler, with precedence between
concatenation and alternation. Counted repetition (`{m}`, `{m,n}`, and
`{m,}`) is deferred. It adds no expressive power and can later be compiled
into concatenation, optional paths, and closure with an expansion limit. `^`
and `$` are also deferred until their
relationship to structural boundaries is defined. Lazy or possessive
quantifiers, captures, backreferences, lookaround, conditionals, recursion,
and embedded code are outside the intended language. In particular,
shortest-substring semantics removes the need for greediness modifiers.

Known design inputs to revisit later are:

- regular-expression terms expand over the reversible n-gram dictionary;
- matching is local to supplied structural elements, with newline treated as
  an ordinary byte and GCL handling wider relationships;
- results are shortest, nonnested intervals;
- when automaton paths collide in one state, retain the largest starting
  address;
- selective posting lists should drive query planning;
- regexp syntax need not promise POSIX compatibility; and
- unselective expressions cannot be made selective merely by introducing a
  universal feature.

Relevant prior work includes Clarke's *An Algebra for Structured Text Search*
(1996), Clarke and Cormack's *On the Use of Regular Expressions for Searching
Text* (1995), and Qiu et al.'s *Efficient Regular Expression Matching Based on
Positional Inverted Index* (2020/2022). Positional regexp evaluation itself is
not a novelty claim.

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

The replacement is deliberately mechanical: every stored byte is a token
position, every proper gram has one configured byte length, and Unicode
semantics—if wanted—belong above the positional representation.
