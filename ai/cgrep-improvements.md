# cgrep Improvements

This file records cases discovered while dogfooding cgrep where another local
search tool was needed. Note the intended search, why cgrep was unsuitable,
and the fallback used. These observations and the opt-in CGREP_LOG workload
collection inform later development; they do not authorize implementation.
Current behavior is recorded in cgrep.md. Matcher development is paused while
shipping Meadowlark takes priority.

## Open Observations

- File discovery: the documentation audit needed a list of ai/ and regexp/
  files and the location of Meadowlark sources. Used rg --files because cgrep
  searches supplied file contents and has no filename-listing or recursive
  discovery mode. Continue using file-discovery tools where appropriate.
- Follow and union queries are candidates for later specialization, after
  indexed regexp execution works. Alternation is common in the current agent
  searches for related names, but this documentation audit is a biased slice
  of the workload. Collect broader searches before choosing optimizations.
- Recursive walking (including symlink/hidden-file policy), parallel file
  search, and bounded-memory input are still proposals, not implemented
  capabilities. Do not infer them from earlier discussion of desired flags.

## Resolved Observations

- Resolved: looking up `Cell` in `regexp/cgrep.cc` originally found exact byte
  offsets but required another tool for complete source lines and line numbers.
  Default `--lines 4` output now reports the lines touched by each match and its
  one-based line-relative byte positions.

- Resolved: raw `Cgrep` advances the Haystack limit when active candidates
  disappear and at inactive chunk boundaries. `LineCgrep` applies the
  corresponding rule while retaining the current line prefix and queued
  reports. Both suppress repeated watermarks instead of making a virtual
  `limit()` call for every inactive byte.

- Resolved: fixed-literal buffer searches now supply enclosing lines and line
  coordinates through BufferLineCgrep, without reverting to bytewise matching.
- Resolved: malformed UTF-8 report text is omitted with binary:true instead
  of producing large Base64 match/line fields. This is per report; valid
  later reports from the same file still carry text. Malformed filenames
  retain their file_base64 fallback.
