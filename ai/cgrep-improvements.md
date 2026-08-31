# cgrep Improvements

This file records cases discovered while dogfooding cgrep where another local
text-search tool was needed. Note the intended search, why cgrep was unsuitable,
and the fallback used. These observations are input to later cgrep design and
implementation work.

- Resolved: looking up `Cell` in `regexp/cgrep.cc` originally found exact byte
  offsets but required another tool for complete source lines and line numbers.
  Default `--lines 4` output now reports the lines touched by each match and its
  one-based line-relative byte positions.

- Resolved: raw `Cgrep` now advances the Haystack limit through the current
  offset whenever no active candidate remains. `LineCgrep` applies the
  corresponding rule while retaining the current line prefix and queued
  reports.
