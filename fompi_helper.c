// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <unistd.h>

#include "fompi.h"

void foMPI_Set_window_mutex( dmapp_return_t* status, dmapp_pe_t target_pe, foMPI_Win_desc_t* win_ptr, dmapp_seg_desc_t* win_ptr_seg, foMPI_Win win ) {

  int64_t result;

 /* TODO: busy waiting */

  do {
	//	do {

	//		*status = dmapp_get( &result, (void*) &(win_ptr->mutex), win_ptr_seg, target_pe, 1, DMAPP_QW);
  //		_check_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

	//	} while (result != foMPI_MUTEX_NONE);

  //  *status = dmapp_acswap_qw( &result, (void*) &(win_ptr->mutex), win_ptr_seg, target_pe, (int64_t) foMPI_MUTEX_NONE, (int64_t) win->commrank );
    *status = dmapp_acswap_qw( &result, (void*) &(win_ptr->mutex), win_ptr_seg, target_pe, (int64_t) foMPI_MUTEX_NONE, (int64_t) 0 );
    _check_dmapp_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

			//printf("[%i] kurwa mac\n",win->commrank);
  } while (result != foMPI_MUTEX_NONE);
}

void foMPI_Release_window_mutex( dmapp_return_t* status, dmapp_pe_t target_pe, foMPI_Win_desc_t* win_ptr, dmapp_seg_desc_t* win_ptr_seg, foMPI_Win win ) {

  int64_t result;

  result = foMPI_MUTEX_NONE;
  /* TODO: nonblocking ? */
 // *status = dmapp_put( (void*) &(win_ptr->mutex), win_ptr_seg, target_pe, &result, 1, DMAPP_QW); 
    *status = dmapp_acswap_qw( &result, (void*) &(win_ptr->mutex), win_ptr_seg, target_pe, (int64_t) 0, (int64_t) foMPI_MUTEX_NONE );
  _check_dmapp_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
}

int get_free_elem( fortran_free_mem_t **list_head ) { /* as a side effect also removes the first element from the list */

  int index;
  fortran_free_mem_t *entry;

  if (*list_head == NULL) {
    return -1; /* no free element left */
  }

  entry = *list_head;
  *list_head = entry->next;
  index = entry->index;

  _foMPI_FREE( entry );

  return index;
}

#ifdef XPMEM
/**
This is executed on the rank who has the buffer that should be exposed at ptr

provided by Timo Schneider, ETH Zurich
*/

/* TODO: static inline */
xpmem_seg_desc_t foMPI_export_memory_xpmem(void *ptr, int len) {

  xpmem_seg_desc_t ret;
  
  void *pagestart = (void*)(((unsigned long)ptr / getpagesize()) * getpagesize());
  ret.offset = (unsigned long) ptr - (unsigned long) pagestart;
  //FIXME might be one page too much
  size_t szpages = ((unsigned long)(ret.offset + len) / getpagesize() + 1) * getpagesize();
  //possible fix: if( ((unsigned long)(ret.offset + len) % getpagesize()) == 0) szpages--;
  ret.seg = xpmem_make(pagestart, szpages, XPMEM_PERMIT_MODE, (void *)0666);
  assert( ret.seg != -1 );

  return ret;
}

/**
This is called by the process that wants to write into a memory region exported
by someone else.

provided by Timo Schneider, ETH Zurich
*/

/* TODO: static inline */
void* foMPI_map_memory_xpmem(xpmem_seg_desc_t rseg, int len, xpmem_apid_t* apid, int* offset) {

  *apid = xpmem_get(rseg.seg, XPMEM_RDWR, XPMEM_PERMIT_MODE, NULL);
  //assert( *apid != -1 );
  if ( *apid == -1 ) {
    printf("%i: apid == -1\n", glob_info.debug_pe);
  }

  *offset = rseg.offset;
  //printf("%i: map offset = %i\n", debug_pe, *offset);

  struct xpmem_addr addr;
  addr.apid = *apid;
  addr.offset = 0;
  int szpages = ((unsigned long)(rseg.offset + len) / getpagesize() + 1) * getpagesize();
  void *map = xpmem_attach(addr, szpages, NULL);
  assert( map != (void*)-1 );
  // correct address in mapping
  return ((void*)((char*)map + rseg.offset));
}

/**
This is called by the process that wanted to free his access to a memory region exported
by someone else.
*/

/* TODO: static inline */
void foMPI_unmap_memory_xpmem( xpmem_apid_t* apid, void* addr, int offset ) {

#ifndef NDEBUG
  int ret;
#endif
 
  //printf("%i: unmap offset = %i\n", debug_pe, offset); 
#ifdef NDEBUG
  xpmem_detach( (char*)addr-offset );
#else  
  ret = xpmem_detach( (char*)addr-offset );
#endif
  assert( ret == 0 );

#ifdef NDEBUG
  xpmem_release( *apid );
#else
  ret = xpmem_release( *apid );
#endif
  assert( ret == 0 );
}

/**
This is called by the process that exported a memory region to unexport this region.
*/

/* TODO: static inline */
void foMPI_unexport_memory_xpmem( xpmem_seg_desc_t rseg ) {

#ifndef NDEBUG
  int ret;
#endif

#ifdef NDEBUG
  xpmem_remove( rseg.seg );
#else
  ret = xpmem_remove( rseg.seg );
#endif
  assert( ret == 0 );
}

int foMPI_onnode_rank_global_to_local( int grank, foMPI_Win win ) {
  int i;

  assert( (grank >= 0 ) && (grank <= win->commsize) );

  if ( ( grank >= win->onnode_lower_bound ) && ( grank <= win->onnode_upper_bound ) ) { /* fast range search */
    return grank-win->onnode_lower_bound;
  } else {
    if ( win->onnode_lower_bound == -1 ) {
      for( i=0 ; i<win->onnode_size ; i++) { /* slow search, TODO: faster implementation */
        if( win->onnode_ranks[i] == grank) {
          return i;
        }
      }
    }
    return -1; /* not a onnode process */
  }
}

int foMPI_onnode_rank_local_to_global( int lrank, foMPI_Win win ) {

  assert( (lrank >= 0) && (lrank < win->onnode_size) );

  if( win->onnode_lower_bound != -1 ) {
    return lrank+win->onnode_lower_bound;
  } else {
    return win->onnode_ranks[lrank];
  }

}
#endif

#ifdef PAPI
#include <string.h>

#include <papi.h>
//! handles all the time measurement

static int counter;

static char epoch_name[23][20];
static int ids[1024];

static MPI_File filehandle_values;

/* PAPI relevant variables */
static int PapiNumberEvents; // max 2
static int PapiEventSet;
static int PapiEventCode[2];

static long long PapiEventCounts[2048];
static char PapiEventName1[20] = "NA\0";
static char PapiEventName2[20] = "NA\0";

//! writes the timing values to the output file
//! if the last parameter is set, then all the unused epochs are also printed

static void timing_print( int last ) {

  char line[256];
  int i;

  char comparand[20];
  snprintf( comparand, 20, "NA" );

  char eventcount1[20];
  char eventcount2[20];

  for( i=0; i<counter ; i++ ) {

// select the right values for PAPI columns
    snprintf( eventcount1, 20, "%lli",  PapiEventCounts[2*i] );
    if (strncmp(PapiEventName1, comparand, 20) == 0 ) { /* handling of event counter that are not set */
      snprintf( eventcount1, 20, "NA");
    }
    snprintf( eventcount2, 20, "%lli",  PapiEventCounts[2*i+1] );
    if (strncmp(PapiEventName2, comparand, 20) == 0 ) { /* handling of event counter that are not set */
      snprintf( eventcount2, 20, "NA");
    }

//! if a epoch id is not in 1..4 is found, only the id is printed (instead of
//! the the epoch name)
    snprintf(line, 256, "%20s%20s%20s%20s%20s\n", epoch_name[ids[i]], PapiEventName1, eventcount1, PapiEventName2, eventcount2 );
    MPI_File_write(filehandle_values, line, strlen(line), MPI_CHAR, MPI_STATUS_IGNORE);
  }

  if (last == 1) {
    long long PapiValue[PapiNumberEvents];
    //stop the PAPI counter
    PAPI_stop( PapiEventSet, PapiValue );
    PAPI_reset( PapiEventSet );
  }

}

//! opens a file handle, to where the timing values are written
static void timing_open_file() {
      
  char filename[20] = "mpi3rma-ex.out\0";
  char line[256];
  int ier;

  ier = MPI_File_open( MPI_COMM_SELF, filename, MPI_MODE_EXCL + MPI_MODE_CREATE + MPI_MODE_WRONLY, MPI_INFO_NULL, &filehandle_values );

  if ( ier != MPI_SUCCESS ) {
    printf("Error at open file %s for writing the timing values. The file probably already exists.\nWill now abort.\n", filename);
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }
  snprintf(line, 256, "%20s%20s%20s%20s%20s\n", "id", "papi_evt1_type", "papi_evt1_val", "papi_evt2_type", "papi_evt2_val");
  MPI_File_write(filehandle_values, line, strlen(line), MPI_CHAR, MPI_STATUS_IGNORE);
}

static void init_papi() {

  int retval;

  retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT && retval > 0) {
    fprintf(stderr,"PAPI library version mismatch!\n");
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }

  if (retval < 0) {
    fprintf(stderr,"PAPI init error!\n");
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }

  retval = PAPI_is_initialized();
  if (retval != PAPI_LOW_LEVEL_INITED) {
    fprintf(stderr,"PAPI library not initialized!\n");
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }

  PapiEventSet = PAPI_NULL;
  if (PAPI_create_eventset(&PapiEventSet) != PAPI_OK) {
    fprintf(stderr,"PAPI could not create event set!\n");
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }

  //get the PAPI events
  char* papi_evt1 = getenv("PAPI_EVT1");
  char* papi_evt2 = getenv("PAPI_EVT2");

  PapiNumberEvents = 0;

  if ( (papi_evt1 != NULL) && (strlen(papi_evt1) != 0)) {
    retval = PAPI_event_name_to_code( papi_evt1, &PapiEventCode[0] );
    if ( retval != PAPI_OK ) {
      switch( retval ) {
        case PAPI_ENOTPRESET:   fprintf(stderr,"The event code provided in the environmental variable PAPI_EVT1 is not a valid PAPI preset!\n"); break;
        case PAPI_ENOEVNT:      fprintf(stderr,"The event provided in the environmental variable PAPI_EVT1 is not available on the underlying hardware!\n"); break;
        default:                fprintf(stderr,"The event code provided in the environmental variable PAPI_EVT1 is not correct!\n"); 
      }
      MPI_Abort( MPI_COMM_WORLD, 1 );
    }
    strncpy( PapiEventName1, papi_evt1, 20 );

    PapiNumberEvents++;
    
    if (PAPI_add_event(PapiEventSet, PapiEventCode[0]) != PAPI_OK) {
      fprintf(stderr,"PAPI could not add counter to event set!\n");
      MPI_Abort( MPI_COMM_WORLD, 1 );
    }
  }

  if ( (papi_evt2 != NULL) && (strlen(papi_evt2) != 0) ) {
    retval = PAPI_event_name_to_code( papi_evt2, &PapiEventCode[1] );
    if ( retval != PAPI_OK ) {
      switch( retval ) {
        case PAPI_ENOTPRESET:   fprintf(stderr,"The event code provided in the environmental variable PAPI_EVT2 is not a valid PAPI preset!\n"); break;
        case PAPI_ENOEVNT:      fprintf(stderr,"The event provided in the environmental variable PAPI_EVT2 is not available on the underlying hardware!\n"); break;
        default:                fprintf(stderr,"The event code provided in the environmental variable PAPI_EVT2 is not correct!\n"); 
      }
      MPI_Abort( MPI_COMM_WORLD, 1 );
    }
    strncpy( PapiEventName2, papi_evt2, 20 );

    PapiNumberEvents++;
  
    if ( PAPI_add_event(PapiEventSet, PapiEventCode[1]) != PAPI_OK) {
      fprintf(stderr,"PAPI could not add counter to event set!\n");
      MPI_Abort( MPI_COMM_WORLD, 1 );
    }
  } 

  if ( PapiNumberEvents == 0 ) {
    fprintf(stderr, "Althrough the mpi3rma code was compiled with PAPI support, the environmental variables PAPI_EVT1 and PAPI_EVT2 are not set.\n");
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }
}

static void cleanup_papi() {
  if ( PAPI_cleanup_eventset(PapiEventSet) != PAPI_OK ) {
    fprintf(stderr,"PAPI could not clean up event set!\n");
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }
  if ( PAPI_destroy_eventset(&PapiEventSet) != PAPI_OK ) {
    fprintf(stderr,"PAPI could not destroy event set!\n");
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }
}

//! closes the above mentioned filehandle
static void timing_close_file() {
  MPI_File_close( &filehandle_values );
}
//! sets some init parameter like the testname, method and number of bytes
//! also sets static the epoch names, the epoch_used array and the initial timing with a special id (zero)

void timing_init() {

  counter = 0;

  snprintf( &epoch_name[0][0], 20, "reference");
  snprintf( &epoch_name[1][0], 20, "put_before_dmapp");
  snprintf( &epoch_name[2][0], 20, "put_dmapp");
  snprintf( &epoch_name[3][0], 20, "put_after_dmapp");
  snprintf( &epoch_name[4][0], 20, "flush");
  snprintf( &epoch_name[5][0], 20, "get_before_dmapp");
  snprintf( &epoch_name[6][0], 20, "get_dmapp");
  snprintf( &epoch_name[7][0], 20, "get_after_dmapp");
  snprintf( &epoch_name[8][0], 20, "unlock_before_flush");
  snprintf( &epoch_name[9][0], 20, "unlock_after_flush");
  snprintf( &epoch_name[10][0], 20, "lock");
  snprintf( &epoch_name[11][0], 20, "start");
  snprintf( &epoch_name[12][0], 20, "complete");
  snprintf( &epoch_name[13][0], 20, "post");
  snprintf( &epoch_name[14][0], 20, "wait");
  snprintf( &epoch_name[15][0], 20, "fence");
  snprintf( &epoch_name[16][0], 20, "lock_all");
  snprintf( &epoch_name[17][0], 20, "unlock_all");
  snprintf( &epoch_name[18][0], 20, "cswap");
  snprintf( &epoch_name[19][0], 20, "flush_all");
  snprintf( &epoch_name[20][0], 20, "fetch_and_op");
  snprintf( &epoch_name[21][0], 20, "accumulate");
  snprintf( &epoch_name[22][0], 20, "get_accumulate");

  init_papi();

  if (glob_info.debug_pe == 0) {
    timing_open_file();
  }

  /* start counter */
  PAPI_start( PapiEventSet);
  timing_record( -1 );
  timing_record( 0 );
}

void timing_close() {
  if (glob_info.debug_pe == 0) {
    timing_print( 1 );
  }

  cleanup_papi();

  if (glob_info.debug_pe == 0) {
    timing_close_file();
  }
}

//! records the timing for the given epoch id
//! if number of recorded timings exceed 1024, then the function empties
//! the buffer and they will be printed

void timing_record( int id ) {

  PAPI_stop( PapiEventSet, &PapiEventCounts[2*counter] );
  PAPI_reset( PapiEventSet );

  ids[counter] = id;

  if ( id > -1  ) { 
    if ( ++counter > 1024 ) {
      if (glob_info.debug_pe == 0) {
        timing_print(0);
      }
      counter = 0;
    }
  }
  PAPI_start( PapiEventSet );
}
#endif
