# Annotative Indexing — Architectural Summary

Source:
Charles L. A. Clarke, *Annotative Indexing*, arXiv:2411.06256.
Reference implementation: https://github.com/claclark/Cottontail

## Purpose

Annotative indexing is a general indexing framework designed to unify several
data management and retrieval paradigms under a single abstraction.

Traditional systems tend to use separate technologies for:

* text search (inverted indexes)
* relational analytics (column stores)
* object storage
* graph databases
* semi-structured document stores

Annotative indexing proposes that these systems are all variations of the
same underlying idea and can be implemented using a common indexing model.
The framework therefore provides a single system capable of supporting ranked
retrieval, structured queries, graph queries, and heterogeneous document
collections. ([ResearchGate][1])

## Core Concept

The fundamental unit of the system is an **annotation**.

An annotation is a triple:

```
<feature, interval, value>
```

where

* feature: a label describing what is being indexed
* interval: a location in the underlying content
* value: associated data such as weights or attributes

The indexed collection consists of:

```
content
+ annotations describing properties of that content
```

Rather than transforming data into multiple derived collections,
all processing steps simply **add annotations to the original content**.

## Minimal Interval Semantics

Annotations follow minimal-interval semantics.

For a given feature:

* annotations cannot contain one another
* annotations may overlap
* annotations are totally ordered by their start position

These properties allow efficient query processing using structural operators
such as containment, proximity, ordering, and boolean combination. ([arXiv][2])

## Query Processing

Query processing operates over lists of annotations.

Two core access primitives are used:

```
τ (tau) – find the next annotation beginning at or after a position
ρ (rho) – find the next annotation ending at or after a position
```

Complex query operators (boolean, containment, proximity, etc.) are
implemented by combining these primitives.

The resulting algorithms can skip large parts of annotation lists and
often run in time close to the number of actual results rather than
the total index size.

## Relationship to Existing Systems

Annotative indexing generalizes several well-known storage models.

Examples:

Inverted index
features = terms
intervals = token positions

Column store
features = columns
intervals = rows

Knowledge graph
features = relationship types
intervals = nodes or spans

JSON / semi-structured store
features = field names

Thus a single annotative index can support multiple data models and
query paradigms simultaneously. ([ResearchGate][1])

## Dynamic Update and Transactions

The framework supports fully dynamic collections with concurrent readers
and writers and can implement transactional semantics.

This allows indexing systems that continuously evolve rather than requiring
periodic complete rebuilds of the index. ([ResearchGate][1])

## Reference Implementation: Cottontail

Cottontail is the reference implementation of annotative indexing.

The implementation includes:

* static and dynamic index structures
* components for tokenization and feature extraction
* annotation processing and query evaluation
* support for transactions and concurrent access

The architecture is intentionally modular: a central "Warren" object manages
the major system components and index structures.

## Mental Model for Developers

Think of the system as:

```
collection of objects
    +
layers of annotations describing structure and features
    +
indexes over those annotations
```

Most indexing or data-processing steps simply add new annotations rather
than creating new collections.

All query operations ultimately operate over annotation lists.

## Key Takeaway

Annotative indexing treats indexing as the management and querying of
annotations attached to content. By expressing different data models
in terms of annotations, a single indexing framework can support
traditional search engines, structured data queries, graph queries,
and heterogeneous document collections.

[1]: https://www.researchgate.net/publication/385722142_Annotative_Indexing?utm_source=chatgpt.com "(PDF) Annotative Indexing"
[2]: https://arxiv.org/html/2411.06256v3?utm_source=chatgpt.com "Annotative Indexing"

