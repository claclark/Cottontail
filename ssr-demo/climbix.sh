#!/bin/sh

# Build the twelve ClimbMix meadows, consolidating each before the next build.
set -eu

usage() {
    echo "usage: $0 source-directory target-directory"
}

if [ "$#" -eq 1 ] && [ "$1" = "--help" ]; then
    usage
    exit 0
fi
if [ "$#" -ne 2 ]; then
    usage >&2
    exit 1
fi

for program in meadowlark consolidate; do
    if ! command -v "$program" >/dev/null 2>&1; then
        echo "$0: executable not found on PATH: $program" >&2
        exit 1
    fi
done

source_dir=$(CDPATH= cd -- "$1" && pwd)
mkdir -p -- "$2"
target_dir=$(CDPATH= cd -- "$2" && pwd)

# Initialize every missing meadow before starting ingestion.
for shard in 00 01 02 03 04 05 06 07 08 09 10 11; do
    output="$target_dir/$shard.meadow"
    if [ ! -e "$output" ] && [ ! -L "$output" ]; then
        echo "Creating $output"
        meadowlark --meadow "$output" --create
    fi
done

build_shard() {
    output="$target_dir/$1.meadow"
    shift
    # An unmatched shell glob remains literal and fails this check.
    for input in "$@"; do
        if [ ! -f "$input" ] || [ ! -r "$input" ]; then
            echo "$0: missing or unreadable input: $input" >&2
            exit 1
        fi
    done
    # Meadowlark skips committed sources and recovers interrupted ingestion.
    echo "Ingesting $output"
    meadowlark --meadow "$output" --jsonl "$@"
    echo "Consolidating $output"
    consolidate --verbose "$output"
}

build_shard 00 \
    "$source_dir"/shard_00[01234]*.jsonl.gz \
    "$source_dir"/shard_005[0123]*.jsonl.gz \
    "$source_dir"/shard_0054[012345].jsonl.gz

build_shard 01 \
    "$source_dir"/shard_0054[6789].jsonl.gz \
    "$source_dir"/shard_005[56789]*.jsonl.gz \
    "$source_dir"/shard_00[6789]*.jsonl.gz \
    "$source_dir"/shard_010[012345678]*.jsonl.gz \
    "$source_dir"/shard_0109[01].jsonl.gz

build_shard 02 \
    "$source_dir"/shard_0109[23456789].jsonl.gz \
    "$source_dir"/shard_01[12345]*.jsonl.gz \
    "$source_dir"/shard_016[012]*.jsonl.gz \
    "$source_dir"/shard_0163[01234567].jsonl.gz

build_shard 03 \
    "$source_dir"/shard_0163[89].jsonl.gz \
    "$source_dir"/shard_016[456789]*.jsonl.gz \
    "$source_dir"/shard_01[789]*.jsonl.gz \
    "$source_dir"/shard_020*.jsonl.gz \
    "$source_dir"/shard_021[01234567]*.jsonl.gz \
    "$source_dir"/shard_0218[012].jsonl.gz

build_shard 04 \
    "$source_dir"/shard_0218[3456789].jsonl.gz \
    "$source_dir"/shard_0219*.jsonl.gz \
    "$source_dir"/shard_02[23456]*.jsonl.gz \
    "$source_dir"/shard_027[01]*.jsonl.gz \
    "$source_dir"/shard_0272[01234567].jsonl.gz

build_shard 05 \
    "$source_dir"/shard_0272[89].jsonl.gz \
    "$source_dir"/shard_027[3456789]*.jsonl.gz \
    "$source_dir"/shard_02[89]*.jsonl.gz \
    "$source_dir"/shard_03[01]*.jsonl.gz \
    "$source_dir"/shard_032[0123456]*.jsonl.gz \
    "$source_dir"/shard_0327[012].jsonl.gz

build_shard 06 \
    "$source_dir"/shard_0327[3456789].jsonl.gz \
    "$source_dir"/shard_032[89]*.jsonl.gz \
    "$source_dir"/shard_03[34567]*.jsonl.gz \
    "$source_dir"/shard_0380*.jsonl.gz \
    "$source_dir"/shard_0381[01234567].jsonl.gz

build_shard 07 \
    "$source_dir"/shard_0381[89].jsonl.gz \
    "$source_dir"/shard_038[23456789]*.jsonl.gz \
    "$source_dir"/shard_039*.jsonl.gz \
    "$source_dir"/shard_04[012]*.jsonl.gz \
    "$source_dir"/shard_043[012345]*.jsonl.gz \
    "$source_dir"/shard_0436[012].jsonl.gz

build_shard 08 \
    "$source_dir"/shard_0436[3456789].jsonl.gz \
    "$source_dir"/shard_043[789]*.jsonl.gz \
    "$source_dir"/shard_04[45678]*.jsonl.gz \
    "$source_dir"/shard_0490[01234567].jsonl.gz

build_shard 09 \
    "$source_dir"/shard_0490[89].jsonl.gz \
    "$source_dir"/shard_049[123456789]*.jsonl.gz \
    "$source_dir"/shard_05[0123]*.jsonl.gz \
    "$source_dir"/shard_054[01234]*.jsonl.gz \
    "$source_dir"/shard_0545[012].jsonl.gz

build_shard 10 \
    "$source_dir"/shard_0545[3456789].jsonl.gz \
    "$source_dir"/shard_054[6789]*.jsonl.gz \
    "$source_dir"/shard_05[5678]*.jsonl.gz \
    "$source_dir"/shard_059[012345678]*.jsonl.gz \
    "$source_dir"/shard_0599[01234567].jsonl.gz

build_shard 11 \
    "$source_dir"/shard_0599[89].jsonl.gz \
    "$source_dir"/shard_06[01234]*.jsonl.gz \
    "$source_dir"/shard_065[0123]*.jsonl.gz \
    "$source_dir"/shard_0654[012].jsonl.gz
