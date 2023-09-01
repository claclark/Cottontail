#!/bin/sh -v
TEST=/home/claclark/Cottontail/Three/bazel-bin/apps/dynamic-test
QUERIES=/data/hdd3/claclark/queries.trec8
QRELS=/data/hdd3/claclark/qrels.trec8
TREC4=/home/claclark/Cottontail/TREC-disks/trec/trec_disk_4
TREC5=/home/claclark/Cottontail/TREC-disks/trec/trec_disk_5

$TEST $QUERIES $QRELS $TREC4/fr94 $TREC4/ft $TREC5/fbis $TREC5/latimes
#$TEST $QUERIES $QRELS $TREC4/ft/ft911
