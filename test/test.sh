#/!bin/sh

#python ./graph/generator.py test

TIMEFORMAT='%3R'
time="time.out"
results="results.out"

echo "TEST `date`" >> $results

for file in ../graph/graph_* ; do
  for maxproc in $(seq 0 11); do
	    for threads in $(seq 1 15); do  
	      
	      
  	    (time taskset -c 0-$maxproc ../parallel_v/matching $file $threads ) >/dev/null 2>$time

        n=`head -n 1 $file`
        e=`head -n 2 $file | tail -n 1`
        t=`head -n 1 $time`
        
        line="$n;$e;$maxproc;$threads;$t;"
        
        echo $line >> $results 
        echo $line
        	    
      done
   done
done

rm -f time.out
