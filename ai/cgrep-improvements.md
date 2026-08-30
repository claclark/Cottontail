# cgrep Improvements

This file records cases discovered while dogfooding cgrep where another local
text-search tool was needed. Note the intended search, why cgrep was unsuitable,
and the fallback used. These observations are input to later cgrep design and
implementation work.

- Looking up `Cell` in `regexp/cgrep.cc` found exact byte offsets, but explaining
  the data structure required surrounding source lines and line numbers. Used
  `nl` and `sed` after cgrep because cgrep does not yet provide bounded context
  around a match or report source line numbers.

- `Cgrep` correctly defers `Haystack::limit(p)` for a returned match until the
  caller's next `match()` call, but it does not advance the limit when the
  active-state set becomes empty without a match. This is harmless for the
  current whole-input Haystack, whose limit is only a watermark, but a future
  bounded or rolling Haystack could retain an entire low-match input. Advance
  the limit through the current offset whenever no active candidate remains.
