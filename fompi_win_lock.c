// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
//#include <intrinsics.h> 
#include <string.h>
#include "fompi.h"

#ifdef UGNI
#ifdef UGNI_WIN_RELATED_SRC_CQ
static inline int ugni_Dequeue_local_event(foMPI_Win win);
static inline int _foMPI_Win_uGNI_flush_all(foMPI_Win win);
#endif
#endif

/* We have to keep track of which locks are exclusive.
 * To implement it in a scalable way (without O(p) memory overhead), we use a list.
 * We insert at the head of the list */
inline static void foMPI_insert_excl_lock(int rank, foMPI_Win win) {

	foMPI_Win_excl_lock_elem_t* new_elem = _foMPI_ALLOC(sizeof(foMPI_Win_excl_lock_elem_t));
	new_elem->rank = rank;

	new_elem->next = win->excl_locks;
	win->excl_locks = new_elem;

}

/* functions assumes that we always will find the rank */
inline static void foMPI_delete_excl_lock(int rank, foMPI_Win win) {

	foMPI_Win_excl_lock_elem_t* previous;
	foMPI_Win_excl_lock_elem_t* current;

	assert(win->excl_locks != NULL);

	if (win->excl_locks->rank == rank) { /* remove head element */
		current = win->excl_locks;
		win->excl_locks = win->excl_locks->next;
		_foMPI_FREE(current);
	} else {

		previous = win->excl_locks;
		current = win->excl_locks->next;

		while (current->rank != rank) {
			previous = current;
			current = current->next;
		}

		previous->next = current->next;
		_foMPI_FREE(current);
	}

}

inline static int foMPI_search_excl_lock(int rank, foMPI_Win win) {

	if (win->excl_locks == NULL) /* no exclusive locks */
		return foMPI_LOCK_SHARED;

	foMPI_Win_excl_lock_elem_t* current = win->excl_locks;

	while ((current->rank != rank) && (current->next != NULL)) {
		current = current->next;
	}

	if (current->rank == rank) {
		return foMPI_LOCK_EXCLUSIVE;
	}

	return foMPI_LOCK_SHARED;
}

/* TODO: only does busy waiting, should be doing at least some queueing */
/* TODO: asserts unused */
int foMPI_Win_lock(int lock_type, int rank, int assert, foMPI_Win win) {
#ifdef PAPI
	timing_record( -1 );
#endif
	dmapp_return_t status;
	uint64_t result;
	uint64_t bit_mask = ~(uint64_t) 0 << 32;

	foMPI_Win_desc_t* win_ptr;
	dmapp_seg_desc_t* win_ptr_seg;
	foMPI_Win_desc_t* win_ptr_master;
	dmapp_seg_desc_t* win_ptr_seg_master;

	assert((lock_type == foMPI_LOCK_SHARED) || (lock_type == foMPI_LOCK_EXCLUSIVE));

	if (rank == MPI_PROC_NULL) {
		return MPI_SUCCESS;
	}

	assert((rank >= 0) && (rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[rank];

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		win_ptr = win->dynamic_list[rank].win_ptr;
		win_ptr_seg = &(win->dynamic_list[rank].win_ptr_seg.dmapp);
		win_ptr_master = win->dynamic_list[MASTER].win_ptr;
		win_ptr_seg_master = &(win->dynamic_list[MASTER].win_ptr_seg.dmapp);
	} else {
		win_ptr = win->win_array[rank].win_ptr;
		win_ptr_seg = &(win->win_array[rank].win_ptr_seg.dmapp);
		win_ptr_master = win->win_array[MASTER].win_ptr;
		win_ptr_seg_master = &(win->win_array[MASTER].win_ptr_seg.dmapp);
	}

	if (lock_type == foMPI_LOCK_EXCLUSIVE) {
		if (win->local_exclusive_count == 0) {
			/* check if lock_all mutex is set on the master */
			/* if not, then acquire lock_all mutex */
			/* TODO: howto to count exclusive locks in the system? */
			dmapp_pe_t master_pe = (dmapp_pe_t) win->group_ranks[MASTER];

			do {
				/* check if lock_all mutex is set on the master */
				/* if not, then acquire lock_all mutex */
				/* TODO: Are we in trouble, because of int64_t operands in acswap and uint64_t variable? */
				status = dmapp_afadd_qw(&result, (void*) &(win_ptr_master->lock_all_mutex),
						win_ptr_seg_master, master_pe, (int64_t) 1);
				_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

				if (!(result & bit_mask)) {
					/* try to acquire exclusive on target */
					status = dmapp_acswap_qw(&result, (void*) &(win_ptr->lock_mutex), win_ptr_seg,
							target_pe, (int64_t) 0, (int64_t) 1);
					_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

					/* if not acquired back off: release lock_all mutex  */
					if (result == 0) {
						break;
					} else {
						/* TODO: non-blocking? */
						status = dmapp_aadd_qw((void*) &(win_ptr_master->lock_all_mutex),
								win_ptr_seg_master, master_pe, (int64_t) -1);
						_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
					}
				} else {
					/* back off on master lock_all mutex */
					/* TODO: non-blocking? */
					status = dmapp_aadd_qw((void*) &(win_ptr_master->lock_all_mutex),
							win_ptr_seg_master, master_pe, (int64_t) -1);
					_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
				}
			} while (1);
			win->local_exclusive_count++;
		} else {
			/* we already hold the global exclusive lock */
			/* try to acquire exclusive on target */
			do {
				status = dmapp_acswap_qw(&result, (void*) &(win_ptr->lock_mutex), win_ptr_seg,
						target_pe, (int64_t) 0, (int64_t) 1);
				_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
			} while (result != 0);
			win->local_exclusive_count++;
		}

		foMPI_insert_excl_lock(rank, win);

	} else { /* foMPI_LOCK_SHARED */
		/* propagate read intention for target process */
		/* TODO: non-blocking? */
		status = dmapp_afadd_qw((void*) &result, (void*) &(win_ptr->lock_mutex), win_ptr_seg,
				target_pe, (int64_t) 2);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

		/* check if there is an exclusive lock */
		while (result & 1) {
			status = dmapp_get(&result, (void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe, 1,
					DMAPP_QW);
			_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		};
	}

#ifdef PAPI
	timing_record( 10 );
#endif
	return MPI_SUCCESS;
}

int foMPI_Win_unlock(int rank, foMPI_Win win) {
#ifdef PAPI
	timing_record( -1 );
#endif
	dmapp_return_t status;
	foMPI_Win_desc_t* win_ptr;
	dmapp_seg_desc_t* win_ptr_seg;
	foMPI_Win_desc_t* win_ptr_master;
	dmapp_seg_desc_t* win_ptr_seg_master;
	int lock_type;

	if (rank == MPI_PROC_NULL) {
		return MPI_SUCCESS;
	}

	assert((rank >= 0) && (rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[rank];

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		win_ptr = win->dynamic_list[rank].win_ptr;
		win_ptr_seg = &(win->dynamic_list[rank].win_ptr_seg.dmapp);
		win_ptr_master = win->dynamic_list[MASTER].win_ptr;
		win_ptr_seg_master = &(win->dynamic_list[MASTER].win_ptr_seg.dmapp);
	} else {
		win_ptr = win->win_array[rank].win_ptr;
		win_ptr_seg = &(win->win_array[rank].win_ptr_seg.dmapp);
		win_ptr_master = win->win_array[MASTER].win_ptr;
		win_ptr_seg_master = &(win->win_array[MASTER].win_ptr_seg.dmapp);
	}

#ifdef PAPI
	timing_record( 8 );
#endif

	lock_type = foMPI_search_excl_lock(rank, win);

	foMPI_Win_flush(rank, win);

	/* release the lock(s) */
	if (lock_type == foMPI_LOCK_EXCLUSIVE) {
		dmapp_pe_t master_pe = (dmapp_pe_t) win->group_ranks[MASTER];

		/* release lock on target */
		/* TODO: non-blocking? */
		status = dmapp_aadd_qw((void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe,
				(int64_t) -1);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

		win->local_exclusive_count--;

		if (win->local_exclusive_count == 0) {
			/* release lock on master process */
			status = dmapp_aadd_qw_nbi((void*) &(win_ptr_master->lock_all_mutex),
					win_ptr_seg_master, master_pe, (int64_t) -1);
			_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		}

		foMPI_delete_excl_lock(rank, win);

	} else { /* foMPI_LOCK_SHARED */
		/* release lock on target */
		status = dmapp_aadd_qw_nbi((void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe,
				(int64_t) -2);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	}

#ifdef PAPI
	timing_record( 9 );
#endif

	return MPI_SUCCESS;
}

int foMPI_Win_lock_all(int assert, foMPI_Win win) {
#ifdef PAPI
	timing_record( -1 );
#endif

	dmapp_return_t status;
	uint64_t result;
	uint64_t bit_mask = ~(~(uint64_t) 0 << 32);

	foMPI_Win_desc_t* win_ptr_master;
	dmapp_seg_desc_t* win_ptr_seg_master;
	dmapp_pe_t master_pe = (dmapp_pe_t) win->group_ranks[MASTER];

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		win_ptr_master = win->dynamic_list[MASTER].win_ptr;
		win_ptr_seg_master = &(win->dynamic_list[MASTER].win_ptr_seg.dmapp);
	} else {
		win_ptr_master = win->win_array[MASTER].win_ptr;
		win_ptr_seg_master = &(win->win_array[MASTER].win_ptr_seg.dmapp);
	}

	status = dmapp_afadd_qw((void*) &result, (void*) &(win_ptr_master->lock_all_mutex),
			win_ptr_seg_master, master_pe, (int64_t) 4294967296); /* 2^32 = 4294967296 */
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

	while (result & bit_mask) {
		status = dmapp_get(&result, (void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master,
				master_pe, 1, DMAPP_QW);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	}

#ifdef PAPI
	timing_record( 16 );
#endif

	return MPI_SUCCESS;
}

int foMPI_Win_unlock_all(foMPI_Win win) {
#ifdef PAPI
	timing_record( -1 );
#endif

	dmapp_return_t status;

	foMPI_Win_desc_t* win_ptr_master;
	dmapp_seg_desc_t* win_ptr_seg_master;

	/* target and local completion */
	foMPI_Win_flush_all(win);

	/* release lock on master process */
	dmapp_pe_t master_pe = (dmapp_pe_t) win->group_ranks[MASTER];

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		win_ptr_master = win->dynamic_list[MASTER].win_ptr;
		win_ptr_seg_master = &(win->dynamic_list[MASTER].win_ptr_seg.dmapp);
	} else {
		win_ptr_master = win->win_array[MASTER].win_ptr;
		win_ptr_seg_master = &(win->win_array[MASTER].win_ptr_seg.dmapp);
	}
	status = dmapp_aadd_qw_nbi((void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master,
			master_pe, (int64_t) -4294967296); /* 2^32 = 4294967296 */
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

#ifdef PAPI
	timing_record( 17 );
#endif

	return MPI_SUCCESS;
}

/* TODO: write a test for the flush functions */

int foMPI_Win_flush(int rank, foMPI_Win win) {
	if (rank == MPI_PROC_NULL) {
		return MPI_SUCCESS;
	}

	assert((rank >= 0) && (rank < win->commsize));
	return foMPI_Win_flush_all(win);
}

int foMPI_Win_flush_local(int rank, foMPI_Win win) {
	return foMPI_Win_flush(rank, win);
}

int foMPI_Win_flush_all(foMPI_Win win) {
#ifdef PAPI
	timing_record( -1 );
#endif
	dmapp_return_t status_dmapp;
#ifdef UGNI
	int res;
#ifdef UGNI_WIN_RELATED_SRC_CQ
	res = _foMPI_Win_uGNI_flush_all(win);
#else
	res = _foMPI_Comm_flush_all_internal(win->fompi_comm);
#endif
	_check_MPI_status(res, MPI_SUCCESS, (char*) __FILE__, __LINE__);
#endif

#ifdef XPMEM
	/* XPMEM completion */
	__sync_synchronize();
#endif

	/* completion */
	if (win->nbi_counter > 0) {
		status_dmapp = dmapp_gsync_wait();
		_check_dmapp_status(status_dmapp, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		win->nbi_counter = 0;
	}

#ifdef PAPI
	timing_record( 4 );
#endif

	return MPI_SUCCESS;
}

int foMPI_Win_flush_local_all(foMPI_Win win) {
	return foMPI_Win_flush_all(win);
}

int foMPI_Win_sync(foMPI_Win win) {
	return MPI_SUCCESS;
}

#ifdef UGNI

#ifdef UGNI_WIN_RELATED_SRC_CQ
static inline int _foMPI_Win_uGNI_flush_all(foMPI_Win win) {
	/*
	 * Check the completion queue to verify that the message request has
	 * been sent.  The source completion queue needs to be checked and
	 * events to be removed so that it does not become full and cause
	 * succeeding calls to PostCqWrite to fail.
	 */

	/*
	 * Get all of the data completion queue events.
	 */
	while (win->counter_ugni_nbi > 0) {

		_foMPI_TRACE_LOG(3, "checking %"PRIu64" CQ events\n",
				win->counter_ugni_nbi);
		ugni_Dequeue_local_event(win);
	}

	_foMPI_TRACE_LOG(1, "_foMPI_Win_uGNI_flush_all\n");
	return MPI_SUCCESS;
}

static inline int ugni_Dequeue_local_event(foMPI_Win win) {

	gni_post_descriptor_t *event_post_desc_ptr;

	gni_cq_entry_t event_data = 0;
	uint64_t event_type;
	gni_return_t status = GNI_RC_NOT_DONE;

	/*
	 * Check the completion queue to verify that the message request has
	 * been sent.  The source completion queue needs to be checked and
	 * events to be removed so that it does not become full and cause
	 * succeeding calls to PostRdma to fail.
	 */
	while (status == GNI_RC_NOT_DONE) {

		/*
		 * Get the next event from the specified completion queue handle.
		 */

		status = GNI_CqGetEvent(win->source_cq_handle, &event_data);
		if (status == GNI_RC_SUCCESS) {

			//event_data; CONTAINS THE EVENT INFOS

			/*
			 * Processed event succesfully.
			 */

			event_type = GNI_CQ_GET_TYPE(event_data);

			if (event_type == GNI_CQ_EVENT_TYPE_POST || event_type == GNI_CQ_EVENT_TYPE_MSGQ) {
				_foMPI_TRACE_LOG(5,
						"GNI_CqGetEvent    source      type: %lu inst_id: %lu tid: %lu event: 0x%16.16lx\n",
						event_type, GNI_CQ_GET_INST_ID(event_data), GNI_CQ_GET_TID(event_data),
						event_data);

				/* process received event */
				status = GNI_GetCompleted(win->source_cq_handle, event_data, &event_post_desc_ptr);
				validate_gni_return(status, "GNI_GetCompleted");

				win->counter_ugni_nbi--;
				_foMPI_FREE((void * ) event_post_desc_ptr);

				_foMPI_TRACE_LOG(3,
						"GNI_CqGetCompleted    source      type: %lu inst_id: %lu tid: %lu event: 0x%16.16lx\n",
						event_type, GNI_CQ_GET_INST_ID(event_data), GNI_CQ_GET_TID(event_data),
						event_data);

			} else {
				_foMPI_ERR_LOG(
						"[%s] Rank: %4i GNI_CqGetEvent    source      type: %lu inst_id: %lu event: 0x%16.16lx\n",
						glob_info.curr_job.uts_info.nodename, glob_info.curr_job.rank_id,
						event_type,
						GNI_CQ_GET_DATA(event_data), event_data);
				win->counter_ugni_nbi--;

			}
		} else if (status != GNI_RC_NOT_DONE) {
			int error_code = 1;

			/*
			 * An error occurred getting the event.
			 */

			char *cqErrorStr;
			char *cqOverrunErrorStr = "";
			gni_return_t tmp_status = GNI_RC_SUCCESS;
#ifdef CRAY_CONFIG_GHAL_ARIES
			uint32_t status_code;

			status_code = GNI_CQ_GET_STATUS(event_data);
			if (status_code == A_STATUS_AT_PROTECTION_ERR) {
				return 1;
			}
#endif

			/*
			 * Did the event queue overrun condition occurred?
			 * This means that all of the event queue entries were used up
			 * and another event occurred, i.e. there was no entry available
			 * to put the new event into.
			 */

			if (GNI_CQ_OVERRUN(event_data)) {
				cqOverrunErrorStr = "CQ_OVERRUN detected ";
				error_code = 2;

				_foMPI_ERR_LOG("[%s] Rank: %4i ERROR CQ_OVERRUN detected\n",
						glob_info.curr_job.uts_info.nodename, glob_info.curr_job.rank_id);

			}

			cqErrorStr = (char *) _foMPI_ALLOC(256);
			if (cqErrorStr != NULL) {

				/*
				 * Print a user understandable error message.
				 */

				tmp_status = GNI_CqErrorStr(event_data, cqErrorStr, 256);
				if (tmp_status == GNI_RC_SUCCESS) {
					_foMPI_ERR_LOG(
							"[%s] Rank: %4i GNI_CqGetEvent    ERROR %sstatus: %s (%d) inst_id: %lu event: 0x%16.16lx GNI_CqErrorStr: %s\n",
							glob_info.curr_job.uts_info.nodename, glob_info.curr_job.rank_id,
							cqOverrunErrorStr, gni_err_str[status], status,
							GNI_CQ_GET_INST_ID(event_data), event_data, cqErrorStr);
				} else {

					/*
					 * Print the error number.
					 */

					_foMPI_ERR_LOG(
							"[%s] Rank: %4i GNI_CqGetEvent    ERROR %sstatus: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
							glob_info.curr_job.uts_info.nodename, glob_info.curr_job.rank_id,
							cqOverrunErrorStr, gni_err_str[status], status,
							GNI_CQ_GET_INST_ID(event_data), event_data);
				}

				_foMPI_FREE(cqErrorStr);

			} else {

				/*
				 * Print the error number.
				 */

				_foMPI_ERR_LOG(
						"[%s] Rank: %4i GNI_CqGetEvent    ERROR %sstatus: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
						glob_info.curr_job.uts_info.nodename, glob_info.curr_job.rank_id,
						cqOverrunErrorStr, gni_err_str[status], status,
						GNI_CQ_GET_INST_ID(event_data), event_data);
			}
			return error_code;
		} else {

			/*
			 * An event has not been received yet.
			 */
			// busy Wait
		}
	}

	return MPI_SUCCESS;
}
#endif
#endif

