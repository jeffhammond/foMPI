// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __MYMPI_INTERNAL_H
#define __MYMPI_INTERNAL_H

#include <stdint.h> //int32_t uint64_t
#include <stdio.h> //TODO: remove comment printf exit
#include <stdlib.h>

//TODO: remove and switch macros
#ifdef NA
#define UGNI
#endif


#ifndef PAPI
//#define LIBPGT
//#define PAPI
#endif

/*
 * New Includes
 * */
#include <sys/utsname.h> //utsname data structure
#include <inttypes.h> //printf macro uint64_t
#include <errno.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h> //atoi
#include <unistd.h>
#include <time.h> //time for rand

#include "dmapp.h"

#ifdef XPMEM
#include <xpmem.h>
#include "libtopodisc.h"
#endif

/*
 * New Includes
 * */

#ifdef UGNI

#include <pmi.h>
#include <gni_pub.h>

#include "ugni_config.h"
#include "xpmem_config.h"
#include "fompi_notif_uq.h"

#endif

#ifdef LIBPGT
#include <libpgt.h>
#endif

#include "fompi_internal_ex.h"

#define MASTER 0

#define foMPI_MUTEX_NONE -1

#define _foMPI_TRUE 1
#define _foMPI_FALSE 0
#define _foMPI_REQUEST_PERSISTENT 0
#define _foMPI_REQUEST_NOT_PERSISTENT 1
#define _foMPI_PUT 0
#define _foMPI_GET 1
#define _foMPI_NO_MATCH 1

#ifndef NDEBUG
#define _foMPI_V_OPTION 2
#else
#define _foMPI_V_OPTION 0
#endif

/* data structures */
typedef int foMPI_Op;

/*foMPI Communicator*/

typedef struct foMPI_Comm_desc {
	/*Gen Info*/
	MPI_Comm mpi_comm;

#ifdef UGNI
	/*ugni related data structures*/
	/*communication domain handle */
	gni_cdm_handle_t cdm_handle;
	/*gni NIC handle*/
	gni_nic_handle_t nic_handle;
	/*endpoint handles array*/
	gni_ep_handle_t *endpoint_handles_array;
	/*source Completion queue handle*/
	gni_cq_handle_t cq_handle;
	/*size of Source completion queue*/
	uint32_t number_of_cq_entries;
	/*number of on-fly uGNI RMA transfer*/
	uint64_t counter_ugni_nbi;
	uint64_t ins_ind, extr_ind;
	gni_post_descriptor_t *data_desc_array;
	gni_post_descriptor_t **data_desc_ptr;
#endif

	/*id communicator (communication domain)*/
	int cmd_id;
	/*id device (needed by gni)*/
	int device_id;
	/* communicator details */
	int commsize;
	int commrank;
	int32_t *group_ranks;
	//is localaddress useful?
	unsigned int local_nic_address;

} foMPI_Comm_desc_t;

typedef foMPI_Comm_desc_t* _foMPI_Comm;

//foMPI segment descriptor that includes both gni and dmapp
typedef struct foMPI_Seg_desc {
	dmapp_seg_desc_t dmapp;
#ifdef UGNI
	gni_mem_handle_t ugni; /* read remote access */
#endif
#ifdef XPMEM
	//Meaningless!  all_to_all between all PEs
	//xpmem_segid_t xpmem;
#endif
	void *addr;
	uint64_t size;
} fompi_seg_desc_t;

/* local information of a remote window (Win_create/Win_allocate) */
typedef struct foMPI_Win_struct {
	fompi_seg_desc_t seg; /* foMPI segment descriptor for the exposed memory on the remote process */
	fompi_seg_desc_t win_ptr_seg; /* segment descriptor to the general window information on the remote process */
	fompi_seg_desc_t pscw_matching_exposure_seg; /* segment descriptor to PSCW exposure information on the remote process */
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
	fompi_seg_desc_t seg; /* foMPI segment descriptor for the exposed memory on the remote process */
	fompi_seg_desc_t next_seg; /* segement descirptor to the next memory region in the list */

	/* pointer to the next memory region in the list
	 * in case of the local list of memory regions the segment descriptor is not used */
	struct foMPI_Win_dynamic_element* next;
	MPI_Aint base;
	MPI_Aint size;
} foMPI_Win_dynamic_element_t;

/* local information of a remote window (Win_create_dynamic) */
typedef struct foMPI_Win_dynamic_struct {
	fompi_seg_desc_t win_ptr_seg; /* segment descriptor to the general window information on the remote process */
	fompi_seg_desc_t pscw_matching_exposure_seg; /* segment descriptor to PSCW exposure information on the remote process */

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
	fompi_seg_desc_t seg;
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
	//added support for notifications
	xpmem_seg_desc_t notif_queue;
	xpmem_seg_desc_t notif_queue_state;
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
	//added support for notifications
	uint64_t* notif_queue;
	xpmem_apid_t notif_queue_apid;
	int notif_queue_offset;
	uint64_t* notif_queue_state;
	xpmem_apid_t notif_queue_state_apid;
	int notif_queue_state_offset;
} fompi_xpmem_addr_t;

typedef volatile uint32_t lock_flags_t;

typedef struct fompi_xpmem_notif {
	//fixed fields
	uint16_t tag;
	uint16_t source;
	size_t size;
	size_t target_addr;
} fompi_xpmem_notif_t; //it has to be large as a cache line

typedef struct fompi_xpmem_notif_ex {
	//fixed fields
	fompi_xpmem_notif_t n;
	//variable
	char buff[_foMPI_XPMEM_NOTIF_INLINE_BUFF_SIZE];
} fompi_xpmem_notif_ex_t; //it has to be large as a cache line

typedef struct fompi_xpmem_notif_queue{
	volatile fompi_xpmem_notif_ex_t queue_array[_foMPI_XPMEM_NUM_DST_CQ_ENTRIES];
} fompi_xpmem_notif_queue_t;

typedef struct fompi_xpmem_notif_state{
	volatile int tail;
	volatile int count;
	volatile uint32_t inser_ind; //used inside the lock section, should be considered volatile?
	//volatile fompi_xpmem_notif_ex_t queue_array[_foMPI_XPMEM_NUM_DST_CQ_ENTRIES];
} fompi_xpmem_notif_state_t;

#endif

/* global window attributes */
typedef struct foMPI_Win_desc {
#ifdef XPMEM
	/* infos of the exported memory regions */
	fompi_xpmem_info_t xpmem_segdesc;
#endif
	/* pointer and segment descriptor to the local dynamic memory regions */
	fompi_seg_desc_t win_dynamic_mem_regions_seg; /* read remote access */
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
	uint32_t nbi_counter; /* number of outstanding dmapp operations with global synchronization (fence/lock) */
	uint32_t local_exclusive_count; /* number of exclusive lock this process holds */
	/* specific information about the communicator associated with the window */
	MPI_Comm comm;
	_foMPI_Comm fompi_comm;
#ifdef XPMEM
	MPI_Comm win_onnode_comm;
	MPI_Group win_onnode_group;
	int onnode_lower_bound;
	int onnode_upper_bound;
	int onnode_size;
	volatile long xpmem_global_counter;
	//Notification queue ptr
	fompi_xpmem_notif_queue_t *xpmem_notif_queue;
	void *xpmem_notif_state_lock;
	uint32_t xpmem_queue_extract_ind;
#endif
	int commsize;
	int commrank;
	char create_flavor;
	/*char model; don't used at the moment */
	/*char assert; don't used at the moment */
#ifdef UGNI
	/*Data structures completion queue*/
	gni_cq_handle_t destination_cq_handle;
	gni_cq_handle_t source_cq_handle;
	fompi_notif_uq_t *destination_cq_discarded;
	uint32_t number_of_dest_cq_entries;
	uint32_t number_of_source_cq_entries;
	/*number of on-fly uGNI RMA transfer*/
	uint64_t counter_ugni_nbi;

#endif
} foMPI_Win_desc_t;

typedef foMPI_Win_desc_t* foMPI_Win;

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

/*
 * Section Modified DS
 * */

/* structure for the request based rma operations */
typedef struct foMPI_Rma_request {
	foMPI_Win win; /* for the deletion of the element from the queue */
	/* to fill the elements of MPI_Status structure */
	uint32_t elements;
	MPI_Datatype datatype;
	int source;
} foMPI_Rma_request_t;

typedef struct foMPI_Notification_request {
	foMPI_Win win;
	int source_rank;
	int tag;
	int count;
	int found;
} foMPI_Notification_request_t;

/*Structure fo the generic request object*/
typedef struct foMPI_Request_desc {
	int (*test_fun)(void *extra_state, int *flag, MPI_Status *status);
	int (*wait_fun)(void *extra_state, MPI_Status *status);
	/*used by MPI_START if PERMANENT*/
	int (*activate_fun)(void *extra_state);
	/*used by MPI_CANCEL */
	int (*cancel_fun)(void *extra_state, int complete);
	/*used by MPI_REQUEST_FREE*/
	int (*free_fun)(void *extra_state);
	void* extra_state;
	int type; //PERMANENT or NOT
	int active;
} foMPI_Request_t;

typedef foMPI_Request_t* foMPI_Request;

/*Data Structures of global variables*/
typedef struct PMI_job_desc {
	int first_spawned;
	int number_of_ranks;
	int rank_id;
	struct utsname uts_info;
	int ptag;
	int cookie;
} PMI_job_desc_t;

typedef struct fortran_free_mem {
	struct fortran_free_mem* next;
	int index;
} fortran_free_mem_t;

typedef struct foMPI_global {
	_foMPI_Comm comm_world;
	/* global variables foMPI */
	/* TODO: delete */
	int debug_pe;
	unsigned int rand_seed;

	dcache_blocks_t dcache_blocks[10]; /* FIFO based cache */
	int dcache_last_entry;
	int dcache_origin_last_entry;
	int dcache_target_last_entry;

	/* Fortran handling variables */
	foMPI_Win *Fortran_Windows;
	foMPI_Request *Fortran_Requests;
	int Fortran_win_sz;
	int Fortran_req_sz;

	fortran_free_mem_t *Fortran_free_win;
	fortran_free_mem_t *Fortran_free_req;

	/*DMAPP:*/
	/*this variable is used?*/
	/*num of non blocking implicit dmapp calls on-the-fly*/
	int counter_dmapp_nbi;
	int v_option;
	int num_ugni_comm_instances;

#ifdef XPMEM
	MPI_Group onnode_group;
#endif
#ifdef UGNI
	PMI_job_desc_t curr_job;
	int default_ugni_dev_id;
	int default_ugni_rdma_modes;
#endif

} foMPI_global_t;

/* global data structure */
extern foMPI_global_t glob_info;
/*
 * End Section
 * */

/*!
 * copied from DMAPP_GOAL code written by Timo Schneider and Torsten Hoefler
 Compare the two return values. If they are same do nothing, if they differ try
 to give an explanation why and exit.

 @params[in] actual    return value returned by the last call to a dmapp function
 @params[in] expected  return value which was expected
 */

inline static void _check_dmapp_status(dmapp_return_t actual, dmapp_return_t expected, char* file,
		int line) {

#ifndef NDEBUG

	if (actual == expected)
		return;

	char* expl = NULL;
	dmapp_explain_error(actual, (const char**) &expl);
	fprintf(stderr, "Error on rank %i in %s line %i: %s\n", glob_info.debug_pe, file, line, expl);
	abort();

#endif

}

inline static void _check_gni_status(gni_return_t actual, gni_return_t expected, char* file,
		int line) {

#ifndef NDEBUG

	if (actual == expected)
		return;

	fprintf(stderr, "Error on rank %i in %s line %i: %s\n", glob_info.debug_pe, file, line,
			gni_err_str[actual]);
	abort();

#endif

}

inline static void _check_MPI_status(int actual, int expected, char* file, int line) {

#ifndef NDEBUG

	if (actual == expected)
		return;
	char str[MPI_MAX_ERROR_STRING];
	int resultlen;
	MPI_Error_string(actual, &str[0], &resultlen);
	fprintf(stderr, "Error on rank %i in %s line %i: %s\n", glob_info.debug_pe, file, line, str);
	abort();

#endif

}

/*used in init and finalize */
int _foMPI_Comm_create(MPI_Comm comm, _foMPI_Comm *newcomm);
int _foMPI_Comm_free(_foMPI_Comm *communicator);
inline gni_post_descriptor_t * _foMPI_Comm_get_ugni_data_descriptor(_foMPI_Comm comm);

/*register and unregister memory seg with nic*/
void _foMPI_mem_register(void *addr, uint64_t size, fompi_seg_desc_t *seg, foMPI_Win win);
void _foMPI_mem_unregister(fompi_seg_desc_t *seg, foMPI_Win win);
inline int _foMPI_is_addr_in_seg(void * addr, fompi_seg_desc_t *seg);

/*flushing ugni ops*/
inline int _foMPI_Comm_flush_all_internal(_foMPI_Comm communicator);
/* used in lock and the rma calls */
void foMPI_Release_window_mutex(dmapp_return_t* status, dmapp_pe_t target_pe,
		foMPI_Win_desc_t* win_ptr, dmapp_seg_desc_t* win_ptr_seg, foMPI_Win win);
void foMPI_Set_window_mutex(dmapp_return_t* status, dmapp_pe_t target_pe, foMPI_Win_desc_t* win_ptr,
		dmapp_seg_desc_t* win_ptr_seg, foMPI_Win win);

int foMPI_RMA_op(void *buf3, void *buf1, void *buf2, foMPI_Op op, MPI_Datatype type, int count);

int get_free_elem(fortran_free_mem_t **list_head);
inline int _foMPI_EncodeID(uint16_t rank, uint16_t id_msg, uint32_t *id_encoded);
inline int _foMPI_DecodeID(uint16_t *rank, uint16_t *id_msg, uint32_t id_encoded);

#ifdef XPMEM
void xpmem_notif_init_queue(foMPI_Win win, int onnode_size);
void xpmem_notif_free_queue(foMPI_Win win);
inline int xpmem_notif_push(int16_t rank, int16_t tag, int target_rank, int target_local_rank, foMPI_Win win);
inline int xpmem_notif_push_and_data(int source_rank, MPI_Aint target_offset, size_t size, int target_rank, int target_local_rank, MPI_Aint origin_offset, int tag, foMPI_Win win);
inline int xpmem_notif_pop(int16_t *rank, int16_t *tag, foMPI_Win win);
inline void sse_memcpy(char *to, const char *from, size_t len);

xpmem_seg_desc_t foMPI_export_memory_xpmem(void *ptr, int len);
void* foMPI_map_memory_xpmem(xpmem_seg_desc_t rseg, int len, xpmem_apid_t* apid, int* offset);
void foMPI_unmap_memory_xpmem(xpmem_apid_t* apid, void* addr, int offset);
void foMPI_unexport_memory_xpmem(xpmem_seg_desc_t rseg);
int foMPI_onnode_rank_global_to_local(int grank, foMPI_Win win);
int foMPI_onnode_rank_local_to_global(int lrank, foMPI_Win win);
#endif

#ifdef PAPI
void timing_init();
void timing_close();
void timing_record(int id);
#endif

#endif
