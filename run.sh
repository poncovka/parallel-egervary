#/!bin/sh

FILES="graph_*"

#python ./graph/generator.py test

for P in $FILES ; do
    echo "$P" >> "$1"
    for A in 1 2 3 4 5 6 7 8 9 10 11 12; do
	TIMEFORMAT='%3R'
        { /usr/bin/time -f "%e;%M" -o "$1" -a taskset -c 0-12 ./parallel_v/matching "$P" "$A" >tothroug ; } 
#        { time taskset -c 0-4 ./parallel_v/matching "$P" "$A" >tothroug ; } 
   done
done

