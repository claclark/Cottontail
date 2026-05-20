# Goal: Plan HazelIdx Caching

Next planning target: design the smallest useful HazelIdx posting cache.

The ranker regression after the HazelTxt rebuild is correct but still spends
about 43 minutes on the Hazel shard. The remaining large bottleneck appears to
be repeated HazelIdx posting reads, decompression, and decoding.

Do not start implementation from this note alone. First inspect the current
`HazelIdx` code, the latest timings in `ai/hazel-progress.md`, and the Hazel
format notes in `ai/hazel.md`, then write a concrete plan for the next coding
step.
