#
# GAL 2014
# Generator of bipartite graphs
#
# Dependencies:
# python 2.7
# python-igraph 0.7
#
# Run:
# python generator.py

import random
from igraph import *

# set max nodes you want to generate
MAXNODES = 2500

# set the prefix of generated files
FILENAME = "graph"

def createGraph(n1, n2, p):
  g = Graph.Random_Bipartite(n1=n1, n2=n2, p=p, directed=False)
  return g
  
def findMaximumMatching(g):
  m = g.maximum_bipartite_matching()
  return m
  
def writeGraph(fname, g, m):

  with open(fname, "w") as f:
  
    f.write("{}\n{}\n".format(g.vcount(), g.ecount()))
        
    for e in g.es:
      f.write("{} {}\n".format(e.source, e.target))

def generateGraph(n1, n2, p):
  
  print "Creating graph with params: {} {} {}".format(n1, n2, p)
  
  g = createGraph(n1, n2, p)
  m = findMaximumMatching(g)
  
  fname = "{}_{}_{}_{}_{}".format(FILENAME, n1, n2, int(p*100), len(m))
  writeGraph(fname, g, m)

def generateRandomGraph():
  n1 = random.randrange(0, MAXNODES) 
  n2 = random.randrange(0, MAXNODES) if random.random() < 0.2 else n1
  p  = random.random() 
  
  generateGraph(n1, n2, p)
  
if __name__ == "__main__":
  generateRandomGraph()

  
