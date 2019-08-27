#!/bin/bash
for i in {0..1}
do
    ((x=$i * 100))
    ./process-vmalloc 0 $(($i + 1)) $((2000 - $i * 500)) $((($i+1) * 4)) \
        $x $x 100 100
done
