#ifndef __MYMPI_INTERNAL_H
#define __MYMPI_INTERNAL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dmapp.h"

#ifdef XPMEM
#include <xpmem.h>
#endif

#define MASTER 0

#define foMPI_MUTEX_NONE -1

/* data structures */
typedef int foMPI_Op;

/* local information of a remote window (Win_create/Win_allocate) */
typedef struct foMPI_Win_struct {
  dmapp_seg_desc_t seg; /* dmapp segment descriptor for the exposed memory on the remote process */
  dmapp_seg_desc_t win_ptr_seg; /* segment descriptor to the general window information on the remote process */
  dmapp_seg_desc_t pscw_matching_exposure_seg; /* segment descripter to PSCW exposure information on the remote process */
  void* base;
  /* pointer to the general window information on the remote process, 
   * so that one can manipulate "global" visible information on that window (mutex/lock/...) */
  struct foMPI_Win_desc* win_ptr;
  /* pointer to PSCW exposure information on the remote process 
   * it is possible to move the general window information, but then there is one indirection more
   * (first access on the window information, then access on the PSCW exposure data)
   * could be smaller, but to access with dmapp atomics a uint64_t is used */
  uint64_t* pscw_matching_exposure;
  MPI_Aint size;
  uint32_t disp_unit;
} foMPI_Win_struct_t;

/* list element of a memory region in the Win_dynamic_create case */
/* TODO: in the current implementation, there is a different element for every memory region, and so there is one remote access per region
 *       but the informations are cached until the change of the win->dynamic_id
 *       maybe a better tradeof is that one element consists of a array of more than one memory regions */
typedef struct foMPI_Win_dynamic_element {
  dmapp_seg_desc_t seg; /* dmapp segment descriptor for the exposed memory on the remote process */
  dmapp_seg_desc_t next_seg; /* segement descirptor to the next memory region in the list */
  /* pointer to the next memory region in the list 
   * in case of the local list of memory regions the segment descriptor is not used */
  struct foMPI_Win_dynamic_element* next;
  MPI_Aint base;
  MPI_Aint size;
} foMPI_Win_dynamic_element_t;

/* local information of a remote window (Win_create_dynamic) */
typedef struct foMPI_Win_dynamic_struct {
  dmapp_seg_desc_t win_ptr_seg; /* segment descriptor to the general window information on the remote process */
  dmapp_seg_desc_t pscw_matching_exposure_seg; /* segment descripter to PSCW exposure information on the remote process */
  foMPI_Win_dynamic_element_t* remote_mem_regions; /* local list (cached) of remote memory regions */
  /* pointer to the general window information on the remote process,
   * so that one can manipulate "global" visible information on that window (mutex/lock/...) */
  struct foMPI_Win_desc* win_ptr;
  /* pointer to PSCW exposure information on the remote process
   * it is possible to move the general window information, but then there is one indirection more
   * (first access on the window information, then access on the PSCW exposure data)
   * array of size commsize, counts the exposure epochs with each process
   * used to match the current access id against the exposure id 
   * could be smaller, but to access with dmapp atomics a uint64_t is used */
  uint64_t* pscw_matching_exposure;
  uint32_t dynamic_id; /* id of the cached data */
} foMPI_Win_dynamic_struct_t;

/* dynamic window: to fetch a pointer and segment descriptor combo for the pointer to first remote memory region */ 
typedef struct foMPI_Win_ptr_seg_comb {
  dmapp_seg_desc_t seg;
  void* ptr;
} foMPI_Win_ptr_seg_comb_t;

typedef struct foMPI_Win_excl_lock_elem {
  struct foMPI_Win_excl_lock_elem* next;
  int rank;
} foMPI_Win_excl_lock_elem_t;

#ifdef XPMEM
typedef struct xpmem_seg_desc {
  xpmem_segid_t seg;
  int offset;
} xpmem_seg_desc_t;

typedef struct fompi_xpmem_info {
  xpmem_seg_desc_t base;
  xpmem_seg_desc_t win_ptr;
  xpmem_seg_desc_t pscw_matching_exposure;
} fompi_xpmem_info_t;

typedef struct fompi_xpmem_addr {
  void* base;
  xpmem_apid_t base_apid;
  int base_offset;
  struct foMPI_Win_desc* win_ptr;
  xpmem_apid_t win_ptr_apid;
  int win_ptr_offset;
  uint64_t* pscw_matching_exposure;
  xpmem_apid_t pscw_matching_exposure_apid;
  int pscw_matching_exposure_offset;
} fompi_xpmem_addr_t;
#endif

/* global window attributes */
typedef struct foMPI_Win_desc {
#ifdef XPMEM
  /* infos of the exported memory regions */
  fompi_xpmem_info_t xpmem_segdesc;
#endif
  /* pointer and segment descriptor to the local dynamic memory regions */
  dmapp_seg_desc_t win_dynamic_mem_regions_seg; /* read remote access */
  struct foMPI_Win_dynamic_element* win_dynamic_mem_regions; /* read remote access */
  /* pointer to window information of the remote processes (Win_create/Win_allocate) */
  foMPI_Win_struct_t* win_array;
#ifdef XPMEM
  /* pointer to the XPMEM information of the onnode processes */
  fompi_xpmem_addr_t* xpmem_array;
  /* pointer to array with the ranks (of the window communicator) of the onnode processes 
   * only valid if don't have one consecutive range (onnode_lower_bound = -1, onnode_upper_bound) */
  int* onnode_ranks;
#endif
  /* pointer to window information of the remote processes (Win_dynamic_create) */
  foMPI_Win_dynamic_struct_t* dynamic_list;
  /* pointer to the last element of local memory regions in the list
   * for fast insert */
  struct foMPI_Win_dynamic_element* dynamic_last;
  int* pscw_access_group_ranks; /* array of the global ranks (in MPI_COMM_WORLD) of the processes in the access group */
  uint32_t* pscw_matching_access; /* array of size commsize, counts the access epochs with each process */
  /* list of the acquired exclusive locks */
  foMPI_Win_excl_lock_elem_t* excl_locks;
  int* group_ranks; /* array of the global ranks (in MPI_COMM_WORLD) in the communicator */
  /* assigned name for the window */
  char* name;
  /* mutex for all lock access on this process
   * LSB is reserved for an exclusive lock
   * the other bits are the sum of shared locks on this process */
  volatile uint64_t lock_mutex;
  /* mutex for lock_all access on the window
   * LSB is set if there is one exclusive lock on one of the processes in the window 
   * the other bits are the sum of all lock_all locks on the window 
   * the value of this variable is only significat on MASTER */
  volatile uint64_t lock_all_mutex;
  /* mutex for lock synchronisation and accumulate operations
   * foMPI_MUTEX_NONE if no is holding the mutex, pe if process pe holds the mutex */
  /* TODO: exclusive lock conflicts with a accumulate */
  /* TODO: Can this lock be removed? */
  volatile int64_t mutex; /* read/write remote access */
  /* global counter for collective operations:
   * - counter of processes that have concluded their access epoch
   *   it will be matched against the size of exposure group */
  volatile uint64_t global_counter; /* read/write remote access */
  /* mutex to protect the access to the dynamic memory regions
   * foMPI_MUTEX_NONE if no is holding the mutex, pe if process pe holds the mutex */
  volatile int64_t dynamic_mutex; /* read/write remote access */
  /* current id of the local memory regions, will be incremented */
  uint32_t dynamic_id; /* read remote access */
  uint32_t pscw_access_group_size; /* pscw_access_group_size = size of the access group */
  uint32_t pscw_exposure_group_size; /* pscw_exposure_group_size = size of the exposure group */
  uint32_t nbi_counter; /* number of outstanding dmapp operations with global synchronisation (fence/lock) */
  uint32_t local_exclusive_count; /* number of exclusive lock this process holds */
  /* specific information about the communicator associated with the window */
  MPI_Comm comm;
#ifdef XPMEM
  MPI_Comm win_onnode_comm;
  MPI_Group win_onnode_group;
  int onnode_lower_bound;
  int onnode_upper_bound;
  int onnode_size;
  volatile long xpmem_global_counter;
#endif
  int commsize;
  int commrank; 
  char create_flavor;
  /*char model; don't used at the moment */
  /*char assert; don't used at the moment */
} foMPI_Win_desc_t;

typedef foMPI_Win_desc_t* foMPI_Win;

/* structure for the request based rma operations */
typedef struct foMPI_Win_request {
  foMPI_Win win; /* for the deletion of the element from the queue */
  /* to fill the elements of MPI_Status structure */
  uint32_t elements;
  MPI_Datatype datatype;
  int source;
} foMPI_Win_request_t;

typedef foMPI_Win_request_t* foMPI_Request;

/* structure for basic datatype detection, needed for the accumulate functions */
typedef struct dtype_list {
  struct dtype_list* next;
  MPI_Datatype type;
} dtype_list_t;

typedef struct dcache_blocks_struct {
  MPI_Aint* displ;
  int* blocklen;
  MPI_Aint number_blocks;
  MPI_Aint block_offset;
  MPI_Datatype type;
  int type_count;
  int pos;
  int block_remaining_size;
} dcache_blocks_t;

/* global variables */
extern MPI_Group onnode_group;
/* TODO: delete */
extern int debug_pe;

extern dcache_blocks_t dcache_blocks[10]; /* FIFO based cache */
extern int dcache_last_entry;
extern int dcache_origin_last_entry;
extern int dcache_target_last_entry;

/* Fortran handling variables */
extern foMPI_Win *Fortran_Windows;
extern foMPI_Request *Fortran_Requests;
extern int Fortran_win_sz;
extern int Fortran_req_sz;

typedef struct fortran_free_mem {
  struct fortran_free_mem* next;
  int index;
}  fortran_free_mem_t;

extern fortran_free_mem_t *Fortran_free_win;
extern fortran_free_mem_t *Fortran_free_req;

/*!
 * copied from DMAPP_GOAL code written by Timo Schneider and Torsten Hoefler
Compare the two return values. If they are same do nothing, if they differ try
to give an explanation why and exit.

@params[in] actual    return value returned by the last call to a dmapp function
@params[in] expected  return value which was expected
*/

inline static void _check_status(dmapp_return_t actual, dmapp_return_t expected, char* file, int line) {

#ifndef NDEBUG

	if (actual == expected) return;

	char* expl = NULL;
	dmapp_explain_error(actual, (const char**) &expl);
	fprintf(stderr, "Error on rank %i in %s line %i: %s\n", debug_pe, file, line, expl);
	exit(EXIT_FAILURE);

#endif

}

/* used in lock and the rma calls */
void foMPI_Release_window_mutex( dmapp_return_t* status, dmapp_pe_t target_pe, foMPI_Win_desc_t* win_ptr, dmapp_seg_desc_t* win_ptr_seg, foMPI_Win win );
void foMPI_Set_window_mutex( dmapp_return_t* status, dmapp_pe_t target_pe, foMPI_Win_desc_t* win_ptr, dmapp_seg_desc_t* win_ptr_seg, foMPI_Win win );

int foMPI_RMA_op(void *buf3, void *buf1, void *buf2, foMPI_Op op, MPI_Datatype type, int count);

int get_free_elem( fortran_free_mem_t **list_head );

#ifdef XPMEM
  xpmem_seg_desc_t foMPI_export_memory_xpmem(void *ptr, int len);
  void* foMPI_map_memory_xpmem(xpmem_seg_desc_t rseg, int len, xpmem_apid_t* apid, int* offset ); 
  void foMPI_unmap_memory_xpmem( xpmem_apid_t* apid, void* addr, int offset );
  void foMPI_unexport_memory_xpmem( xpmem_seg_desc_t rseg );
  int foMPI_onnode_rank_global_to_local( int grank, foMPI_Win win );
  int foMPI_onnode_rank_local_to_global( int lrank, foMPI_Win win );
#endif

#ifdef PAPI
  void timing_init();
  void timing_close();
  void timing_record( int id );
#endif

#endif
