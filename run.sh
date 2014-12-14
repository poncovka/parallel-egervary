#/!bin/sh

FILES="graph_*"

#python ./graph/generator.py test

for P in $FILES ; do
    echo "$P" >> "thistest1"
    for A in 1 2 3 4 5 6 7 8 9 10 11 12; do
	TIMEFORMAT='%3R'
        { time taskset -c 0-4 ./parallel_v/matching "$P" "$A" >tothroug ; } 
	echo ";" >&2 
#        { time taskset -c 0-4 ./parallel_v/matching "$P" "$A" >tothroug ; } 
   done
done

