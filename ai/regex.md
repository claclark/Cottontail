# Preliminary Workplan for Indexed Regular Expressions

This document records the current design discussion. It is preliminary and is
expected to be revised as examples expose problems. Work proceeds one agreed
step at a time; this document does not authorize coding any step.

The implementation sequence is:

1. Make the narrow `HashingFeaturizer` correction and reserve feature `-1`.
2. Add visibly literal features to GCL.
3. Add the contextual byte-gram tokenizer and featurizer.
4. Improve phrase search as the first restricted form of regular-expression
   matching.
5. Add regular-expression matching over the positional index.

Each step should be completed, checked, and discussed before work begins on
the next one.

## Semantic Foundation

A regular-expression result is a generalized concordance list. Matches use
the shortest-substring semantics developed for GCL: results do not properly
contain other results satisfying the expression. In automaton terms, when
paths collide in the same state, the largest starting address is retained.

The matcher does not define a built-in search universe. Documents, fields,
JSON elements, passages, code regions, and arbitrary annotations are imposed
by ordinary GCL composition. Likewise, a newline is not the implicit unit of
search.

The expression operates over token addresses, not an undifferentiated byte
stream. The planned tokenizer normalizes the distinctions that the index can
reasonably support. In particular, this work does not aim for rigid POSIX
regular-expression compatibility.

## Step 1: Featurizer Boundary and the Universal Feature

### `HashingFeaturizer`

`HashingFeaturizer` intends to return non-negative features, but negating the
one hash value equal to `INT64_MIN` does not produce a positive value. Fix this
without changing any existing index feature other than that single boundary
case:

- Detect `INT64_MIN` before attempting to negate it.
- Map it to one fixed, randomly selected positive feature value.
- Choose the replacement from the hashed-feature namespace, excluding `0`,
  `-1`, other reserved values, and values used by the reversible short-string
  encoding.
- Select the replacement once and make it a stable constant. Indexing must not
  involve runtime randomness.

The added branch should be effectively free in the indexing path. Big-endian
portability of feature encodings is explicitly deferred; Cottontail indices
need not currently be portable between machines.

### Feature `-1`

Reserve feature `-1` as the universal positional feature:

- `Idx::hopper(-1)` returns a universal hopper equivalent to `(# 1)`.
- `Idx::count(-1)` returns `0`.
- Feature `-1` has no stored posting list.
- Index construction continues to store only ordinary non-negative features.
- Bag-of-words and ranking consumers ignore `-1`; it has positional meaning
  but no collection frequency.

`UniversalHopper` is the tentative name for the hopper implementation. The
contract must be observed by all index implementations and wrappers, rather
than being an accidental property of one index type.

The immediate compatibility requirement is that existing indices continue to
produce the same features and remain readable.

## Step 2: Literal Features in GCL

Extend GCL with a `|...|` form representing one literal feature. The form is
needed because contextual text features may contain whitespace, punctuation,
or internal Unicode noncharacters and must remain visibly a single GCL term.

The literal form should support a conservative, C/C++-like set of escapes:

- `\\` for backslash and `\|` for the delimiter;
- the familiar named ASCII control escapes, including `\r` and `\t`;
- fixed-width hexadecimal and Unicode escapes;
- an unknown escape drops the backslash, so for example `\q` denotes `q`
  rather than being rejected;
- a trailing unpaired backslash is an error.

A literal newline is rejected, as is an escaped newline. Newline has a deep
meaning in Cottontail text: it prevents token splices during append. NUL is
also excluded for now. Other ASCII controls may be represented; serialization
must use a canonical visible escape, so a literal carriage return is emitted
as `\r`.

The parser and serializer must round-trip literal features. Unicode escapes
must reject surrogate values and values outside the Unicode range.

Contextual textual features occupy a distinct feature type. Internally, a
designated Unicode noncharacter tells the new featurizer to use reversible
textual encoding rather than hashing. In discussion this marker is written as
`#` for visibility, but the implementation will use an actual noncharacter.
The marker is a dispatch/type marker and is not part of the textual payload.
Consequently a typed textual literal that visibly contains `:foo:` is not the
same feature as the structural annotation `:foo:`.

## Step 3: Contextual Byte-Gram Tokenizer and Featurizer

The new representation is provisionally described as *minimum four-byte
contextual grams*. It is not a conventional fixed four-character or four-byte
gram index:

- An annotation starts only at a token boundary.
- Its textual payload contains at least four and at most seven UTF-8 bytes.
- It ends on a Unicode character boundary.
- A token position may carry multiple annotations where variable-width UTF-8
  requires them to preserve possible pattern endings.
- A position with no usable textual annotation is represented in phrase
  construction by the universal feature `-1`, not by a stored posting.

The featurizer can pack up to seven payload bytes reversibly into an `addr`.
The proposed implementation uses a zeroed union of `char[8]` and `addr`, then
fills the byte buffer. The internal textual marker selects this path but does
not consume one of the seven payload bytes. All other features continue to be
hashed into the existing positive hashed-feature namespace.

The dictionary can therefore expand regular expressions over textual
features, while structural annotations such as `:id:` and `:docno:` remain in
a separate hashed namespace and are not accidentally matched.

### Character Classes

Tokenization distinguishes four classes:

- **TOKEN**: a Unicode character that creates a token address and may start a
  contextual annotation.
- **WHITESPACE**: a maximal run of whitespace other than newline. The run is
  one token and contributes a single ordinary space to annotations.
- **IGNORE**: a character such as a presentation selector. It creates no token,
  contributes no annotation bytes, and does not break a whitespace run.
- **BREAK**: a newline. It creates no token and no annotation, terminates
  context, and prevents token splicing.

The exact Unicode membership of these classes remains to be reviewed.
Regular-expression classes such as `\d` are a parser concern and do not belong
in this tokenizer classification.

For example, U+FE0F VARIATION SELECTOR-16 is ignored for annotation purposes.
In `U+2764 U+FE0F`, U+2764 contributes the text and U+FE0F does not. Since
U+2764 is only three UTF-8 bytes, a heart alone has no four-byte textual
feature, but context makes expressions such as `I U+2764 NY` and `A U+2764`
searchable. U+1F916 ROBOT FACE is four bytes and can be represented as a
feature by itself. Ranked retrieval of an isolated three-byte heart is not a
goal.

### Malformed UTF-8

Tokenization must be robust in the presence of malformed input:

- A UTF-8 continuation byte can never begin a token or annotation.
- Every apparent UTF-8 leading byte increments the token count, even when it
  does not begin a correctly formed character. This preserves the ability to
  identify token boundaries by inspecting bytes.
- An annotation contains only complete, correctly formed UTF-8 characters.
- On malformed context, annotation construction stops and tokenization moves
  on; it does not splice text across the malformed sequence.

### Context Selection

Starting at a valid token, collect normalized right context until at least
four payload bytes are available, without crossing a break. Additional
annotations may extend the same start position, up to seven bytes, when they
are needed to represent a possible phrase or expression ending that cannot be
covered by an annotation beginning at the next token.

All-ASCII text normally yields four-byte annotations: one byte from the
current character and three bytes of right context. Three-byte characters
typically combine with right context. For a Chinese sequence such as
`中国人`, the annotation beginning at `中` is `中国` (six bytes), and the one
beginning at `国` is `国人` when that context exists. There is no three-byte
singleton annotation for `中`.

The exact rule deciding which additional annotations are necessary remains
preliminary. Before implementation, work through a table of examples covering:

- ASCII words and punctuation;
- single and repeated whitespace;
- phrase endings and end of text;
- Chinese and mixed-width scripts;
- combining and presentation characters;
- four-byte characters;
- malformed UTF-8; and
- newline boundaries.

The examples must demonstrate both how an input is indexed and how a phrase
against that input is compiled and matched.

### Tokenizer Interface

Replace the overloaded uses of `split` with operations named for their
semantics:

- `tokenize`: emit every annotation required for indexing, including multiple
  annotations at one position;
- `skip`: advance over the next token according to the same scanner;
- `count`: count token positions without manufacturing a feature vector;
- `bow`: return one feature per useful bag-of-words position, provisionally
  selecting the shortest right context and omitting universal positions;
- `phrase`: return the representation best suited to exact phrase expansion,
  provisionally selecting the longest required right context and retaining
  universal positions.

Existing `split` consumers must be classified before changing the interface.
Traditional ranking code normally wants `bow`; phrase expansion wants
`phrase`; token accounting wants `count`. The phrase return type and its
relationship to generated GCL remain open design questions.

## Step 4: Phrase Search

Phrase search is the first restricted form of indexed regular-expression
matching and should establish the compiler boundary used by the general
matcher.

The tokenizer should provide enough information to compile a quoted phrase
into GCL over contextual features and universal positions. Phrase expansion
then uses ordinary positional and containment operators, after which the
existing optimizer may rewrite the result. A phrase such as `rubber` denotes
the complete phrase, not merely one indexed gram.

This step must settle:

- how the phrase extent is represented in generated GCL;
- how leading and trailing universal positions preserve the full phrase;
- how normalized whitespace and ignored characters affect equality;
- how phrase boundaries interact with available right context;
- which feature `phrase` selects when a position has several annotations; and
- how ranking code uses `bow` independently of phrase semantics.

Phrase examples should be evaluated in both directions: compile the phrase,
then walk its generated operators against the annotations produced for
candidate text. This is intended to expose false negatives, accidental token
splices, and boundary errors before general regular expressions add more
moving parts.

## Step 5: Indexed Regular-Expression Matching

The final stage adds regular-expression terms to GCL. Illustrative intended
uses include:

```text
(^ 'foo[dt]' 'bar[ft]')
(+ 'foo[dt]' 'bar[ft]')
```

Quotes distinguish a regular expression from a literal term. A pattern such
as `foo.*bar` should be compiled into positional operations over the useful
literal portions, with the gap represented by ordinary GCL machinery where
possible, rather than forcing a raw-text scan.

The matcher should:

- parse the supported regular-expression language;
- expand applicable textual grams through the Warren dictionary;
- construct an automaton or equivalent matrix representation over posting
  lists;
- retain the largest start address when paths collide in an automaton state;
- emit shortest, nonnested matching intervals as an ordinary hopper;
- compile useful fixed and bounded pieces into existing GCL operations;
- plan from selective posting lists where possible; and
- preserve an exact but potentially expensive universal path for expressions
  with no selective feature.

Regular-expression terms do not cross newline breaks. Anchors `^` and `$` are
not initial goals: collections do not have a single natural notion of a line
or document, and boundary conditions can be expressed through GCL structure.
Regular-expression character classes and escapes are interpreted by the
regular-expression parser, not by the tokenizer.

The first implementation need not cover every conventional regexp feature.
The supported language, behavior of empty matches, repetition bounds, and
resource limits must be made explicit before coding. Exact semantics and
composability are more important than nominal POSIX compatibility.

Selective expressions may run in a few hundred milliseconds over very large
collections. Expressions dominated by universal transitions may still require
work proportional to a large portion of the address space. The universal
feature makes such expressions expressible; it does not make them selective.

## Related Work to Revisit

The implementation should be informed by, and clearly distinguished from:

- Clarke's *An Algebra for Structured Text Search* (1996), for generalized
  concordance lists, shortest-substring semantics, inverted-list execution,
  structured pattern matching, and ranking;
- Clarke and Cormack's *On the Use of Regular Expressions for Searching Text*
  (1995), especially the automaton-state rule retaining the largest start;
- Qiu, Yang, Wang, and Wang's *Efficient Regular Expression Matching Based on
  Positional Inverted Index* (2020/2022), particularly its gram-driven NFA and
  posting-list query plans; and
- n-gram filter-and-verify systems such as Zoekt and workload-selected
  multigram indexes, as useful engineering contrasts rather than the intended
  semantics.

Regular-expression evaluation over an inverted index is not itself a novelty
claim. The relevant design combination is exact positional evaluation with
shortest nonnested interval results, GCL-supplied structure and scope, and
integration with phrase and ranked retrieval.

## Review Gates

Before each implementation step:

1. Review the corresponding contract and examples.
2. Inspect the affected interfaces and enumerate consumers.
3. Decide compatibility and serialization behavior.
4. Agree on focused tests and compile targets.
5. Obtain explicit authorization to code that step only.

The first proposed coding step is limited to the `HashingFeaturizer`
`INT64_MIN` correction and the index-wide feature `-1` convention.
