#/!bin/sh

FILES34="graph_100_100_37_100 graph_200_200_37_200 graph_500_500_37_500 graph_606_606_37_606 graph_909_909_37_909 graph_1074_1074_37_1074 graph_1737_1737_37_1737"
FILES66="graph_100_100_66_100 graph_200_200_66_200 graph_500_500_66_500 graph_606_606_66_606 graph_909_909_66_909 graph_1074_1074_66_1074 graph_1737_1737_66_1737"

python ./graph/generator.py test

for P in $FILES34 $FILES66 ; do
    for A in 1 2 5 10 50 100 250 500 1000 2000 5000; do
        echo "$BASE"/"$P" with "$A"  threads
        time ./parallel_v/matching "$P" "$A"
   done
done

