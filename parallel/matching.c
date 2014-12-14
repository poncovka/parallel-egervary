/*
 * Project: GAL 2014 - parallel version of matching in bipartite graphs
 * Authors: Vendula Poncova, xponco00
 *          Chernikava Alena, xcerni0700
 * Date:    4.12.2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>

#define DEBUG(y) //y;

//------------------------------------------------------------------- ENUMS

enum errors {
  EOK = 0,
  EPARAM,
  EFILE,
  EINPUT,
  EALLOC,
  EQUEUE,
  EINTERN,
  EUNKNOWN
};

enum colours {
  RED = 0,
  BLUE,
  GREEN,
  WHITE
};

enum treestat {
  FREE = 0,
  INPROCESS,
  HASPATH,
  APSTREE
};

enum retstat {
  OK = 0,
  ABORT,
  IGNORE,
  CONFLICT,
  PATH
};

//------------------------------------------------------------------- TYPES

typedef struct tGraph TGraph;
typedef struct tTree TTree;
typedef struct tNode TNode;
typedef struct tEdge TEdge;

typedef struct tList TList;
typedef struct tQueue TQueue;
typedef struct tItem TItem;

typedef pthread_t TThread;
typedef pthread_mutex_t TMutex;
typedef struct tThreadData TThreadData;

struct tList {
  TItem *last;
};

struct tQueue {
  TItem *first;
  TItem *last;
};

struct tItem {
  void *item;
  TItem *next;
};

struct tGraph {
  int n;
  int m;
  TNode *nodes;
  
  int ntree;
  TMutex mutex;
};

struct tTree {
  int id;
  int status;
  int owner; 

  TNode *root;
  TNode *pathEnd;
  
  TList nodes;
  TMutex mutex;
};

struct tNode {
  int id;
  int colour;

  TEdge *edges;
  TEdge *entry;
  TTree *tree;
  TMutex mutex;
};

struct tEdge {
  int M;
  TNode *node;
  TEdge *reversed;
  TEdge *next;
};

struct tThreadData {
  int id;
  int error;
  TGraph *graph;
  TQueue *queue;
  TMutex *mutex;
};

//------------------------------------------------------------------- PRINT

void msg(char *format, int id, ...)
{
   va_list args;
   va_start(args, id);
   fprintf(stderr, "DEBUG[%d] ", id);
   vfprintf(stderr, format, args);
   fprintf(stderr, "\n");
   va_end(args);
}

void msgt(char *format, TTree *t, ...)
{
   va_list args;
   va_start(args, t);
   fprintf(stderr, "DEBUG[%d:%d] ", t->owner, t->id);
   vfprintf(stderr, format, args);
   fprintf(stderr, "\n");
   va_end(args);
}

//------------------------------------------------------------------- LIST

void initList(TList *L) {
  L->last = NULL;
}

int isEmptyList(TList *L) {
  return (L->last == NULL);
}

int pushList(TList *L, void *item) {

  TItem *litem = malloc(sizeof(TItem));
  
  if (litem == NULL) {
    return EALLOC;
  }

  litem->item = item;
  litem->next = L->last;
  L->last = litem;
  
  return EOK;
}

void* popList(TList *L) {

  void *item = L->last->item; 
  
  TItem *litem = L->last;
  L->last = litem->next;
  free(litem);
  
  return item;
}

void freeList(TList *L) {
  while(!isEmptyList(L)) popList(L);
}

//------------------------------------------------------------------- QUEUE

void initQueue(TQueue *Q) {
  Q->first = NULL;
  Q->last = NULL;
}

int isEmptyQueue(TQueue *Q) {
  return (Q->first == NULL);
}

int pushQueue(TQueue *Q, void *item) {
  
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

void* popQueue(TQueue *Q) {
  
  if (isEmptyQueue(Q)) {
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
  while(!isEmptyQueue(Q)) popQueue(Q);
}

//------------------------------------------------------------------- GRAPH

int initGraph(TGraph *graph, int n) {

  // init graph
  graph->n = n;
  graph->m = 0;
  graph->ntree = 0;
  graph->nodes = malloc(n * sizeof(TNode));

  if(graph->nodes == NULL) {
    return EALLOC;
  }

  // init graph mutex
  pthread_mutex_init(&(graph->mutex), NULL); 
  
  // init nodes
  TNode *node = NULL;
  for(int i = 0; i < n; i++) {
  
    node = &(graph->nodes[i]);
    node->id = i;
    node->colour = WHITE;
    node->edges = NULL;
    node->entry = NULL;
    node->tree = NULL;   
    
    pthread_mutex_init(&(node->mutex), NULL); 
  }
  
  return EOK;
}

//-------------------------------------------------------------------

void freeGraph(TGraph *graph) {

  // free edges  
  for(int i=0; i < graph->n; i++) {
    TNode *node = &(graph->nodes[i]);
    TEdge *edge = node->edges;
    
    while (edge != NULL) {
      TEdge *old = edge;
      edge = edge->next;
      free(old);
    }
    
    pthread_mutex_destroy(&(node->mutex));
  }
  
  // free nodes
  free(graph->nodes);
  // free mutex
  pthread_mutex_destroy(&(graph->mutex));  
}

//-------------------------------------------------------------------

int addEdge(TGraph *graph, int idA, int idB) {

  // check input
  if (idA == idB || idA >= graph->n || idB >= graph->n) {
    return EINPUT;
  }

  // get nodes
  TNode *A = &(graph->nodes[idA]);
  TNode *B = &(graph->nodes[idB]);
    
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
  
  // increment number of edges
  graph->m++;
  
  return EOK;
}

//-------------------------------------------------------------------

int loadGraph(TGraph *graph, FILE *f) {

  // init
  int n = 0, m = 0, x = 0, y = 0;
  
  // read numbers of vertices and edges
  if(fscanf(f, "%d %d", &n, &m) != 2) {
    return EINPUT;
  }

  // init graph
  initGraph(graph, n);
  
  // read edges
  for (int i = 1; i <= m; i++) {
  
    if(fscanf(f, "%d %d", &x, &y) != 2) {
      return EINPUT;
    }
    
    addEdge(graph, x, y);
  }
    
  return EOK;
}

//-------------------------------------------------------------------

void printGraph(TGraph *graph, FILE *f) {

  fprintf(f, "<Graph>\n");
  for(int i = 0; i < graph->n; i++) {
  
    TNode *node = &(graph->nodes[i]);
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

//-------------------------------------------------------------------

void printMatching(TGraph *graph, FILE *f) {

  int M = 0;
  fprintf(f, "<Matching>\n");
  
  // print edges in matching
  for(int i = 0; i < graph->n; i++) {
  
    TNode *node = &(graph->nodes[i]);
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
  
  fprintf(f, "<Nodes>\n%d\n\n", graph->n);
  fprintf(f, "<Edges>\n%d\n\n", graph->m);
  fprintf(f, "<Trees>\n%d\n\n", graph->ntree);
  fprintf(f, "<M>\n%d\n", M);  
}

//------------------------------------------------------------------- SYNC

void lockNode(TNode *node) {
  pthread_mutex_lock(&(node->mutex));
}

void unlockNode(TNode *node) {
  pthread_mutex_unlock(&(node->mutex));
}

void lockTree(TTree *tree) {
  pthread_mutex_lock(&(tree->mutex));
}

void unlockTree(TTree *tree) {
  pthread_mutex_unlock(&(tree->mutex));
}

void lockNodes(TNode *nodeA, TNode *nodeB) {

  if (nodeA->id < nodeB->id) {
    pthread_mutex_lock(&(nodeA->mutex));
    pthread_mutex_lock(&(nodeB->mutex));
  }
  else {
    pthread_mutex_lock(&(nodeB->mutex));
    pthread_mutex_lock(&(nodeA->mutex));  
  }
}

void lockTrees(TTree *treeA, TTree *treeB) {

  if (treeA->id < treeB->id) {
    pthread_mutex_lock(&(treeA->mutex));
    pthread_mutex_lock(&(treeB->mutex));
  }
  else {
    pthread_mutex_lock(&(treeB->mutex));
    pthread_mutex_lock(&(treeA->mutex));  
  }
}

//------------------------------------------------------------------- TREE

TTree *createTree(TGraph *graph) {

  // allocate tree
  TTree *tree = malloc(sizeof(TTree));
  if (tree == NULL) {
    return NULL;
  }
  
  // init tree
  tree->status = INPROCESS;
  tree->root = NULL;
  tree->owner = 0;
  tree->pathEnd = NULL;
  
  // init list of nodes
  initList(&(tree->nodes));
  
  // init mutex
  pthread_mutex_init(&(tree->mutex), NULL); 

  // critical section
  pthread_mutex_lock(&(graph->mutex));  
  tree->id = graph->ntree++;    
  pthread_mutex_unlock(&(graph->mutex));  
  
  return tree;
}

//-------------------------------------------------------------------

void freeTree(TTree *tree) {

  // free list of nodes
  freeList(&(tree->nodes));
  
  // free tree
  pthread_mutex_destroy(&(tree->mutex));
  free(tree);  
}

//------------------------------------------------------------------- 

void colourNodes(TTree *tree, int colour) {
  DEBUG(msgt("Colour nodes.", tree))

  TNode *node = NULL;
  TList *L = &(tree->nodes);
  
  while(!isEmptyList(L)) {
  
    node = popList(L);  
    lockNode(node);

    if (node->tree == tree) {
      node->colour = colour;
      node->tree = NULL;
      node->entry = NULL;
    }
    
    unlockNode(node);
  }
}

//------------------------------------------------------------------- MATCHING

int inM(TNode *node) {

  TEdge *edge = node->edges;     
  while((edge != NULL) && !(edge->M)) {
    edge = edge->next;
  }
  
  return (edge != NULL);
}

//------------------------------------------------------------------- 

void changeM(TEdge *edge) {

  edge->M = !(edge->M);
  edge->reversed->M = !(edge->reversed->M);
}

//-------------------------------------------------------------------

void processPath(TNode *end) {

  TNode *u, *v;
  TEdge *uv, *vu;
    
  u = end;

  while (u->entry != NULL) {

    vu = u->entry;
    uv = vu->reversed;
    v  = uv->node;
    
    changeM(uv);
    u = v;
    
  }
}

//------------------------------------------------------------------- ADD NODE TO TREE

void _addNodeToTree(TTree *tree, TNode *node, TEdge *edge, int colour) {
  
  node->tree = tree;
  node->entry = edge;
  node->colour = colour;
  
  pushList(&(tree->nodes), node);
}

//-------------------------------------------------------------------


int addNodeToTree(TTree *treeA, TNode *nodeA, TNode *nodeB, TEdge *AB, int M) {

  DEBUG(msgt("Try add node B %d to node A %d.", treeA, nodeB->id, nodeA->id))

  // init
  int status = OK;
  int hasPath = 0;
  int colour = M ? RED : BLUE;  
  
  TTree *treeB = NULL;
  
  // lock nodes
  lockNodes(nodeA, nodeB);
    
  // lock tree A and check if it has path
  lockTree(treeA);
  hasPath = (treeA->status == HASPATH);
  unlockTree(treeA);
  
  // has path
  if(hasPath) {
    DEBUG(msgt("PATH: Has path.", treeA))
    status = PATH;
  }
  // same trees
  else if (nodeA->tree == nodeB->tree) {
    DEBUG(msgt("IGNORE: Same trees.", treeA))
    status = IGNORE;
  }
  // check matching
  else if (AB->M != M) {
    DEBUG(msgt("IGNORE: Wrong type of edge.", treeA))
    status = IGNORE;  
  }
  // APS tree
  else if (nodeB->colour == GREEN) {
    DEBUG(msgt("IGNORE: The node %d is in APS tree.", treeA, nodeB->id))
    status = IGNORE;   
  }
  // FREE tree
  else if (nodeB->colour == WHITE) {
    DEBUG(msgt("OK: The node %d is free.", treeA, nodeB->id))
    
    lockTree(treeA);
    _addNodeToTree(treeA, nodeB, AB, colour);
    unlockTree(treeA);
    
    status = OK; 
  }
  // INPROCESS tree - found path
  else if (nodeB->colour == nodeA->colour) {
      
    treeB = nodeB->tree;
    lockTrees(treeA, treeB);
    
    if (treeA->status == INPROCESS && treeB->status == INPROCESS) {
      DEBUG(msgt("PATH: Found path.", treeA))
      
      treeA->status = HASPATH;
      treeA->pathEnd = nodeA;  
      treeB->status = HASPATH;
      treeB->pathEnd = nodeB;
      changeM(AB);
      status = PATH;
    }
    else {
      DEBUG(msgt("IGNORE: Tree A is in conflict.", treeA))
      status = CONFLICT;
    }
    
    unlockTree(treeA);
    unlockTree(treeB);
  
  }
  // INPROCESS tree - conflict
  else {
    DEBUG(msgt("IGNORE: Tree A is in conflict.", treeA))
    status = CONFLICT;      
  }
  
  unlockNode(nodeA);
  unlockNode(nodeB);

  DEBUG(msgt("End of add node to tree.", treeA))
  return status;
}

//------------------------------------------------------------------- APPLY APS

int _applyAPS(TTree *tree, TQueue *Q, int *ptrStatus) {

  DEBUG(msgt("Apply APS for root %d.", tree, tree->root->id))

  int M = 0; 
  int error = EOK; 
  int status = OK;
  int colour = WHITE;
  
  TNode *x, *y, *z, *pathEnd = NULL;
  TEdge *xy, *yz;

  // insert root into Q
  error = pushQueue(Q, (void*) tree->root);
  
  if (error != EOK)
    return error;
  
  // process the queue
  while(!isEmptyQueue(Q) && status == OK && error == EOK) {

    // get x
    x = popQueue(Q);    
    xy = x->edges;
    
    while(xy != NULL && status == OK && error == EOK) {

      // get y
      y = xy->node;
      status = addNodeToTree(tree, x, y, xy, 0);
      
      // try next edge
      if (status == IGNORE || status == CONFLICT) {
        xy = xy->next;
        status = OK;
      }      
      // new y
      else if (status == OK) {
        DEBUG(msgt("Added new y=%d.", tree, y->id));      
        
        M = 0;      
        yz = y->edges;
      
        while(yz != NULL && status == OK && error == EOK) {
            
          // get z
          z = yz->node;
          status = addNodeToTree(tree, y, z, yz, 1);

          // try next edge
          if (status == IGNORE) {
            yz = yz->next;
            status = OK;
          }
          // node in conflict, try next y
          else if (status == CONFLICT) {
            status = OK;
            yz = NULL;
            M++;
          }
          // new z
          else if (status == OK) {
            DEBUG(msgt("Added new z=%d.", tree, z->id));
            
            error = pushQueue(Q, (void*) z);
            M++;
            break;
          }
        }    
      
        // found path from y
        if (status == OK && !M) {  
          DEBUG(msgt("Found path, y is not in M.", tree))
          pathEnd = y;
          status = PATH;
        }
      
        // try next edge xy
        xy = xy->next;      
      }
    }
  }

  // process results  
  lockTree(tree);
  
  if (status == PATH) {
    DEBUG(msgt("Processing path.", tree))
    
    if (tree->status == HASPATH) {
      pathEnd = tree->pathEnd;
    }

    tree->status = HASPATH;
    unlockTree(tree);
    
    processPath(pathEnd);
    colour = WHITE;
    status = OK;
  }
  else {
    
    DEBUG(msgt("Found APS tree.", tree))
    tree->status = APSTREE;
    colour = GREEN;
    status = OK;
       
    unlockTree(tree);
  }

  // recolour nodes
  colourNodes(tree, colour);
  
  // set status
  *ptrStatus = status;
  
  DEBUG(msgt("End of apply APS.", tree))
  return error;
}

int applyAPS(TTree *tree, int *status) {

  TQueue Q;
  initQueue(&Q);
    
  int error = _applyAPS(tree, &Q, status);

  freeQueue(&Q);
  return error;
}

//------------------------------------------------------------------- FIND MATCHING

int _findMatching(TGraph *graph, TQueue *Q, TMutex *qmutex, int id) {

  int error = EOK;
  int status = OK;
  
  while(error == EOK) {
    
    // lock the queue
    DEBUG(msg("Get new root node.", id))
    pthread_mutex_lock(qmutex);
    
    // is queue empty?
    if(isEmptyQueue(Q)) {
      DEBUG(msg("Root node queue is empty.", id))
      pthread_mutex_unlock(qmutex);
      break;
    }
    
    else {
    
      // get root node
      TNode *node = popQueue(Q);
      
      // unlock the queue
      pthread_mutex_unlock(qmutex);
      
      // lock the node
      lockNode(node);

      if (node->colour == WHITE) {
        if (!inM(node)) {
        
          // create tree
          TTree *tree = createTree(graph);
          if (tree == NULL) {
            unlockNode(node);
            return EALLOC;
          }
          
          // lock the tree
          lockTree(tree);
      
          tree->root = node; 
          tree->owner = id;
          
          _addNodeToTree(tree, node, NULL, RED);
          
          // unlock the tree     
          unlockTree(tree);
          
          // unlock the node
          unlockNode(node);
      
          // find augmenting path
          error = applyAPS(tree, &status);      
          
          // free nodes in tree and tree     
          freeTree(tree);     
        }
        else {
          DEBUG(msg("Root node %d in M.", id, node->id))
          unlockNode(node);        
          status = OK;
        }
      }
      else if (node->colour == GREEN) {
        DEBUG(msg("Root node %d in APS tree.", id, node->id))
        unlockNode(node);
        status = OK;
      }
      else {
        DEBUG(msg("Root node %d is processed.", id, node->id))
        unlockNode(node);
        status = ABORT;
      }

      // return node to queue
      if (status == ABORT) {
        DEBUG(msg("Return node %d to root node queue.", id, node->id))
        pthread_mutex_lock(qmutex);
        error = pushQueue(Q, (void*) node);
        pthread_mutex_unlock(qmutex);    
      }
    }  
  }

  return error;
}

//-------------------------------------------------------------------

void* _findMatchingParallel(void *params) {

  TThreadData *data = (TThreadData*) params;
  data->error = _findMatching(data->graph, data->queue, data->mutex, data->id);

  if (data->error != EOK) {
    fprintf(stderr, "ERROR %d\n", data->error);
  }

  pthread_exit(0);
}

//-------------------------------------------------------------------

int findMatching(TGraph *graph, int n) {

  // init queue
  int error = EOK;
  
  // init processes
  TThread *threads = malloc(n * sizeof(TThread));
  TThreadData *data = malloc(n * sizeof(TThreadData));
  
  if (threads == NULL) {
    free(threads);
    error = EALLOC;
  }
  
  if(data == NULL) {
    free(data);
    error = EALLOC;
  }
  
  if (error != EOK) {
    return error;
  }
  
  // init queue
  TQueue Q;
  initQueue(&Q);
  
  TMutex qmutex;
  pthread_mutex_init(&(qmutex), NULL);
  
  for (int i = 0; i < graph->n; i++) {
    error = pushQueue(&Q, (void*) &(graph->nodes[i]));
    
    if (error != EOK) {
      freeQueue(&Q);
      free(threads);
      free(data);
      return error;
    }
  }
  
  // find matching parallel
  for (int i = 0; i < n; i++) {

    // prepare data
    data[i].id = i + 1;
    data[i].graph = graph;
    data[i].queue = &Q;
    data[i].mutex = &qmutex;
    data[i].error = EOK;  
    
    // create thread
    pthread_create (&threads[i], NULL, &_findMatchingParallel, (void *) &data[i]);  
  }

  // join processes
  for (int i = 0; i < n; i++) {
    pthread_join(threads[i], NULL);
    if(error == EOK) error = data[i].error;
  }

  free(threads);
  free(data);  
  freeQueue(&Q);
  return EOK;
}

//------------------------------------------------------------------- MAIN FUNCTION

int main (int argc, char *argv[])
{
  int error = EOK;
  
  // check params
  if (argc == 3) {
  
    // get number of processes
    int n = atoi(argv[2]);
    if (n > 0) {
  
      // open file
      FILE *f = fopen(argv[1], "r");
      if (f != NULL) {
  
        // load graph
        TGraph graph;      
        error = loadGraph(&graph, f);
        if (error == EOK) {
      
          // print graph
          DEBUG(printGraph(&graph, stderr))
        
          // find matchinf
          error = findMatching(&graph, n);
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
