/*
 * Project: GAL 2014 - sequence version of matching in bipartite graphs
 * Author:  Vendula Poncova, xponco00
 * Date:    13.11.2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IFDEBUG(y) //y;
#define DEBUG(x)   //fprintf(stderr, "DEBUG: " x "\n");

enum errors {
  EOK = 0,
  EPARAM,
  EFILE,
  EINPUT,
  EALLOC,
  EQUEUE,
  EUNKNOWN
};

enum colors {
  RED,
  BLUE
};

enum treestat {
  NONE = 0,
  ACTUAL,
  APSTREE
};

typedef struct tGraph TGraph;
typedef struct tTree TTree;
typedef struct tNode TNode;
typedef struct tEdge TEdge;
typedef struct tQueue TQueue;
typedef struct tItem TItem;


struct tGraph {
  int count;
  int size;
  int maxM;
  
  TNode **nodes;
  TTree *trees;
};

struct tTree {
  int status;
  TNode *root;
  TTree *next;
};

struct tNode {
  int id;
  TEdge *edges;
  TEdge *entry;
  TTree *tree;
};

struct tEdge {
  int M;
  TNode *node;
  TEdge *reversed;
  TEdge *next;
};

struct tQueue {
  TItem *first;
  TItem *last;
};

struct tItem {
  void *item;
  TItem *next;
};

int initGraph(TGraph *graph) {

  // init graph
  graph->count = 0;
  graph->size = 100;
  graph->trees = NULL;
  graph->nodes = malloc(graph->size * sizeof(TNode*));
  
  if(graph->nodes == NULL) {
    return EALLOC;
  }
  
  // init array of pointers
  for(int i = 0; i < graph->size; i++) {\
    graph->nodes[i] = NULL;
  }
  
  return EOK;
}

void freeGraph(TGraph *graph) {

  // free nodes
  for(int i=0; i < graph->size; i++) {
    TNode *node = graph->nodes[i];
    
    // free edges
    if (node != NULL) {
      TEdge *edge = graph->nodes[i]->edges;
    
      while (edge != NULL) {
    
        TEdge *old = edge;
        edge = edge->next;
        free(old);
      }
      
      free(node);
    }
  }
  
  free(graph->nodes);
  
  // free trees
  TTree *tree = graph->trees;
  while(tree != NULL) {
    TTree *old = tree;
    tree = tree->next;
    free(old);
  }
}


int addNode(TGraph *graph, int id) {  
  
  // resize the array
  if(id >= graph->size) {
    TNode **old = graph->nodes;
    int oldsize = graph->size;
    graph->size = id * 2;
    graph->nodes = realloc(old, graph->size * sizeof(TNode*));
    
   if(graph->nodes == NULL) {
    return EALLOC;
   }
   
   for (int i = oldsize; i < graph->size; i++ ) {
     graph->nodes[i] = NULL;
   }
  }
  
  // check id
  if(graph->nodes[id] != NULL) {
    return EINPUT;
  }
  
  // create the node
  TNode *node = malloc(sizeof(TNode));

  if(node == NULL) {
    return EALLOC;
   }  

  node->id = id;
  node->entry = NULL;
  node->edges = NULL;
  node->tree = NULL;
  
  // add the node
  graph->nodes[id] = node;
  graph->count++;
    
  return EOK;
}

TNode* getNode(TGraph *graph, int id) {
  if (id >= graph->size) {
    return NULL;  
  }
  
  return graph->nodes[id];
}

int addEdge(TGraph *graph, int idA, int idB) {

  // get nodes
  TNode *A = getNode(graph, idA);
  TNode *B = getNode(graph, idB);
  
  if (idA == idB || A == NULL || B == NULL) {
    return EINPUT;
  }
  
  // allocate edges
  TEdge *edgeAB = malloc(sizeof(TEdge));
  TEdge *edgeBA = malloc(sizeof(TEdge));
  
  if (edgeAB == NULL || edgeBA == NULL) {
    return EALLOC;  
  }
  
  // init edge from A to B
  edgeAB->M = 0;
  edgeAB->node = B;
  edgeAB->reversed = edgeBA;
  edgeAB->next = A->edges;
  A->edges = edgeAB;
  
  // init edge from B to A
  edgeBA->M = 0;
  edgeBA->node = A;
  edgeBA->reversed = edgeAB;
  edgeBA->next = B->edges;
  B->edges = edgeBA;
  
  return EOK;
}

/*
int skipLine(FILE *f) {

  int c;
  while((c = fgetchar(f)) != EOF) {
    if (((char)c) == '\n') break;
  }
  
  return c;
}
*/

int loadGraph(TGraph *graph, FILE *f) {

  initGraph(graph);
  
  int m, x, y;
  
  // read m
  if(fscanf(f, "%d\n", &m) != 1) {
    return EINPUT;
  }
  
  graph->maxM = m;
    
  // read vertices  
  if (fscanf(f,"Vertices") == EOF) {
    return EINPUT;
  }
  
  while (fscanf(f, "%d", &x) == 1) {
    addNode(graph, x);
  }

  // read edges
  if (fscanf(f,"Edges") == EOF) {
    return EINPUT;
  }
  
  while (fscanf(f, "%d %d\n", &x, &y) == 2) {
    if (x < y) {
      addEdge(graph, x, y);
    }
  }
  
  return EOK;
}

void printGraph(TGraph *graph, FILE *f) {

  fprintf(f, "<Graph>\n");
  for(int i = 0; i < graph->size; i++) {
  
    TNode *node = graph->nodes[i];
    if (node != NULL) {
    
      fprintf(f, "Node %d: ", node->id);  

      TEdge *edge = node->edges;      
      while(edge != NULL) {
      
        fprintf(f, "%d[%d] ", edge->node->id, edge->M);  
        edge = edge->next;
      }
      
      fprintf(f, "\n");  
    }
  }
}

void printMatching(TGraph *graph, FILE *f) {

  int M = 0;
  fprintf(f, "<Matching>\n");
  
  for(int i = 0; i < graph->size; i++) {
  
    TNode *node = graph->nodes[i];
    if (node != NULL) {
    
      TEdge *edge = node->edges;      
      while(edge != NULL) {
        
        if (node->id < edge->node->id && edge->M) {      
          fprintf(f, "(%d,%d) ", node->id, edge->node->id); 
          M++;
        }

        edge = edge->next;
      }
    }
  }
  
  if (M != 0) {
    fprintf(f, "\n\n");  
  }
  
  fprintf(f, "%d (=%d)\n", M, graph->maxM);  

}

TTree *createTree(TGraph *graph) {

  // allocate tree
  TTree *tree = malloc(sizeof(TTree));
  if (tree == NULL) {
    return NULL;
  }
  
  // init tree
  tree->status = ACTUAL;
  tree->root = NULL;
  tree->next = graph->trees;
  graph->trees = tree;
  
  return tree;
}


int inAPSTree (TNode *x) {
 return (x->tree != NULL && x->tree->status == APSTREE);
}

void initQueue(TQueue *Q) {
  Q->first = NULL;
  Q->last = NULL;
}

int isEmpty(TQueue *Q) {
  return (Q->first == NULL);
}

int enqueue(TQueue *Q, void *item) {
  
  TItem *qitem = malloc(sizeof(TItem));
  if (qitem == NULL) {
    return EALLOC;
  }
  
  qitem->item = item;
  qitem->next = NULL;
  
  if (Q->first == NULL) {
    Q->first = qitem;
  }

  if (Q->last != NULL) {
    Q->last->next = qitem;
  }
  
  Q->last = qitem;
  
  return EOK;
}

void* dequeue(TQueue *Q) {
  
  if (isEmpty(Q)) {
    return NULL;
  }
  
  void *item = Q->first->item;
  
  TItem *old = Q->first;
  Q->first = Q->first->next;
  
  if (Q->first == NULL) {
    Q->last = NULL;
  }
  
  free(old);
  return item;
}

void freeQueue(TQueue *Q) {

  while(!isEmpty(Q)) dequeue(Q);

}

int _applyAPS(TTree *tree, TQueue *Q) {

  DEBUG("Apply APS.")
  
  int error = EOK;  
  int yM = 0, foundPath = 0;
  TNode *x, *y, *z, *pathEnd = NULL;
  TEdge *xy, *yz;

  // insert root into Q
  DEBUG("Insert root to Q.")
  error = enqueue(Q, (void*) tree->root);
  if (error != EOK) return error;
  
  // while Q is not empty and no path found
  while(!isEmpty(Q) && !foundPath) {

    DEBUG("Get x from Q.")

    // get x
    x = dequeue(Q);    
    xy = x->edges;

    while(xy != NULL && !foundPath) {

      DEBUG("Find y.")

      // get y
      y = xy->node;
      
      if (y->tree == tree || inAPSTree(y)) {
        xy = xy->next;
        continue;
      }

      y->tree = tree;
      y->entry = xy;
      yM = 0;
      
      yz = y->edges;
      while(yz != NULL) {
      
        DEBUG("Find z.")
      
        // get z
        z = yz->node;
        
        if (!(yz->M) || (z->tree == tree) || inAPSTree(z)) {
          yz = yz->next;
          continue;
        }

        z->tree = tree;
        z->entry = yz;
        yM = 1;
        
        // add z to Q
        error = enqueue(Q, (void*) z);
        if (error != EOK) return error;
        
        // get next edge
        yz = yz->next;
      }    
      
      // y is not in M, we found a path
      if (!yM) {
        DEBUG("Found path.")
        
        foundPath = 1;
        pathEnd = y;
      }
      
      // get next edge
      xy = xy->next;      
    }
  }
  
  // we found M-path, change M
  if(foundPath) {
    DEBUG("Process path.")
  
    TNode *u, *v;
    TEdge *uv, *vu;
      
    u = pathEnd;
       
    while (u != tree->root) {
    
      vu = u->entry;
      uv = vu->reversed;
      v  = uv->node;
      
      vu->M = !(vu->M);
      uv->M = !(uv->M);
      
      u = v;
    }
    
    tree->status = NONE;
  }
  
  // we found APS-tree
  else {
    DEBUG("Found APS-tree.")
    tree->status = APSTREE;
  }

  DEBUG("Finished apply APS.")
  return EOK;
}

int applyAPS(TTree *tree) {

  TQueue Q;
  initQueue(&Q);
  
  int error = _applyAPS(tree, &Q);

  freeQueue(&Q);
  return error;
}

int findMatching(TGraph *graph) {

  for (int i = 0, j = 1; i < graph->size && j <= graph->count; i++) {
    
    // get node
    TNode *node = graph->nodes[i];

    if (node != NULL) {
     
      // does it belongs to M?
      TEdge *edge = node->edges;
      while((edge != NULL) && !(edge->M)) {
        edge = edge->next;
      }

      // yes, then skip this node.
      if (edge != NULL) continue;
    
      // create tree
      TTree *tree = createTree(graph);
      if (tree == NULL) return EALLOC;
      
      tree->root = node; 
      node->tree = tree;
      
      // find augmenting path
      int err = applyAPS(tree);
      if (err != EOK) return err;
    
      j++;
    }  
  }

  return EOK;
}

/////////////////////////////////////////////////////
/**
 * Main function
 */
int main (int argc, char *argv[])
{
  int error = EOK;
  
  // check params
  if (argc == 2) {
  
    // open file
    FILE *f = fopen(argv[1], "r");
    if (f != NULL) {
  
      // load graph
      TGraph graph;      
      error = loadGraph(&graph, f);
      if (error == EOK) {
      
        // print graph
        IFDEBUG(printGraph(&graph, stderr))
        
        // find matchinf
        error = findMatching(&graph);
        if (error == EOK) {
        
          // print matching
          printMatching(&graph, stdout);
        }
      }    
      freeGraph(&graph);
      fclose(f);
    }
    else {
      error = EFILE;
    }
  }
  else {
    error = EPARAM;
  }
  
  if (error != EOK) {
    fprintf(stderr, "ERROR %d\n", error);
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}
/* end of file */
