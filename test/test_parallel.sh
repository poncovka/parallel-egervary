#!/bin/bash

# Test the parallel version with the given input 1000 times.

for i in `seq 1 10000` 
do
  
  date=`date`
  echo -en "\r$i $date ";
  
  ./matching ../graph/graph_1000_1000_100_1000 2 >output 2>error
  #./matching ../graph/graph_8_8_55_8 3 >output 2>error
  #./matching ../graph/graph_755_755_47_755 10 >output 2>error

  x=`tail -n 1 output`

  if [ "$x" != "1000" ]; then
    echo "Found one!"
    break
  fi
  
done

echo "End."
