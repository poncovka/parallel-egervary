#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#define true 1
#define false 0

typedef enum errors {
    EOK = 0,
    EPARAM,
    EFILE,
    EINPUT,
    EALLOC,
    EQUEUE,
    EUNKNOWN
} err;
  
typedef enum TColor {
    RED,
    BLUE,
    NONE
} tcolor_t;
    
typedef enum ETreeState {
    NEWTREE,    // there is no tree yet
    FREE,       // tree is created and free
    PROCESSED,  // tree is processing (matching changing/deliting)
    APS
} etreestate_t;

typedef struct TTree tree_t;

typedef struct TNode tnode_t;

typedef struct TAdjList adjlist_t;

// list of the vertices representing the edges
typedef struct TListVert{
    adjlist_t* vertex;          // vertex
    struct TListVert* next;     // the next vertex in the list
    int isM;
    struct TListVert* reverse;  // reversed edge
} listvert_t, *listvert_p;

// Adjacency list
typedef struct TAdjList{
    int vname;         // name of the vertex
    int len;                   // number of members in the list
    tcolor_t color;        // color of he vertex
    tree_t* vertex_owner;      // id of the thread, that owns this vertex
    struct TListVert *parentInTree; // =NULL, if vertex is free and pointer if vertex is owned by some process
    struct TListVert *head;    // head of the adjacency linked list 
} *adjlist_p;

// Graph
typedef struct TGraph{
    int n;              // total number of vertices
    adjlist_p vertices;     // array of the adjacency lists
} graph_t, *graph_p;

typedef struct TTree{
    int thread_id;
    adjlist_p root;
    int size;
    etreestate_t state;
    struct TTree* next;
} *tree_p;

tree_p aps_trees = NULL ;

graph_p mygraph = NULL;

// Function to create a graph with n vertices 

graph_p graph_create (int n)
{
    graph_p graph = (graph_p) malloc ( sizeof (graph_t) );
    
    if ( graph == NULL )
        exit (3);       // Unable to allocate memory for graph
    graph->n = n;
 
    graph->vertices = (adjlist_p) malloc (n * sizeof (adjlist_t) );
    
    if ( !graph->vertices )
        exit (3);       // Unable to allocate memory for adjacency list array
 
    for ( int i = 0; i < n; i++ )
    {
        graph->vertices[i].vname = i;
        graph->vertices[i].len = 0;
        graph->vertices[i].color = NONE;
        graph->vertices[i].vertex_owner = NULL; 
        graph->vertices[i].head = NULL;
        graph->vertices[i].parentInTree = NULL;
    }
 
    return graph;
}

// Function to create an adjacency list node
listvert_p node_create (void)
{
    listvert_p newNode = (listvert_p) malloc ( sizeof (listvert_t) );
    if ( !newNode )
        exit(3);    // Unable to allocate memory for new node
 
    newNode->vertex = NULL;
    newNode->next = NULL;
    newNode->isM = false;
    newNode->reverse = NULL;

    return newNode;
}
 
// Function to destroy a graph
void graph_destroy (graph_p graph)
{
    if ( graph )
    {
        if ( graph->vertices )
        {
            // Free up the nodes
            for ( int v = 0; v < graph->n ; v++ )
            {
                listvert_p curhead = graph->vertices[v].head;
                while ( curhead )
                {
                    listvert_p tmp = curhead;
                    curhead = curhead->next;
            printf("graph destroy, tmp \n");    
                    free (tmp);
                }
            }
            // Free the adjacency list array
        printf("graph destroy, vertices \n");   
            free (graph->vertices);
        }
        // Free the graph
    printf("graph destroy, graph \n");  
        free (graph);
    }
}
 
// Function to add an edge to a graph
void graph_add_edge (graph_p graph, int src, int dest)
{
    listvert_p newNode = node_create ();
    newNode->next = graph->vertices[src].head;
    newNode->vertex = &(graph->vertices[dest]);
    graph->vertices[src].head = newNode;
    graph->vertices[src].len++;
 
    listvert_p newNodeR = node_create ();
    newNodeR->next = graph->vertices[dest].head;
    newNodeR->vertex = &(graph->vertices[src]);
    graph->vertices[dest].head = newNodeR;
    graph->vertices[dest].len++;

    newNode->reverse = newNodeR;
    newNodeR->reverse = newNode;    
}

// Function to print the adjacency list of graph
void graph_print (graph_p graph)
{
    for ( int i = 0; i < graph->n; i++ )
    {
        listvert_p curhead = graph->vertices[i].head;
        printf ("\n%d: ", i);
        while ( curhead )
        {
            printf ("%d->", curhead->vertex->vname);
            curhead = curhead->next;
        }
        printf ("NULL\n");
    }
}

// Function to read graph from file
// n m
// from to
//
// n - number of vertices ( numbering start from 0 )
// m - number of edges
graph_p graph_read (FILE* file)
{
    int n = 0;                  // number of vertices
    fscanf (file, "%d", &n);
    int m = 0;                  // number of edges
    fscanf (file, "%d", &m);
    graph_p mygraph = graph_create (n);     // graph
    int from, to;
    for ( int i = 1 ; i <= m ; i++ )
    {
        fscanf (file, "%d", &from);
        fscanf (file, "%d", &to);
        graph_add_edge (mygraph, from, to);
    }
    return mygraph;
}

pthread_t* mythreads = NULL;
pthread_mutex_t* vermutex = NULL; 
pthread_mutex_t* treemutex = NULL; 

tree_p tree_create (adjlist_p root, int threadid)
{
    tree_p result = &aps_trees[threadid];
    pthread_mutex_lock (&treemutex[threadid]);
    if ( aps_trees[threadid].root == NULL )
    {
        aps_trees[threadid].root = root;
        aps_trees[threadid].thread_id = threadid;
        aps_trees[threadid].state = FREE;
    }
    else
    {
        // dalsi strom davame na konec seznamu
        tree_p newapstree = malloc ( sizeof (tree_p)); 
        if ( !newapstree )
            exit(3);    // Unable to allocate memory for new tree
        result = newapstree;
        newapstree->root = root;
        newapstree->thread_id = threadid;
        newapstree->state = FREE;
        newapstree->next = NULL;
        tree_p curtree = &aps_trees[threadid];
        while ( curtree->next != NULL )
            curtree = curtree->next;
        curtree->next = newapstree;
    }
    pthread_mutex_unlock (&treemutex[threadid]);
    return result;
}

// should be called under the mutex treemutex 
void tree_destroy (tree_p tr, graph_p grph)
{
    for ( int i = 0 ; i < grph->n ; i++ )
    {
        pthread_mutex_lock (&vermutex[i]);
        if ( grph->vertices[i].vertex_owner->thread_id == tr->thread_id )
        {
            grph->vertices[i].vertex_owner = NULL ;
            grph->vertices[i].color = NONE ;
            grph->vertices[i].parentInTree = NULL ;
        }  
        pthread_mutex_unlock (&vermutex[i]);
    }
    tr->root = NULL; 
    tr->state = NEWTREE;
    // no need to change next
//    tree_p curtree = aps_trees[tr->thread_id];
//    if (curtree == tr)
//    {
//        curtree->root = NULL; 
//        curtree->state = FREE; 
//    }
//    else
//    {
//       while (curtree->next != tr )
//            curtree = curtree->next;
//       curtree->next = tr->next;
//        printf("tree destroy, tr\n");
//        free(tr); 
//    }
}

int isInM( adjlist_p vert)
{
    listvert_p curedge = vert->head;
    while ( ( curedge != NULL) && !( curedge->isM ) )
        curedge = curedge->next;
    if ( curedge != NULL )
        return true;
    else
        return false;
}

tree_p graph_get_free_vertex (graph_p grph, int myid)
{
    tree_p result = NULL;
    for ( int i = 0 ; i < grph->n ; i++ )
    {
        pthread_mutex_lock (&vermutex[i]);
        if ( grph->vertices[i].vertex_owner == NULL && grph->vertices[i].color == NONE  && !isInM (&grph->vertices[i]))
        {
            tree_p aps_tree = tree_create (&(mygraph->vertices[i]), myid);
            grph->vertices[i].vertex_owner = aps_tree;
            grph->vertices[i].color = RED;
            grph->vertices[i].parentInTree = NULL;
            printf ("thread %d: root is %d\n", myid, i);
            pthread_mutex_unlock (&vermutex[i]);
            result = aps_tree; 
            break;
        }
        else
            pthread_mutex_unlock (&vermutex[i]);
    }
    return result;
}

typedef struct tQueue TQueue;

typedef struct tItem TItem;

struct tQueue {
    TItem *first;
    TItem *last;
};

struct tItem {
    void *item;
    TItem *next;
};

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
    printf ("free queue\n");
    free(old);
    return item;
}

void freeQueue(TQueue *Q) {
    while(!isEmpty(Q)) dequeue(Q);
}

void matching_change (tree_p tree, adjlist_p pathEnd)
{
    adjlist_p u,vertex;
    listvert_p edge, edgeR;
    u = pathEnd;    
    while ( u != tree->root )
    {
        edge = u->parentInTree;
        edgeR = edge->reverse;
        vertex = edgeR->vertex;

        edge->isM = !(edge->isM);
        edgeR->isM = !(edgeR->isM);
        u = vertex;
    }
}

void graph_matching_print (graph_p graph)
{
    int M = 0;
    printf("<Matching>\n");
    for ( int i = 0; i < graph->n; i++)
    {
        adjlist_p node = &graph->vertices[i];
        if ( node != NULL )
        {
            listvert_p edge = node->head;
            while( edge != NULL )
            {
                if ( node->vname < edge->vertex->vname && edge->isM ) {
                    printf("(%d,%d) ", node->vname, edge->vertex->vname);
                    M++;
                }
                edge = edge->next;
            }
        }
    }
    if (M != 0) {
        printf("\n\n");
    }
}

// Because thread (name it A), that initial conflict can have an acces to the shared memory, where 
// the other process(name it B) store it information, then Process A can do all job itself and
// just notify the process B, that its tree was destroyed
// So, every process should be aware of it, and check the status of its tree before every important
// operation

void* do_aps (void *arg) 
{
    int id = *(int*) arg;
    printf ("thread %d: Starting do_aps()\n", id);
    tree_p aps_tree = graph_get_free_vertex (mygraph, id); // threadsafe
    // if there is no free vertices, then end;
    while ( aps_tree != NULL )
    {
        TQueue redvertices;
        initQueue (&redvertices);
        int iWasDestroyed = false;
        pthread_mutex_lock (&treemutex[id]); // lock myself
        // nevime, jestli ten vrchol nam jeste nekdo nesebral
        if ( aps_tree->state == NEWTREE )
        {
            // taky sebral
            iWasDestroyed = true;
        }
        else
        {
            // nesebral, pokracujeme
            enqueue (&redvertices, (void*) aps_tree->root);
        }
        pthread_mutex_unlock (&treemutex[id]);// unlock myself
        int pathFound = false;
        
        while ( !isEmpty(&redvertices) && !pathFound  && !iWasDestroyed)
        {
            adjlist_p curred = dequeue (&redvertices);  // current red vertex
            listvert_p curtoblue = curred->head;        // current edge from the red vertex to some possible blue vertex
    //      pthread_mutex_lock (&vermutex[curred->vname]);
    //      if is in conflict ??
    //      // solve conflict
    //      else go further
            while ( curtoblue != NULL  && !pathFound && !iwasDestroyed)
            {   
                pthread_mutex_lock (&vermutex[curtoblue->vertex->vname]);

                if ( curtoblue->vertex->vertex_owner == NULL ) // vertex is free
                {
                    if ( curtoblue->vertex->vertex_owner->thread_id != aps_tree->thread_id ) // vertex is not my
                    {
                        curtoblue->vertex->vertex_owner = aps_tree ; //add curtoblue to aps_tree
                        curtoblue->vertex->color = BLUE ; //add curtoblue to aps_tree
                        curtoblue->vertex->parentInTree = curtoblue ; //add curtoblue to aps_tree
                    }
                    else
                    {
                        // a blue vertex I want is not free, so, try to take it
                        // lock trees: with max id first
                        if ( curtoblue->vertex->vertex_owner->thread_id > id )
                        {
                            pthread_mutex_lock (&treemutex[curtoblue->vertex->vertex_owner->thread_id]);
                            pthread_mutex_lock (&treemutex[id]);// lock myself
                        }
                        else
                        {
                            pthread_mutex_lock (&treemutex[id]);// lock myself
                            pthread_mutex_lock (&treemutex[curtoblue->vertex->vertex_owner->thread_id]);
                        }
                        aps_tree->state = PROCESSED;
                        //TODO change the pointer to the tree
                        aps_trees[curtoblue->vertex->vertex_owner->thread_id].state = PROCESSED;
                        // tady by se nic nemelo stat, protoze strom je zablokovan
                        // muzeme operovat se vsem tak, jak chceme
                        if ( curtoblue->vertex->color == RED ) 
                        {
                            matching_change (aps_tree, curred); //change my M
                            tree_destroy (aps_tree, mygraph);
                            curtoblue->isM = !curtoblue->isM;   // = true
                            // this produces sementation fault TODO
                            matching_change (aps_tree/*WRONG TREE*/, curtoblue->vertex); //change M
                            tree_destroy (&aps_trees[curtoblue->vertex->vertex_owner->thread_id], mygraph);
                            graph_matching_print (mygraph);
                            pathFound = true;   
                        }
                        else
                        {
                            //TODO subtree
                        }   
                        // unlock
                        aps_trees->state = FREE;
                        //TODO WRONG TREE
                        aps_trees[curtoblue->vertex->vertex_owner->thread_id].state = FREE;
                        if ( curtoblue->vertex->vertex_owner->thread_id > id )
                        {
                            pthread_mutex_unlock (&treemutex[id]);// lock myself
                            pthread_mutex_unlock (&treemutex[curtoblue->vertex->vertex_owner->thread_id]);
                        }
                        else
                        {
                            pthread_mutex_unlock (&treemutex[curtoblue->vertex->vertex_owner->thread_id]);
                            pthread_mutex_unlock (&treemutex[id]);// lock myself
                        }
                    }
                    // now, I have a blue vertex I want
                    if ( !isInM(curtoblue->vertex) ) // to do, it seems, that it is not atomic
                    {
                        // hey, this blue is not in M, we found a path
                        // so release vertex and lock the tree
                        pthread_mutex_unlock (&vermutex[curtoblue->vertex->vname]);
                        pthread_mutex_lock (&treemutex[id]);// lock myself
                        // check myself if smth changed TODO
                        matching_change (aps_tree, curtoblue->vertex);//change M
                        graph_matching_print (mygraph);
                        tree_destroy (aps_tree, mygraph);
                        pathFound = true;   
                        pthread_mutex_unlock (&treemutex[id]);// unlock myself
                    }
                    else
                    {
                        pthread_mutex_unlock (&vermutex[curtoblue->vertex->vname]);
                        
                        listvert_p newEdgeToRed = curtoblue->vertex->head;
                        int redIsFound = false; 
                        // there SHOULD  be EXACTLY one, because of the bipartitity of the graph
                        while ( ( newEdgeToRed != NULL ) && ( !redIsFound) )
                        {
                            adjlist_p newred = newEdgeToRed->vertex;
                            
                            pthread_mutex_lock (&vermutex[newred->vname]);
                            if ( newEdgeToRed->isM )
                            {
                                if ( newred->vertex_owner == NULL ) // is free
                                {   
                                    newred->vertex_owner = aps_tree ; //add newred to aps_tree
                                    newred->color = RED ; //add newred to aps_tree
                                    newred->parentInTree = newEdgeToRed ; //add newred to aps_tree
                                }
                                else
                                {// conflict B-B. B-R nikdy nenastane, porusi se podminka biparcitnosti
                                    pathFound = true;
                                    // lock trees: with max id first
                                    if ( newred->vertex_owner->thread_id > id )
                                    {
                                        pthread_mutex_lock (&treemutex[newred->vertex_owner->thread_id]);
                                        pthread_mutex_lock (&treemutex[id]);// lock myselfi
                                    }
                                    else
                                    {
                                        pthread_mutex_lock (&treemutex[id]);// lock myselfi
                                        pthread_mutex_lock (&treemutex[newred->vertex_owner->thread_id]);
                                    }
                                    // check if somthing changed TODO
                                    matching_change (aps_tree, curtoblue->vertex);//change M
                                    tree_destroy (aps_tree, mygraph);
                                    newEdgeToRed->isM = !newEdgeToRed->isM;   // = false    
                                    // this produces sementation fault TODO
                                    matching_change (aps_tree, newred); //change M
                                    tree_destroy (&aps_trees[newred->vertex_owner->thread_id], mygraph);
                                    graph_matching_print (mygraph);
                                    // unlock
                                    if ( newred->vertex_owner ->thread_id> id )
                                    {
                                        pthread_mutex_unlock (&treemutex[id]);// lock myselfi
                                        pthread_mutex_unlock (&treemutex[newred->vertex_owner->thread_id]);
                                    }
                                    else
                                    {
                                        pthread_mutex_unlock (&treemutex[newred->vertex_owner->thread_id]);
                                        pthread_mutex_unlock (&treemutex[id]);// lock myselfi
                                    }
                                }   
                                enqueue (&redvertices, newred);
                                redIsFound = true;
                            }
                            pthread_mutex_unlock (&vermutex[newred->vname]);
                            newEdgeToRed = newEdgeToRed->next;
                        }//end of processing blue vertex childs
                    }// end of blue vertex processing
                }// end if vertex is my 
                else
                    pthread_mutex_unlock (&vermutex[curtoblue->vertex->vname]);
                curtoblue = curtoblue->next;
            }
                        
    //      pthread_mutex_unlock (&vermutex[curred->vname]);
    
            curred = dequeue (&redvertices);
        }   // end of processing of all red vertices
        aps_tree  = graph_get_free_vertex (mygraph, id);
    }// end of while
    // there is no free vertices    
    pthread_exit (NULL);
}


void graph_egervary_parallel (int threads_num)
{
    int *threadnumbers = NULL;
    graph_p matching = graph_create (mygraph->n); // nemuzu se toho zbavit, ptotoze po zakomentovaani tohoto radku, nefunguje mutex
//  graph_print (matching );
    
    int tn = mygraph->n < threads_num ? mygraph->n : threads_num;
    printf (" would be created %d threads\n", tn);
    
    threadnumbers = malloc (sizeof (int) * tn); 
    if ( threadnumbers == NULL )
    {
        printf ("Out of memory\n");
        exit (EXIT_FAILURE);
    }
    mythreads  = malloc (sizeof (pthread_t) * tn); 
    if ( mythreads == NULL ) 
    {
        free (threadnumbers);
        printf ("Out of memory\n");
        exit (EXIT_FAILURE);
    }

    vermutex  = malloc (sizeof (pthread_mutex_t) * mygraph->n); 
    if ( vermutex == NULL ) 
    {
        free (threadnumbers);
        free (mythreads);
        printf ("Out of memory\n");
        exit (EXIT_FAILURE);
    }

    aps_trees = malloc (sizeof (tree_p) * tn); 
    if ( aps_trees == NULL )
    {
        free (threadnumbers);
        free (mythreads);
        free (vermutex);
        printf ("Out of memory\n");
        exit (EXIT_FAILURE);
    }
    
    treemutex  = malloc (sizeof (pthread_mutex_t) * tn); 
    if ( treemutex == NULL ) 
    {
        free (threadnumbers);
        free (mythreads);
        free (vermutex);
        free (aps_trees);
        printf ("Out of memory\n");
        exit (EXIT_FAILURE);
    }

    pthread_attr_t attr;
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

    for ( int i = 0 ; i < tn ; i++ )
    {
        pthread_mutex_init(&vermutex[i], NULL);
        pthread_mutex_init(&treemutex[i], NULL);
        threadnumbers[i] = i;
        aps_trees[i].root = NULL;
        aps_trees[i].state = NEWTREE;
        aps_trees[i].next = NULL;
        while ( pthread_create (&mythreads[i], &attr, do_aps, &threadnumbers[i]) != 0 )
        {
            if ( errno == EAGAIN )  /* #define EAGAIN      11  znamena  Try again  */
                continue;
            printf ("The specified number of threads could not been created, because err=%d\n, exit", errno);
            free (threadnumbers);
            free (mythreads);
            free (vermutex);
            free (aps_trees);
            free (treemutex);
            exit (EXIT_FAILURE);
        }   
    }
    
    pthread_attr_destroy(&attr);
    
    printf ("main start to wait\n" );   
    for ( int i = 0; i < tn; i++) 
        pthread_join (mythreads[i], NULL);
        
    printf ("main end wait, start free\n" );    
    free (threadnumbers);
    free (mythreads);
    free (vermutex);
    free (aps_trees);
    // TODO proper free
    free (treemutex);
}


int main (int argc, char* argv[])
{
    if ( argc != 3 )
        {
            printf ("Wrong number of arguments, exit\n");
        exit (EXIT_FAILURE);
        }
        
    int n = atoi (argv[2]);
    if ( n <= 0 )
    {
        printf ("Wrong number of threads, exit\n");
        exit (EXIT_FAILURE);
    }

    FILE *file = fopen (argv[1], "r");

        if ( file == 0 )
        {
                printf("Could not open file %s, exit\n", argv[1]);
        exit (EXIT_FAILURE);
        }
        else 
    {
        mygraph = graph_read (file);
        fclose (file);
        graph_print (mygraph);
        graph_egervary_parallel (n);
        graph_matching_print (mygraph);
        //graph_print_to_file (matching);
        graph_destroy (mygraph);
    }
    return 0;
}
