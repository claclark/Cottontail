# Goal: Bigwig/Hazel Directory Sanity And Consolidation Plan

This plan records the next staged direction for integrating Hazel into Bigwig.
It is a design checkpoint, not automatic authorization to implement every stage
at once. The user will guide you to implement one step at a time with testing
done by the user at each step. You do compile-only checks.

Current alignment already in place:

- `Owsla` is the narrow Warren subclass for posting-capable shards.
- Fiver and Hazel both subclass `Owsla` and expose
  `std::shared_ptr<SimplePosting> posting(addr feature)`.
- Fluffle owns the Bigwig merged-posting cache generation as an `OwslaCache`.
- Bigwig is not an `Owsla`; it is the aggregator over a started shard snapshot.
- Bigwig currently composes Fiver-only postings and caches only normal
  multi-shard posting merges.
- `text_chunk_tag` is mergeable, but must not be stored in the generic
  `OwslaCache`; Bigwig should merge it fresh or delegate through hoppers.
- Hazel merge sidecars now use the `mrg`/`pst`/`dct` prefixes so
  `working->ls("hazel")` sees only live Hazel candidates and transitional
  old-style sidecar cleanup remains in the merge path.
- User-reported regression measurements are recorded in
  `ai/hazel-progress.md`.

## Core Invariant

The live Bigwig shard set should move toward this logical shape:

```
[ Hazel prefix ][ Fiver suffix ]
```

All reasoning here is sequence-range reasoning. Directory cleanup and
consolidation selection should not depend on text ranges, posting contents, or
physical byte layout.

The normal visible set has no sequence overlap. The tolerated recovery state is
shadowing:

```
dead shard *.A.B is covered by living shard *.X.Y when X <= A && B <= Y
```

For the current conservative mixed boundary, the important expected case is a
newly created Hazel shadowing the Fiver it came from. Startup cleanup may remove
or ignore the dead Fiver if a living Hazel covers the same sequence range.

Any non-shadowed dead shard is a directory sanity failure. Any partial mixed
Hazel/Fiver overlap that cannot be explained as shadowing should be reported as
an error rather than silently activated.

## Step 0: Rename Hazel Merge Sidecars

Live Hazel shard names remain:

```
hazel.A.B
```

Hazel merge intermediates have moved out of the `hazel` prefix so
`working->ls("hazel")` sees only live Hazel candidates. Current names:

```
mrg.hazel.A.B
pst.hazel.A.B
dct.hazel.A.B
```

Meanings:

- `mrg.hazel.A.B`: temporary final Hazel output.
- `pst.hazel.A.B`: resumable idx posting checkpoint.
- `dct.hazel.A.B`: resumable idx directory checkpoint.

On successful publication, link or rename `mrg.hazel.A.B` to `hazel.A.B`, then
remove the associated `mrg`/`pst`/`dct` files.

During the transition, startup cleanup may also recognize and remove old-style
sidecars:

```
hazel.A.B.tmp
hazel.A.B.pst
hazel.A.B.dct
```

once they are known not to be needed.

## Step 1: Replace `fiver_files` With `sanitize`

`fiver_files(...)` should become a directory-normalization pass, tentatively
named `sanitize(...)`.

Its purpose is to restore the working directory to a sane, idempotent startup
state. Activation code should run after `sanitize(...)` and depend on the
conditions it establishes.

`sanitize(...)` should:

- strict-parse live shard names:

  ```
  fiver.A.B
  hazel.A.B
  ```

- build `living` and `dead` lists by sequence range;
- preserve the current Fiver contained-range cleanup behavior;
- apply the same living/dead idea independently for Hazels;
- verify every dead shard is shadowed by a living shard by sequence range:

  ```
  living.start <= dead.start && dead.end <= living.end
  ```

- delete known startup garbage such as `kitten*` and `temp*`;
- scan Hazel merge sidecars under `mrg`, `pst`, and `dct`;
- remove associated merge sidecars when the final `hazel.A.B` already exists;
- keep resumable Hazel merge checkpoints only when their source Hazel group can
  be determined from living Hazels;
- delete sidecars for in-flight Hazel merges whose source group cannot be
  determined;
- return the sanitized live inventory that activation should use.

For an in-flight Hazel merge `hazel.A.B`, the source group should be a
contiguous group of living Hazels whose sequence ranges cover `A..B`. If that
group cannot be identified, the sidecars are not resumable and should be
deleted.

Postconditions:

- strict live Fiver and Hazel names are the only live shard candidates;
- no dead shard remains unless a living shard shadows its sequence range;
- resumable Hazel merge sidecars are associated with identifiable living Hazel
  sources;
- activation can build a coherent snapshot without reinterpreting filesystem
  leftovers.

## Step 2: Activate Hazels With No Hazel Merging

After `sanitize(...)`, Bigwig activation should include live Hazel shards in the
Fluffle/Bigwig read snapshot.

This step should not add Hazel lifecycle policy:

- no Fiver-to-Hazel conversion;
- no Hazel-to-Hazel merge scheduling;
- no mixed physical Fiver/Hazel merge;
- no Fiver retention policy beyond startup shadow cleanup.

Expected implementation shape:

- Fluffle's visible read list moves from Fiver-only toward
  `std::vector<std::shared_ptr<Owsla>>`.
- `BigwigIdx` composes postings from `Owsla` children.
- `BigwigTxt` uses the public Warren/Txt interface available through `Owsla`.
- Bigwig caches only true normal multi-shard posting merges in the captured
  `OwslaCache`.
- `text_chunk_tag` remains uncached.
- `posting(feature)` remains raw storage composition and does not apply
  `null_feature` exclusions.

The cache generation must still change only when the logical visible snapshot
changes.

## Step 3: Convert Fivers To Hazels

Add one-at-a-time Fiver-to-Hazel conversion at the Hazel/Fiver boundary.

Rules:

- Hazels are always older than Fivers in the live logical order.
- The boundary Fiver is the oldest live Fiver, immediately after the Hazel
  prefix.
- Only a boundary Fiver is eligible for conversion.
- A converted Hazel may temporarily shadow the source Fiver on disk.
- Startup `sanitize(...)` resolves that recovery state by making the Hazel
  visible and treating the shadowed Fiver as dead.
- Hazels may pile up; this step does not require Hazel-to-Hazel merging.

This advances:

```
[ Hazel prefix ][ Fiver suffix ]
```

without introducing arbitrary mixed physical merges.

## Step 4: Consolidation Worker Policy

Merge/consolidation workers must constantly respect `fluffle->merging`.
Selection should scan for available work rather than stop at the first blocked
candidate.

Eligibility:

- Candidate sources must be adjacent/sequential by sequence range.
- Every source Warren in the candidate range must not already be in
  `fluffle->merging`.
- Source Warrens must be marked in `fluffle->merging` while holding the Fluffle
  lock, before the worker releases the lock and starts work.
- If an early candidate is blocked by `fluffle->merging`, keep scanning for
  another eligible candidate.

Hazel merging should be capped simply. Before doing any Hazel merge work, ask:

```
Are fewer than N Hazel shards currently in fluffle->merging?
```

If the Hazel cap is reached, skip both resumed Hazel merges and new
Hazel/Hazel merges. The worker may still do Fiver work.

Worker priority:

1. If Hazel merging is currently allowed, consult the sanitized list of
   in-flight Hazel merges left over from the past. If a resumable merge has
   available source Hazels, claim it and continue it.
2. Merge tiny Fivers, as current Bigwig consolidation does.
3. If the boundary Fiver is over the conversion limit, convert it to Hazel.
4. Merge the smallest available adjacent Fiver/Fiver pair not considered tiny.
5. If Hazel merging is currently allowed, merge the smallest available adjacent
   Hazel/Hazel pair.
6. If every pass finds no eligible work, quit.

The ordering favors fast transactions and hot-suffix cleanup over long cold
Hazel compaction. Hazel merge recovery comes first only when the Hazel cap
allows it.

Publication rules:

- A completed operation replaces the visible Fluffle shard set atomically under
  the Fluffle lock.
- A changed visible shard set publishes a new empty `OwslaCache` generation.
- Background consolidation is query-semantics preserving. If consolidation
  changes query answers, that is a consolidation bug, not a cache policy issue.

## Step 5: Survive `apps/trec-example`

The user will run this and all other testing. You only run compiles.

`apps/trec-example.cc` is the stress model for the eventual behavior:

- many writer clones ingest and annotate files;
- ranking clones repeatedly `start()` / rank / `end()` while writes continue;
- eraser clones delete old file ranges through transactions;
- commits publish new visible snapshots while rankers are active;
- the filesystem sees kittens, temp files, Fivers, Hazels, sidecars, and
  consolidation products.

The end-state goal is that the filesystem may be busy, but:

- every started reader sees a coherent snapshot;
- every commit that changes visibility publishes a fresh cache generation;
- restart converges through `sanitize(...)`;
- no sequence range is double-counted;
- no sequence range is lost when dead files are removed;
- leftover Hazel merge sidecars are either resumable or deleted.

## Verification

Follow the repository rule: compile/build checks only unless the user
explicitly asks for runtime experiments, ranking runs, evals, or benchmarks.

Likely compile checks:

```sh
bazel build //src:cottontail
bazel build //test:tests
bazel build //test:hazel_test
bazel build //apps:trec-example
```

Do not run `bazel test`, ranking, evals, benchmarks, or `apps/trec-example`
unless explicitly requested.

## Useful Starting Points

- `src/bigwig.cc`: current `fiver_files(...)`, activation, start snapshots, and
  merge workers.
- `src/fluffle.h`: visible shard list, merge state, cache generation.
- `src/hazel.cc`: Hazel merge sidecar names, checkpoint repair, and publication.
- `src/owsla.h`: `Owsla`, `OwslaCache`, Hazel/Fiver shared naming helpers.
- `src/fiver.cc` / `src/fiver.h`: Fiver activation, merge, conversion source.
- `apps/fiver2hazel.cc`: strict shard-name parsing and existing Hazel merge
  command behavior.
- `apps/trec-example.cc`: concurrent write/read/delete stress model.
- `test/hazel.cc`: focused Hazel regression coverage.
