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
} errrrr;


  
typedef enum TColor {
	RED,
	BLUE,
	NONE
} tcolor_t;
 	
typedef enum ETreeState {
	FREE,
	PROCESSED,
	APS
} etreestate_t;

typedef struct TTree tree_t;

typedef struct TNode tnode_t;

typedef struct TTreeListVert{
	tnode_t *vertex;			// vertex
	struct TTreeListVert* next;		// the next vertex in the list
} treelistvert_t, *treelistvert_p;

typedef struct TNode {
	int val;
	tree_t *tree;
	tcolor_t color;
	struct TNode *parent;  // pro rychlé projití M-augmenting path
	treelistvert_p children;
} *tnode_p;



typedef struct TAdjList adjlist_t;
// vertex list
typedef struct TListVert{
	adjlist_t* vertex;			// vertex
	struct TListVert* next;		// the next vertex in the list
	int isM;
	struct TListVert* reverse;  // reversed edge
} listvert_t, *listvert_p;

// Adjacency list
typedef struct TAdjList{
	int vname;
	int len;                   // number of members in the list
	tcolor_t color;
	int vertex_owner; //pthread_t* vertex_owner;
	struct TListVert *parentInTree;
	struct TListVert *head;    // head of the adjacency linked list 
} *adjlist_p;

// Graph
typedef struct TGraph{
	int n;					// total number of vertices
	adjlist_p vertices;		// array of the adjacency lists
} graph_t, *graph_p;


typedef struct TTree{
	int thread_id;
	adjlist_p root;
	int size;
	etreestate_t state;
} *tree_p;

tree_p *aps_trees = NULL ;

graph_p mygraph;

// Function to create a graph with n vertices 

graph_p graph_create (int n)
{
    graph_p graph = (graph_p) malloc ( sizeof (graph_t) );
	
    if ( graph == NULL )
        exit (3);  		// Unable to allocate memory for graph
    graph->n = n;
 
    graph->vertices = (adjlist_p) malloc (n * sizeof (adjlist_t) );
	
    if ( !graph->vertices )
        exit (3); 	    // Unable to allocate memory for adjacency list array
 
    for ( int i = 0; i < n; i++ )
    {
        
	graph->vertices[i].vname = i;
        graph->vertices[i].len = 0;
        graph->vertices[i].color = NONE;
	graph->vertices[i].vertex_owner = -1; 
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
        exit(3); 	// Unable to allocate memory for new node
 
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
                    free (tmp);
                }
            }
            // Free the adjacency list array
            free (graph->vertices);
        }
        // Free the graph
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
	int n = 0;					// number of vertices
	fscanf (file, "%d", &n);
	int m = 0;					// number of edges
	fscanf (file, "%d", &m);
	graph_p mygraph = graph_create (n);		// graph
//	graph_print (mygraph);
	int from, to;
	for ( int i = 1 ; i <= m ; i++ )
	{
		fscanf (file, "%d", &from);
		fscanf (file, "%d", &to);
		graph_add_edge (mygraph, from, to);
	}
	return mygraph;
}



tnode_p treenode_create (int nodeval)
{
	tnode_p node = NULL;
	node = (tnode_p) malloc ( sizeof (tnode_t) );
 
 	if ( node == NULL )
 	{
		printf("Malloc failed in treenode_create\n");
		return NULL;
 	}
	node->tree = NULL;
	node->children = NULL;
	node->val = nodeval;
	node->parent = NULL;
	node->color = NONE;
	return node;
}

pthread_t* mythreads = NULL;
pthread_mutex_t* vermutex = NULL; 
pthread_mutex_t* treemutex = NULL; 



tree_p tree_create (adjlist_p root, int threadid)
{
	aps_trees[threadid]->root = root;
	aps_trees[threadid]->thread_id = threadid;
	aps_trees[threadid]->state = FREE;
	
	return aps_trees[threadid];
}


// Function to add an edge to a tree 
/*void tree_add_edge_from_vertex (tree_p tree , int src, int dest)
{
    listvert_p newNode = node_create ();
    newNode->next = graph->vertices[src].head;
    graph->vertices[src].head = newNode;
    graph->vertices[src].len++;
 
    newNode = node_create ();
    newNode->next = graph->vertices[dest].head;
    graph->vertices[dest].head = newNode;
    graph->vertices[dest].len++;
}
*/
// Function to print the adjacency list of graph
/*
void treenode_destroy (tnode_p node)
{
        treelistvert_p cur = node->children;
        while ( cur )
        {
	        treelistvert_p tmp = cur;
        	cur = cur->next;
		treenode_destroy (tmp->vertex);
                free (tmp);
	}
}*/
void tree_destroy (tree_p tr, graph_p grph, int id)
{
	for ( int i = 0 ; i < grph->n ; i++ )
	{
		if ( grph->vertices[i].vertex_owner == id )
		{
			grph->vertices[i].vertex_owner = -1 ;
			grph->vertices[i].color = NONE ;
			grph->vertices[i].parentInTree = NULL ;
		}  
	}
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
// yes, then skip this node.
// if (edge != NULL) continue;

int graph_get_free_vertex (graph_p grph, int myid)
{
	int result = -1;
	for ( int i = 0 ; i < grph->n ; i++ )
	{
		pthread_mutex_lock (&vermutex[i]);
		if ( grph->vertices[i].vertex_owner == -1 && grph->vertices[i].color == NONE  && !isInM (&grph->vertices[i]))
		{
			grph->vertices[i].vertex_owner = myid;
			grph->vertices[i].color = RED;
			pthread_mutex_unlock (&vermutex[i]);
			result = i;	
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

void* do_aps (void *arg) 
{
	int id = *(int*) arg;
	printf ("thread %d: Starting do_aps()\n", id);
	int root = graph_get_free_vertex (mygraph, id);
	while ( root != -1 )
	{
		tree_p aps_tree = tree_create (&(mygraph->vertices[root]), id);
		
		if ( aps_tree == NULL )
			exit (EXIT_FAILURE);
		printf ("thread %d: root is %d\n", id, aps_tree->root->vname);
		TQueue redvertices;
		initQueue (&redvertices);
		enqueue (&redvertices, (void*) aps_tree->root);
		int pathFound = false;
		while ( !isEmpty(&redvertices) && !pathFound )
		{
			adjlist_p curred  = dequeue (&redvertices);
			listvert_p curblue = mygraph->vertices[curred->vname].head;
	//		pthread_mutex_lock (&vermutex[curred->vname]);
			while ( curblue != NULL  && !pathFound )
			{	
				pthread_mutex_lock (&vermutex[curblue->vertex->vname]);
				if ( mygraph->vertices[curblue->vertex->vname].vertex_owner != id ) // vertex is not my
				{
					if ( mygraph->vertices[curblue->vertex->vname].vertex_owner == -1 ) // vertex is free
					{
						mygraph->vertices[curblue->vertex->vname].vertex_owner = id ; //add curblue to aps_tree
						mygraph->vertices[curblue->vertex->vname].color = BLUE ; //add curblue to aps_tree
						mygraph->vertices[curblue->vertex->vname].parentInTree = curblue ; //add curblue to aps_tree
						if ( !curblue->isM )
						{
							// lock myself
						        matching_change (aps_tree, curblue->vertex);//change M
							graph_matching_print (mygraph);
							tree_destroy (aps_tree, mygraph, id);
							pathFound = true; 	
							// unlock myself
							pthread_mutex_unlock (&vermutex[curblue->vertex->vname]);
						}
						else
						{
							pthread_mutex_unlock (&vermutex[curblue->vertex->vname]);
							
							listvert_p newEdgeToRed = curblue->vertex->head;
							int redIsFound = false; 
							// there SHOULD  be EXACTLY one, because of the bipartitity of the graph
							while ( ( newEdgeToRed != NULL ) && ( !redIsFound) )
							{
								adjlist_p newred = newEdgeToRed->vertex;
								
								pthread_mutex_lock (&vermutex[newred->vname]);
								if ( newEdgeToRed->isM )
								{
									if ( newred->vertex_owner == -1 ) // is free
									{	
										newred->vertex_owner = id ; //add newred to aps_tree
										newred->color = RED ; //add newred to aps_tree
										newred->parentInTree = newEdgeToRed ; //add newred to aps_tree
									}
									else
									{// conflict B-B. B-R nikdy nenastane, porusi se podminka biparcitnosti
										pathFound = true;
										// lockmyself
										// lock vertex ownre tree
										// signal to vertex_owner
										//
						        			matching_change (aps_tree, curblue->vertex);//change M
										graph_matching_print (mygraph);
										tree_destroy (aps_tree, mygraph, id);
										newEdgeToRed->isM = !newEdgeToRed;   // = false	
										// unlock myself
										// //solveconflict
									}	
									enqueue (&redvertices, newred);
									redIsFound = true;
								}
								pthread_mutex_unlock (&vermutex[newred->vname]);
								newEdgeToRed = newEdgeToRed->next;
							}	
						}
					}
					else
					{
					}	
						
				}
				else
					pthread_mutex_unlock (&vermutex[curblue->vertex->vname]);
				curblue = curblue->next;
			}
						
	//		pthread_mutex_unlock (&vermutex[curred->vname]);
	
			curred = dequeue (&redvertices);
		}	
		root = graph_get_free_vertex (mygraph, id);
	}
	pthread_exit (NULL);
}


void graph_egervary_parallel (int threads_num)
{
	int *threadnumbers = NULL;
	graph_p matching = graph_create (mygraph->n); // nemuzu se toho zbavit, ptotoze po zakomentovaani tohoto radku, nefunguje mutex
//	graph_print (matching );
	
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
		threadnumbers[i] = i;
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
	
	free (threadnumbers);
	free (mythreads);
	free (vermutex);
	free (aps_trees);
	free (treemutex);
//	return matching; 
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
	//	graph_destroy (matching);
	}
	return 0;
}
