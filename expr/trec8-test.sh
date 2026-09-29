#!/bin/sh -v
time ~/Cottontail/Three/bazel-bin/apps/rank --verbose --threads 30 --burrow $1 queries.trec8 stem bm25:depth=20 bm25:b=0.298514 bm25:k1=0.786383 bm25 rsj:depth=19.2284 rsj:expansions=20.8663 rsj:gamma=0.186011 rsj bm25:b=0.362828 bm25:k1=0.711716 bm25:depth=1000 bm25 > temp.rank
~/trec_eval/trec_eval qrels.trec8 temp.rank
