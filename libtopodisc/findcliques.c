/* -*- Mode: C; c-basic-offset:4 ; -*- */
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "findcliques.h"

#define MAX_CLIQUE_SIZE 128
#define MAX_NODENAME_SIZE 256

typedef struct {
    char *name;
    int  srcRank;
} NameInfo;

static int nameCompare( const void *, const void * );
static int errReturn( const char * );

/* Set verbose to non-zero to get more detail.  verbose = :
   1 - Print information about progress
   2 - Also print information about progress within loops
*/
static int verbose=0;
/* Collect statistics on the algorithm */
static int hashOccupancy = 0;
static double ttime = 0.0;

/*@
  MPE_FindCliqueFromName - Given a name that is unique to a node, find the
                           ranks of processes on the same node

  Input Parameters:
+ name - name of the node on which the process is running
. comm - communicator 
. hintSize - Used to control initial hash size
- maxSize - Maximum number of ranks in cliqueRanks

  Output Parameters:
+ cliqueNum - Id of this clique, starting from zero
. cliqueSize - Number of processes in this clique
- cliqueRanks - Ranks of processes in this clique

  Note:
  This is the general routine.  A number of convenience routines
  are provided to create communicators from cliques, and to return
  cliques in a communicator by using the node name.
  @*/
int MPE_FindCliqueFromName( const char *name, MPI_Comm comm, 
			    int hintSize, int maxSize, 
			    int *cliqueNum, int *cliqueSize, int cliqueRanks[] )
{
    int         csize, hsize, i, hash, goalVal, goalMaxVal, maxVal;
    int         rank, namelen, maxlen, *hashArray = 0;
    const char *p = 0;
    int        *cliqueInfo = 0;
    MPI_Status  status;
    MPI_Request req;

    /* Initial hash size and create hash array */
    ttime = MPI_Wtime();
    MPI_Comm_size( comm, &csize );
    if (csize == 1) {
	*cliqueSize = 1;
	*cliqueNum  = 1;
	cliqueRanks[0] = 0;
	return 0;
    }
    MPI_Comm_rank( comm, &rank );

    hsize = (hintSize + csize) / hintSize;
    hashArray = (int *) calloc( hsize, sizeof(int) );
    if (!hashArray) {
	return errReturn( "Unable to allocate hash table" );
    }

    goalVal    = (3 + hintSize) / 4;
    goalMaxVal = hintSize * 4;
    do {
	int nEmpty;
	/* Compute hash */
	p    = name;
	hash = 0;
	while (*p) hash += (*p++ - ' ');
	hash = hash % hsize;
	hashArray[hash] = 1;
	
	MPI_Allreduce( MPI_IN_PLACE, hashArray, hsize, MPI_INT, MPI_SUM, comm );

	/* */
	maxVal = 0;
	nEmpty = 0;
	for (i=0; i<hsize; i++) {
	    if (hashArray[i] > maxVal) maxVal = hashArray[i];
	    else if (hashArray[i] == 0) nEmpty++;
	}
	/* hashOccupancy is % occupancy. */
	hashOccupancy = (nEmpty * 100)/hsize;
	
	if (maxVal < goalVal && hsize > 16) hsize = hsize / 2;
	else if (maxVal > goalMaxVal && hsize < csize / 4) {
	    free( hashArray );
	    hsize *= 2;
	    hashArray = (int *)calloc( hsize, sizeof(int) );
	    if (!hashArray) {
		return errReturn( "Unable to reallocate hash table" );
	    }
	}
	else
	    /* Good enough */
	    break;
    } while (1);

    if (verbose && rank == 0) printf( "[0] Hash size = %d\n", hsize );

    /* At this point, we know that at most maxVal messages will be sent to any
       process */
    namelen = strlen( name ) + 1;
    MPI_Allreduce( &namelen, &maxlen, 1, MPI_INT, MPI_MAX, comm );

    /* Send to my target the info */
    /* Should avoid sending to myself */
    MPI_Isend( (char *)name, namelen, MPI_CHAR, hash, 0, comm, &req );
    if (verbose) printf( "[%d] sent to %d\n", rank, hash );

    /* Allocate space for the clique info */
    cliqueInfo = (int *)calloc( maxVal + 2, sizeof(int) );
    if (!cliqueInfo) {
	return errReturn( "Unable to allocate clique info" );
    }

    /* If I'm a target, receive data on the names */
    if (rank < hsize && hashArray[rank]) {
	NameInfo *names;
	int nclique, myIndex=0, j;
	char *curName;

	/* Allocate an array for the data */
	names = (NameInfo *) calloc( hashArray[rank], sizeof(NameInfo) );
	if (!names) {
	    /* Really need to panic */
	    return errReturn( "Unable to allocate names array" );
	}
	for (i=0; i<hashArray[rank]; i++) {
	    names[i].name = (char *)calloc( maxlen, sizeof(char) );
	    MPI_Recv( names[i].name, maxlen, MPI_CHAR, MPI_ANY_SOURCE, 0, comm, 
		      &status );
	    names[i].srcRank = status.MPI_SOURCE;
	    if (verbose & 0x2) printf( "[%d] received %s from %d\n", 
				       rank, names[i].name, status.MPI_SOURCE );
	}
	if (verbose) printf( "[%d] Received info\n", rank );
	
	/* Sort by name (note that there may be multiple names that hashed
	   to the same value */
	qsort( names, hashArray[rank], sizeof(NameInfo), nameCompare );

	/* Compute the number of cliques */
	nclique = 0;
	curName = (char *)"\0";
	for (i=0; i<hashArray[rank]; i++) {
	    if (strcmp(names[i].name, curName)) {
		curName = names[i].name;
		nclique++;
	    }
	}

	/* Compute the clique index*/
	MPI_Exscan( &nclique, &myIndex, 1, MPI_INT, MPI_SUM, comm );

	if (verbose) printf( "[%d] Computed clique index = %d\n", rank, myIndex);
	/* Return the information to each sender.  Note that we could 
	   parallelize this step by sending just to the root; let each 
	   root notify its members.
	   Each process is sent an integer array containing:
	   cliqueIndex, cliqueSize, ranks in clique
	*/
	j = 0;
	for (i=0; i<nclique; i++) {
	    int nInClique = 0, k;

	    cliqueInfo[0] = myIndex++;
	    curName       = names[j].name;
	    k             = j;
	    for (;j<hashArray[rank] && strcmp(curName,names[j].name) == 0; j++) {
		cliqueInfo[2+j-k] = names[j].srcRank;
		if (verbose & 0x2) printf( "[%d] Adding src %d to clique[%d]\n", 
				     rank, names[j].srcRank, j-k );
		nInClique++;
	    }
	    cliqueInfo[1] = nInClique;
	    if (verbose) printf( "[%d] clique size = %d\n", rank, nInClique );
	    for (k=0; k<nInClique; k++) {
		if (verbose & 0x2) printf( "[%d] Sending back to %d\n", rank, 
				     cliqueInfo[2+k] );
		if (rank == cliqueInfo[2+k]) {
		    int kk;
		    *cliqueNum = cliqueInfo[0];
		    *cliqueSize = cliqueInfo[1]; 
		    if (*cliqueSize > maxSize) {
			if (verbose) {
			    fprintf( stderr, "Cliquesize of %d too large\n", 
				     *cliqueSize );
			}
			MPI_Abort( MPI_COMM_WORLD, 1 );
			return 1;
		    }
		    for (kk=0; kk<*cliqueSize; kk++) {
			cliqueRanks[kk] = cliqueInfo[2+kk];
		    }
		}
		else {
		    MPI_Send( cliqueInfo, nInClique+2, MPI_INT, 
			      cliqueInfo[2+k], 1, comm );
		}
	    }
	}

	/* Free all allocated memory */
	for (i=0; i<hashArray[rank]; i++) {
	    free( names[i].name );
	}
	free( names );
    }
    else {
	/* Participate in the Exscan */
	int zero = 0, dummy;
	MPI_Exscan( &zero, &dummy, 1, MPI_INT, MPI_SUM, comm );
	if (verbose) printf( "[%d] Computed clique index = %d\n", rank, dummy);
    }
    MPI_Wait( &req, MPI_STATUS_IGNORE );
    
    /* Receive the clique info (if we need to) */
    if (hash != rank) {
	MPI_Recv( cliqueInfo, 2+maxVal, MPI_INT, hash, 1, comm,
		  MPI_STATUS_IGNORE );

	/* Unpack */
	*cliqueNum = cliqueInfo[0];
	*cliqueSize = cliqueInfo[1]; 
	for (i=0; i<*cliqueSize; i++) {
	    cliqueRanks[i] = cliqueInfo[2+i];
	}
    }

    ttime = MPI_Wtime() - ttime;
    return 0;
}

static int nameCompare( const void *n1, const void *n2 )
{
    NameInfo *p1 = (NameInfo *)n1, *p2 = (NameInfo *)n2;
    return strcmp( p1->name, p2->name );
}

static int errReturn( const char *msg )
{
    fprintf( stderr, "%s\n", msg );
    fflush(stderr);
    return -1;
}

int MPE_FindCliqueFromNameStats( int *ho, double *t )
{
    *ho = hashOccupancy;
    *t  = ttime;
    return 0;
}

/*@
  MPE_FindCliqueFromNodename - Find cliques from the node name within a communicator

  Input Parameters:
+ comm - Communicator of processes
. hintSize - ?
- maxSize - Maximum number of ranks in cliqueRanks

  Output Parameters:
+ cliqueNum - Id of this clique, starting from zero
. cliqueSize - Number of processes in this clique
- cliqueRanks - Ranks of processes in this clique

  @*/
int MPE_FindCliqueFromNodename( MPI_Comm comm, int hintSize, int maxSize, 
				int *cliqueNum, int *cliqueSize,
				int cliqueRanks[] )
{
    int err;
    char myname[MAX_NODENAME_SIZE];

    err = gethostname( myname, sizeof(myname) );
    if (err) {
	perror( "gethostname: " );
	MPI_Abort( MPI_COMM_WORLD, 1 );
    }
    err = MPE_FindCliqueFromName( myname, comm, 16, MAX_CLIQUE_SIZE, 
				  cliqueNum, cliqueSize, cliqueRanks );
    return err;
}

/* Useful service routines to create communicators */
/*@
  MPE_CreateSMPComm - Create a communicator for the processes on the same node

Input Parameter:
. comm - Communicator of processes

Output Parameters:
+ localComm - Communicator of processes on the same node 
. rootComm - Communicator of leaders on the different nodes
- nnodes   - My node number

  @*/
int MPE_CreateSMPComm( MPI_Comm comm, 
		       MPI_Comm *localComm, MPI_Comm *rootComm, int *nnodes )
{
    char myname[MAX_NODENAME_SIZE];
    int cliqueNum, cliqueSize, i, rank;
    int cliqueRanks[MAX_CLIQUE_SIZE];
    int err, iAmLow;

    err = gethostname( myname, sizeof(myname) );
    if (err) {
	perror( "gethostname: " );
	MPI_Abort( MPI_COMM_WORLD, 1 );
    }
    err = MPE_FindCliqueFromName( myname, comm, 16, MAX_CLIQUE_SIZE, 
				  &cliqueNum, &cliqueSize, cliqueRanks );
    *nnodes = cliqueNum;
    MPI_Comm_rank( comm, &rank );
    MPI_Comm_split( comm, cliqueNum, rank, localComm );
    /* Determine which process is the low rank in the local clique */
    iAmLow = 1;
    for (i=0; i<cliqueSize; i++) {
	if (rank > cliqueRanks[i]) iAmLow = 0;
    }
    MPI_Comm_split( comm, iAmLow, cliqueNum, rootComm );
    /* FIXME: Should we remove all but the low rank communicator? */

    return 0;
}

/*@
  MPE_CreateRoundRobinComm - Create a round-robin mapped communicator for 
  all processes. 

Input Parameter:
. comm - Communicator of processes

  Output Parameters:
+ rrcomm - New communicator where processes are mapped in a round-robin
           fashion between nodes
- nnodes   - My node number

  @*/
int MPE_CreateRoundRobinComm( MPI_Comm comm, MPI_Comm *rrcomm, int *nnodes )
{
    char myname[MAX_NODENAME_SIZE];
    int cliqueNum, cliqueSize, i, rank;
    int cliqueRanks[MAX_CLIQUE_SIZE];
    int err, localrank, newrank, maxSize;

    /* Algorithm:
       Find the cliques
       Find the largest number per node ( = P) and the number of nodes ( = N)
       Compute a "new rank" based on the following:  Let R be the index of the
       calling process in the ranks in the local clique and n the node number.
       
       new rank = n + R*P

       Comm_split, with this new rank and an identical color, will produce 
       a properly reordered communicator.
     */

    err = gethostname( myname, sizeof(myname) );
    if (err) {
	perror( "gethostname: " );
	MPI_Abort( MPI_COMM_WORLD, 1 );
    }
    err = MPE_FindCliqueFromName( myname, comm, 16, MAX_CLIQUE_SIZE, 
				  &cliqueNum, &cliqueSize, cliqueRanks );
    MPI_Allreduce( &cliqueNum, nnodes, 1, MPI_INT, MPI_MAX, comm );
    *nnodes = *nnodes + 1;
    maxSize = cliqueSize;
    MPI_Allreduce( MPI_IN_PLACE, &maxSize, 1, MPI_INT, MPI_MAX, comm );
    MPI_Comm_rank( comm, &rank );

    for (localrank = 0; localrank < cliqueSize; localrank++) 
	if (rank == cliqueRanks[localrank]) break;
    
    newrank = cliqueNum + localrank * maxSize;
    MPI_Comm_split( comm, 0, newrank, rrcomm );

    return 0;
}

/*@
  MPE_CreateConseqComm - Create a communiator for all processes where process 
  are consequtive on each node.

Input Parameter:
. comm - Communicator of processes

  Output Parameters:
+ rootComm - Communicator of processes, consequetive on each node
- nnodes   - My node number

  @*/
int MPE_CreateConseqComm( MPI_Comm comm, MPI_Comm *rrcomm, int *nnodes )
{
    char myname[MAX_NODENAME_SIZE];
    int cliqueNum, cliqueSize, i, rank;
    int cliqueRanks[MAX_CLIQUE_SIZE];
    int err, localrank, newrank, maxSize;

    /* Algorithm:
       Find the cliques

       Find the largest number per node ( = P) and the number of nodes ( = N)
       Compute a "new rank" based on the following:  Let R be the index of the
       calling process in the ranks in the local clique and n the node number.
       
       new rank = R + n*P

       Comm_split, with this new rank and an identical color, will produce 
       a properly reordered communicator.
     */

    err = gethostname( myname, sizeof(myname) );
    if (err) {
	perror( "gethostname: " );
	MPI_Abort( MPI_COMM_WORLD, 1 );
    }
    err = MPE_FindCliqueFromName( myname, comm, 16, MAX_CLIQUE_SIZE, 
				  &cliqueNum, &cliqueSize, cliqueRanks );
    MPI_Allreduce( &cliqueNum, nnodes, 1, MPI_INT, MPI_MAX, comm );
    *nnodes = *nnodes + 1;
    maxSize = cliqueSize;
    MPI_Allreduce( MPI_IN_PLACE, &maxSize, 1, MPI_INT, MPI_MAX, comm );
    MPI_Comm_rank( comm, &rank );

    for (localrank = 0; localrank < cliqueSize; localrank++) 
	if (rank == cliqueRanks[localrank]) break;
    
    newrank = localrank + cliqueNum * maxSize;
    MPI_Comm_split( comm, 0, newrank, rrcomm );

    return 0;
}
