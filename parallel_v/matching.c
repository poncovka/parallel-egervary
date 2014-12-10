/*
 * Project: GAL 2014 - parallel version of matching in bipartite graphs
 * Author:  Vendula Poncova, xponco00
 * Date:    4.12.2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>

#define DEBUG(y) //y;
#define INLINE   inline

enum booleans {
  FALSE = 0,
  TRUE = 1
};

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
 WHITE
};

enum treestat {
  FREE = 0,
  INPROCESS,
  APSTREE
};

enum retstat {
  OK = 0,
  ABORT,
  IGNORE,
  PATH,
  LOOSER,
  ERROR
};

typedef struct tGraph TGraph;
typedef struct tTree TTree;
typedef struct tNode TNode;
typedef struct tEdge TEdge;
typedef struct tQueue TQueue;
typedef struct tItem TItem;

typedef pthread_t TThread;
typedef pthread_mutex_t TMutex;
typedef pthread_cond_t TCondition;
typedef struct tThreadData TThreadData;


struct tGraph {
  int n;
  TNode *nodes;
  
  int ntree;
  TTree *trees;
  TMutex mutex;
};

struct tTree {
  int id;
  int status;
  int owner; 
  int count; 
  int looser;
  int hadpath;

  TNode *root;
  TTree *next;
  TMutex mutex;
  
  TMutex cmutex;
  TCondition condition;
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

struct tQueue {
  TItem *first;
  TItem *last;
};

struct tItem {
  void *item;
  TItem *next;
};

struct tThreadData {
  int id;
  int error;
  TGraph *graph;
  TQueue *queue;
  TMutex *mutex;
};

int conflicts = 0;
TMutex confmutex;
TGraph *global_graph;

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

int initGraph(TGraph *graph, int n) {

  // init graph
  graph->n = n;
  graph->nodes = malloc(n * sizeof(TNode));

  if(graph->nodes == NULL) {
    return EALLOC;
  }

  graph->ntree = 0;
  graph->trees = NULL;
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
  
  // init conflict variable
  conflicts = 0;
  pthread_mutex_init(&(confmutex), NULL);
  global_graph = graph;
  
  return EOK;
}

void addConflict() {
  pthread_mutex_lock(&(confmutex));
  conflicts++;
  pthread_mutex_unlock(&(confmutex));
}


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
  }
  
  // free nodes
  free(graph->nodes);
  
  // free trees
  TTree *tree = graph->trees;
  while(tree != NULL) {
  
    pthread_mutex_destroy(&(tree->mutex));
    pthread_cond_destroy(&(tree->condition));
    
    TTree *old = tree;
    tree = tree->next;
    free(old);
  }
  
}

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
  
  return EOK;
}

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
  
  msg("Number of trees: %d",0, graph->ntree);
  msg("Number of conflicts: %d",0, conflicts);
  
  // print number of edges in matching
  fprintf(f, "%d\n", M);  

}

TTree *createTree(TGraph *graph) {

  // allocate tree
  TTree *tree = malloc(sizeof(TTree));
  if (tree == NULL) {
    return NULL;
  }
  
  // init tree
  tree->status = INPROCESS;
  tree->count = 0;
  tree->root = NULL;
  tree->owner = 0;
  tree->looser = 0;
  tree->hadpath = 0;
  
  // init mutex
  pthread_mutex_init(&(tree->mutex), NULL); 
  pthread_mutex_init(&(tree->cmutex), NULL); 
  pthread_cond_init(&(tree->condition), NULL);

  // critical section
  pthread_mutex_lock(&(graph->mutex));  
  
  tree->id = graph->ntree++;
  tree->next = graph->trees;
  graph->trees = tree;
    
  pthread_mutex_unlock(&(graph->mutex));  
  
  return tree;
}

INLINE void initQueue(TQueue *Q) {
  Q->first = NULL;
  Q->last = NULL;
}

INLINE int isEmpty(TQueue *Q) {
  return (Q->first == NULL);
}

INLINE int enqueue(TQueue *Q, void *item) {
  
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

INLINE void* dequeue(TQueue *Q) {
  
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

INLINE void freeQueue(TQueue *Q) {

  while(!isEmpty(Q)) dequeue(Q);

}

INLINE void lockNode(TNode *node) {
  pthread_mutex_lock(&(node->mutex));
}

INLINE void unlockNode(TNode *node) {
  pthread_mutex_unlock(&(node->mutex));
}


INLINE void lockTree(TTree *tree) {
  pthread_mutex_lock(&(tree->mutex));
}

INLINE void unlockTree(TTree *tree) {

  pthread_mutex_lock(&(tree->cmutex));
  pthread_mutex_unlock(&(tree->mutex));
  pthread_cond_broadcast(&(tree->condition));
  pthread_mutex_unlock(&(tree->cmutex));

}

INLINE int tryLockTree(TTree *tree) {

 pthread_mutex_lock(&(tree->cmutex));
 int ret = (pthread_mutex_trylock(&(tree->mutex)) == 0); 
 if (ret) pthread_mutex_unlock(&(tree->cmutex));
 
 return ret;
}

INLINE void waitTree(TTree *tree) {

  pthread_cond_wait(&(tree->condition), &(tree->cmutex));
  pthread_mutex_unlock(&(tree->cmutex));
  
}

// Returns tree if tree is locked, otherwise node is locked.
INLINE TTree* lockTreeOrNode(TNode *node, int id) {

  DEBUG(msg("Lock tree or node %d", id, node->id))

  TTree *tree = NULL;
  
  // lock the node
  DEBUG(msg("Locking node %d", id, node->id))
  lockNode(node);
  DEBUG(msg("Locked node %d", id, node->id))
  
  // lock the tree
  while (node->tree != NULL) {

    tree = node->tree;
    if (tryLockTree(tree)) {
      break;
    }
    else {
      unlockNode(node);
      DEBUG(msg("A1 Wait tree %d", id, tree->id))
      waitTree(tree);
      DEBUG(msg("A2 Locking node %d", id, node->id))
      lockNode(node);
       DEBUG(msg("A3 Locked node %d", id, node->id))
    }
  }
  DEBUG(msg("Locked tree %d", id, node->id))
  // save the tree
  tree = node->tree;
  
  // unlock the node if tree is not null
  if (node->tree != NULL) {
    unlockNode(node);
  }

  //DEBUG(msg("End of lock tree or node.", id))
  return tree;
}

INLINE TTree* lockTreesOrNode(TTree *treeA, TNode *nodeB) {

  DEBUG(msgt("Lock trees or node %d", treeA, nodeB->id))

  TTree *treeB, *treeX, *treeY;
  int idA = treeA->id;
  int idB = 0;
    
  // lock the node B
  DEBUG(msgt("Locking node %d", treeA, nodeB->id))
  lockNode(nodeB);
  DEBUG(msgt("Locked node %d", treeA, nodeB->id))
  treeB = NULL;

  while (nodeB->tree != NULL) {
  
    treeB = nodeB->tree;
    idB = treeB->id;
    
    if (idA == idB) {
    
      // try lock A
      if (tryLockTree(treeA)) {
        break;
      }
      else {
        unlockNode(nodeB);
        DEBUG(msgt("A1 Waiting for tree %d", treeA, treeA->id))
        waitTree(treeA);
        DEBUG(msgt("A2 Locking node %d", treeA, nodeB->id))
        lockNode(nodeB);
        DEBUG(msgt("A3 Locked node %d", treeA, nodeB->id))
      }
    }
    
    else { 
    
      // order by id
      if (idA < idB) {
        treeX = treeA;
        treeY = treeB;
      }
      else {
        treeX = treeB;
        treeY = treeA;
      }
      
      // lock X
      if(tryLockTree(treeX)) {

        // lock Y
        if(tryLockTree(treeY)) {
          break;
        }
        else {
          unlockTree(treeX);
          unlockNode(nodeB);
          DEBUG(msgt("Y1 Waiting tree %d", treeA, treeY->id))
          waitTree(treeY);
          DEBUG(msgt("Y2 Locking node %d", treeA, nodeB->id))
          lockNode(nodeB);
          DEBUG(msgt("Y3 Locked node %d", treeA, nodeB->id))
        }
      } 
      else {
        unlockNode(nodeB);
        DEBUG(msgt("X1 Waiting tree %d", treeA, treeX->id))
        waitTree(treeX);
        DEBUG(msgt("X2 Locking node %d", treeA, nodeB->id))
        lockNode(nodeB);
        DEBUG(msgt("X3 Locked node %d", treeA, nodeB->id))
      }
    }
  }
  
  treeB = nodeB->tree;
  
  // unlock node B
  if(treeB != NULL) {
    unlockNode(nodeB);
  }

  //DEBUG(msgt("End of lock trees or node.", treeA))
  return treeB;
}

// Returns OK, if free node is locked.
INLINE int lockFreeNode(TNode *node, int id) {

  int status = OK; 
  TTree *tree = lockTreeOrNode(node, id);  
  
  if (tree != NULL) {
  
    // tree is locked
    // lock free node
    if (tree->status == FREE) {
      lockNode(node);
      status = OK;
    }
    else if (tree->status == INPROCESS) {
      status = IGNORE;
    }
    else if (tree->status == APSTREE) {
      status = IGNORE;
    }
    else { 
      status = ABORT;
    }
    
    // unlock tree
    unlockTree(tree);   
  }
  
  // free node is locked
  return status;
}

// SYNC: tree and both nodes have to be locked
INLINE void changeM(TEdge *edge) {

  edge->M = !(edge->M);
  edge->reversed->M = !(edge->reversed->M);
  
}

// SYNC: tree has to be locked
INLINE void processPath(TTree *tree, TNode *end) {

  TNode *u, *v;
  TEdge *uv, *vu;
    
  u = end;
  lockNode(u);
       
  while (u->entry != NULL) {
   
    vu = u->entry;
    uv = vu->reversed;
    v  = uv->node;
    
    lockNode(v);
    changeM(uv);
    unlockNode(u);  
    
    u = v;
  }
  unlockNode(u);
  
  tree->status = FREE;
  tree->hadpath = 1;
}

// SYNC: tree and node have to be locked
INLINE void _addNodeToTree(TTree *tree, TNode *node, TEdge *edge, int colour) {
  
  node->tree = tree;
  node->entry = edge;
  node->colour = colour;
  
  tree->count++;
}

// add node B to the tree A via edge AB
int addNodeToTree(TTree *treeA, TNode *nodeA, TNode *nodeB, TEdge *AB, int M) {
  DEBUG(msgt("Add node B %d to node A %d.", treeA, nodeB->id, nodeA->id))

  // init
  int status = OK;
  int colour = M ? RED : BLUE;
  
  // lock trees A and B or lock only node B
  TTree *treeB = lockTreesOrNode(treeA, nodeB);
  
  // node B does not have a tree
  if (treeB == NULL) {
  
    lockTree(treeA);

    if (treeA->status != INPROCESS) {
      DEBUG(msgt("ABORT: Tree A is not in process.", treeA))
      status = ABORT;
    }
    
    else if (AB->M != M) {
      DEBUG(msgt("IGNORE: Wrong type of edge.", treeA))
      status = IGNORE;
    }
    
    else {
      DEBUG(msgt("OK: The node %d is free.", treeA, nodeB->id))
      _addNodeToTree(treeA, nodeB, AB, colour);
      status = OK;   
    }
    
    unlockNode(nodeB);
    unlockTree(treeA);
  }
  
  // node B has a tree, locked tree A and tree B    
  else {  

    lockNode(nodeB);
    
    if (treeA->status != INPROCESS) {
      DEBUG(msgt("ABORT: Tree A not in process.", treeA))
      status = ABORT;
    }
    
    else if (treeA->id == treeB->id) {    
      DEBUG(msgt("IGNORE: Trees A and B are same.", treeA))
      status = IGNORE;
    }
    
    else if (AB->M != M) {    
      DEBUG(msgt("IGNORE: Wrong type of edge.", treeA))
      status = IGNORE;
    }
    
    else if (treeB->status == APSTREE) {
      DEBUG(msgt("IGNORE: The node %d is in APS tree.", treeA, nodeB->id))
      status = IGNORE;
    }
    
    else if (treeB->status == FREE) {
      DEBUG(msgt("OK: The node %d is in a free tree.", treeA, nodeB->id))
      _addNodeToTree(treeA, nodeB, AB, colour);      
      status = OK;
    }
        
    else if (treeB->status == INPROCESS) {
      DEBUG(msgt("CONFLICT with nodes %d and %d.", treeA, nodeA->id, nodeB->id))

      lockNode(nodeA);  
            
      if(nodeA->colour == nodeB->colour) {
        DEBUG(msgt("PATH: Found path.", treeA))
          
        changeM(AB);
        
        unlockNode(nodeA);  
        processPath(treeA, nodeA);
          
        unlockNode(nodeB);
        processPath(treeB, nodeB);
        lockNode(nodeB);
                
        status = PATH;
      }   
      else {
        unlockNode(nodeA);
          
        DEBUG(msgt("IGNORE: Tree A becomes looser.", treeA))
        treeA->looser = 1;
        status = IGNORE;           
      }
    }
    
    unlockNode(nodeB);

    if (treeA != treeB) {
      unlockTree(treeA);
    }
    
    unlockTree(treeB);
  }

  DEBUG(msgt("End of add node to tree.", treeA))
  return status;
}

int _applyAPS(TTree *tree, TQueue *Q, int *ptrStatus) {

  DEBUG(msgt("Apply APS for root %d.", tree, tree->root->id))

  int M = 0; 
  int error = EOK; 
  int status = OK;
  
  TNode *x, *y, *z, *pathEnd = NULL;
  TEdge *xy, *yz;

  // insert root into Q
  error = enqueue(Q, (void*) tree->root);
  
  if (error != EOK)
    return error;
  
  // process the queue
  while(!isEmpty(Q) && status == OK && error == EOK) {

    // get x
    x = dequeue(Q);    
    xy = x->edges;
    
    while(xy != NULL && status == OK && error == EOK) {

      // get y
      y = xy->node;
      status = addNodeToTree(tree, x, y, xy, 0);

      // try next edge
      if (status == IGNORE) {
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
          // new z
          else if (status == OK) {
      
            DEBUG(msgt("Added new z=%d.", tree, z->id));
            error = enqueue(Q, (void*) z);
            M++;
            break;
          }
        }    
      
        // found path from y
        if (status == OK && !M) {  
          DEBUG(msgt("Found path, y is not in M.", tree))
          status = PATH;
          pathEnd = y;
        }
      
        // try next edge xy
        xy = xy->next;      
      }
    }
  }

  // lock the tree
  lockTree(tree);

  if (tree->status == FREE && tree->hadpath) {
    status = PATH;
  }
  
  else if (tree->status == INPROCESS && error == OK) { 
  
    if (tree->looser && status == OK) {
      status = ABORT;
    }
    
    if (status == OK) {
      DEBUG(msgt("Set the tree to APS.", tree))
      tree->status = APSTREE;  
    }
    else if(status == PATH) {
      DEBUG(msgt("Process path from %d.", tree, pathEnd->id))
      processPath(tree, pathEnd);
      tree->status = FREE;            
    }
    else if(status == ABORT) {
      DEBUG(msgt("Free the tree.", tree))
      tree->status = FREE;
    } 
  }
   
  // unlock the tree
  unlockTree(tree);
  
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

int _findMatching(TGraph *graph, TQueue *Q, TMutex *qmutex, int id) {

  int error = EOK;
  int status = OK;
  
  while(error == EOK) {
    
    // lock the queue
    DEBUG(msg("Get new root node.", id))
    pthread_mutex_lock(qmutex);
    
    // is queue empty?
    if(isEmpty(Q)) {
      DEBUG(msg("Root node queue is empty.", id))
      pthread_mutex_unlock(qmutex);
      break;
    }
    
    else {
    
      // get root node
      TNode *node = dequeue(Q);
      
      // unlock the queue
      pthread_mutex_unlock(qmutex);
      
      // lock the node
      status = lockFreeNode(node, id);

      if (status == OK) {
        
        // does it belong to M?
        TEdge *edge = node->edges;
        
        while((edge != NULL) && !(edge->M)) {
          edge = edge->next;
        }

        // node is not in M
        if (edge == NULL) {
        
          // create tree
          TTree *tree = createTree(graph);
          if (tree == NULL) return EALLOC;
          
          // lock the tree
          lockTree(tree);
      
          tree->root = node; 
          tree->owner = id;
          tree->count++;
          
          _addNodeToTree(tree, node, NULL, RED);

          // unlock the node
          unlockNode(node);
          
          // unlock the tree     
          unlockTree(tree);
      
          // find augmenting path
          error = applyAPS(tree, &status);   

        }
        // node is in M
        else {
          DEBUG(msg("Root node %d in M.", id, node->id))
          unlockNode(node);        
          status = OK;
        }
      }

      if (status == ABORT) {
        DEBUG(msg("Return node %d to root node queue.", id, node->id))
        pthread_mutex_lock(qmutex);
        error = enqueue(Q, (void*) node);
        pthread_mutex_unlock(qmutex);    
      }
    }  
  }

  return error;
}

void* _findMatchingParallel(void *params) {

  TThreadData *data = (TThreadData*) params;
  data->error = _findMatching(data->graph, data->queue, data->mutex, data->id);

  if (data->error != EOK) {
    fprintf(stderr, "ERROR %d\n", data->error);
  }

  pthread_exit(0);
}

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
    error = enqueue(&Q, (void*) &(graph->nodes[i]));
    
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


/////////////////////////////////////////////////////
/**
 * Main function
 */
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
