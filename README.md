# Annotative Indexing

A unified approach to indexing that currently supports inverted indices, links, graph structures, JSON, other structured text, dynamic indices, and transactions, with planned support for regular expressions and dense retrieval.

Charles L. A. Clarke. 2025. Annotative Indexing. *Information Retrieval Research* 1, 1 (2025), 109–136. https://doi.org/10.54195/irrj.19910

## Why This May Matter For Agents

Written by Codex.

Annotative indexing is interesting for AI coding and research agents because it
can make large collections of code, text, JSON, and metadata searchable through
one structural model. An agent can translate a natural-language intent into a
precise query, inspect the matching intervals, and then ask for only the local
context it needs, such as the function, file, document span, or annotation that
explains the result.

That changes the role of an expressive query language. Humans may not want to
write detailed structural queries by hand, but agents can generate and refine
them quickly. If the collection records useful annotations like paths, fields,
symbols, spans, provenance, or transactions, an agent can move from vague
requests such as "find where this source was appended" or "show code that
updates this lifecycle" to exact, inspectable evidence in a large corpus.
