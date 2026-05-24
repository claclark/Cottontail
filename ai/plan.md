# Goal: On-Disk Hazel Merge

Implement a Hazel-to-Hazel merge primitive for background shard compaction.
Hazel activation remains standalone, but Hazel merging happens in the larger
Fluffle/Bigwig context and should use `Working` for temporary files and final
publication.

## Public Shape

Add a Working-based merge entry point, roughly:

```
Hazel::merge(working, hazels, parameters, error)
```

where:

- `working` owns names, temporary files, and publication.
- `hazels` are Hazel shard names in sequence order.
- `parameters` is the frozen parameters recipe to place in the output DNA.
  It may be empty for tests.
- The output file is published as the canonical merged shard name:
  `hazel.<first_sequence_start>.<last_sequence_end>`.

The merge should write to `working->make_temp("hazel")`, close a complete file,
link it to the canonical final name, and remove the temp file. On failure,
remove the temp file and leave existing shards untouched. If the final name
already exists, fail rather than overwrite.

## Before Implementation

Review these files before coding:

- `src/fiver.cc`: `Fiver::merge(...)`, `Fiver::hazel(...)`, and the Hazel
  writer helpers near the bottom of the file.
- `src/hazel.cc`: Hazel activation, blob dictionary parsing, idx/txt directory
  loading, inline singleton handling, and text translation.
- `src/simple_posting.cc` and `src/simple_posting.h`:
  `SimplePostingFactory::posting_from_merge(...)`, the exclude-aware overload,
  `SimplePosting::push(...)`, `SimplePosting::write(...)`, and
  `SimplePosting::invariants(...)`.
- `src/working.h` and nearby use sites: temp naming, canonical names, and file
  publication conventions.
- `apps/fiver2hazel.cc`: current standalone conversion flow and how parameters
  are passed into Hazel DNA.

## Input Validation

Filename order is authoritative. Each input name must match:

```
hazel.<sequence_start>.<sequence_end>
```

The filename ranges must be contiguous with no gaps:

```
current.sequence_end + 1 == next.sequence_start
```

Open each Hazel and parse its DNA. The internal `hazel` DNA block must contain
`sequence_start` and `sequence_end`, and those values must exactly match the
sequence numbers from the filename.

After sequence validation, compare parsed DNA across inputs for compatibility.
When comparing DNA, ignore only the expected per-shard or caller-supplied
differences:

- `hazel.sequence_start`
- `hazel.sequence_end`
- `parameters`

Everything else should match, including the Warren type, featurizer package,
tokenizer package, idx recipe, and txt compressor recipe. The txt `chunk_size`
does not need to match if it is separated from the compressor compatibility
check; copied text chunks are self-delimiting in the txt directory. The output
`chunk_size` should be the minimum input chunk size.

## Text Merge

Merge txt blobs by concatenating existing compressed chunks, not by
decompressing and recompressing text.

This should work because Hazel text chunks are independently compressed and the
reader relies on the txt directory boundaries rather than uniform chunk sizes.
For each copied source chunk, append its compressed bytes to the output chunk
space and append a directory entry with cumulative raw and compressed byte ends.

Before implementing Hazel merge, update `Fiver::hazel(...)` so any non-empty
text sealed into a Hazel ends with a separator, adding a trailing newline if
needed. This preserves the existing token-splice prevention hack at the Fiver
boundary and lets Hazel merge concatenate compressed chunks without
decompressing text or inventing separator bytes.

Set output `raw_text_length` to the sum of input raw text lengths. Set output
`target_chunk_size` to the minimum input target chunk size.

Do not insert separator newlines during Hazel merge. Fiver append/merge is
responsible for avoiding token splices before text is sealed into Hazel. Hazel
merge preserves existing compressed chunks and does not invent new raw text
bytes.

## Idx Merge

Treat every input Hazel idx as a sorted stream of `(feature, SimplePosting)`.
Inline Hazel singletons are an on-disk encoding detail only:

- Convert input inline singleton entries to one-record `SimplePosting`s before
  merge.
- Decode non-inline entries to `SimplePosting`s.
- After merge, write the output as an inline singleton only when the final
  posting is exactly one `<p, p, 0>` record. Otherwise write the normal
  compressed `SimplePosting` blob.

Use a balanced priority-queue sweep over input idx directories. Each stream has
one decoded `SimplePosting` at the front. Pop and group all streams with the
same feature, merge that feature, write it to the output idx blob, update the
directory, and advance the contributing streams.

`SimplePostingFactory::posting_from_merge(...)` remains the authority for GCL
posting-list semantics.

## Special Postings

Synthesize special postings first and slip them into the sorted feature stream
at their natural feature positions.

### `null_feature`

Merge all input `null_feature` postings first with:

```
posting_from_merge(postings)
```

If the result is non-empty, write it as the output `null_feature`. Use that
merged posting as the exclusion list for ordinary features.

For ordinary features, merge with:

```
posting_from_merge(postings, exclude)
```

This preserves the Fiver invariant: ordinary postings contained in accumulated
null/exclusion intervals are physically removed from the merged shard.

### `text_chunk_tag`

Build a synthesized `text_chunk_tag` posting by reading each input
`text_chunk_tag` posting in sequence order and adjusting only the value:

```
output_v = input_v + cumulative_raw_text_length_before_this_hazel
```

The token intervals `p, q` are not shifted. Adjusting `v` does not affect GCL
ordering. Do not filter `text_chunk_tag` against `null_feature`, matching
`Fiver::merge(...)`.

When the feature sweep reaches `null_feature` or `text_chunk_tag`, consume and
skip the raw input entries for that feature because their contributions have
already been folded into the synthesized posting.

## Follow-Up Tests

After implementation, add focused regression coverage using small Hazels.
(However, the user adds a note here not to just run off and implement the tests.
The user will supply test files and walk you through implementing the tests.)

- filename and internal sequence mismatch rejection;
- non-contiguous input rejection;
- DNA incompatibility rejection;
- inline singleton input and output behavior;
- `text_chunk_tag` value adjustment and text translation;
- `null_feature` exclusion of ordinary postings;
- ordinary GCL-sensitive interval merge behavior.
