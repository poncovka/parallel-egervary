#/!bin/sh

FILES="graph_*"

python ./graph/generator.py test

for P in $FILES ; do
    for A in 1 2 3 4 5 6 7 8 9 10 11 12; do
        echo "$BASE"/"$P" with "$A"  threads
        time taskset -c 0-12 ./parallel_v/matching "$P" "$A"
   done
done

