#!/bin/sh -e

base=/data/tmp/blksum

for image in 20p 40p 80p; do
    for i in 1 2 3; do
        dd if=$base/$image.raw bs=1M of=/dev/null
    done
    hyperfine --parameter-list t 1,2,3,4,5,6,7,8,9,10,11,12 \
        --export-json "mmap-null-$image.json" \
        --prepare "sleep 3" \
        --runs 3 \
        --time-unit second \
        "build/test/mmap-bench -d null -t {t} -a $base/$image.raw"
done
