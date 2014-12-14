#/!bin/sh

MAXPROC=12
MAXTHREAD=15 

# make
# python ./graph/generator.py test

(cd ../sequence/ && make)
(cd ../parallel/ && make)

# init
TIMEFORMAT='%3R'
time="time.out"
results="results.out"
echo "TEST `date`" >> $results

# testing
for file in ./files/graph_* ; do

  n=`head -n 1 $file`
  e=`head -n 2 $file | tail -n 1`
  
  (time taskset -c 0-11 ../sequence/matching $file ) >/dev/null 2>$time

  t=`head -n 1 $time`        
  line="SEQUENCE;$n;$e;1;1;$t;"
  echo $line >> $results 
  echo $line
  
  for proc in $(seq 0 $MAXPROC); do
	    for threads in $(seq 1 $MAXTHREAD); do  
	      	      
  	    (time taskset -c 0-$proc ../parallel/matching $file $threads ) >/dev/null 2>$time

        t=`head -n 1 $time`
        line="PARALLEL;$n;$e;$proc;$threads;$t;"   
        echo $line >> $results 
        echo $line
        	    
      done
   done
done

rm -f time.out
