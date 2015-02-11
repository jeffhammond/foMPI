// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>

#include "fompi.h"

int foMPI_Win_create_dynamic(MPI_Info info, MPI_Comm comm, foMPI_Win *win) {

	int i;
	int* temp;
	MPI_Group group_comm_world, group;
	foMPI_Win_dynamic_struct_t win_info;

	/* allocate the window */
	void * memptr;
	_foMPI_ALIGNED_ALLOC(&memptr, sizeof(foMPI_Win_desc_t))
	*win = memptr;
	assert(*win != NULL);

	/**transition info. As soon as an foMPI Communicator is implemented update this UGNI use this*/
	(*win)->fompi_comm = glob_info.comm_world;
	/* the window communicator specific informations */
	(*win)->comm = comm;
	MPI_Comm_size(comm, &((*win)->commsize));
	MPI_Comm_rank(comm, &((*win)->commrank));

	/* get all ranks from the members of the group */
	(*win)->group_ranks = _foMPI_ALLOC((*win)->commsize * sizeof(int32_t));
	assert((*win)->group_ranks != NULL);

	temp = _foMPI_ALLOC((*win)->commsize * sizeof(int));
	assert(temp != NULL);
	for (i = 0; i < (*win)->commsize; i++) {
		temp[i] = i;
	}
	MPI_Comm_group(comm, &group);
	MPI_Comm_group(MPI_COMM_WORLD, &group_comm_world);
	MPI_Group_translate_ranks(group, (*win)->commsize, &temp[0], group_comm_world,
			&((*win)->group_ranks[0]));

	_foMPI_FREE(temp);
	MPI_Group_free(&group);
	MPI_Group_free(&group_comm_world);
#ifdef UGNI
	gni_return_t status_gni;
#ifdef _foMPI_UGNI_WIN_RELATED_SRC_CQ
	(*win)->number_of_source_cq_entries = _foMPI_NUM_SRC_CQ_ENTRIES;
	status_gni = GNI_CqCreate((*win)->fompi_comm->nic_handle, (*win)->number_of_source_cq_entries,
			0,_foMPI_SRC_CQ_MODE, NULL, NULL, &((*win)->source_cq_handle));

	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
	_foMPI_TRACE_LOG(3, "GNI_CqCreate      source with %i entries\n", (*win)->number_of_source_cq_entries);
#endif
	(*win)->counter_ugni_nbi = 0;

	(*win)->number_of_dest_cq_entries = _foMPI_NUM_DST_CQ_ENTRIES;
	//we try to use the handler instead of only the dispatcher trying to decrease the latency of the notification
#ifdef NOTIFICATION_SOFTWARE_AGENT
	//	status_gni = GNI_CqCreate(glob_info.comm_world->nic_handle, (*win)->number_of_dest_cq_entries, 0,
	//			foMPI_DST_CQ_MODE, &foMPI_NotificationHandler, (*win), &((*win)->destination_cq_handle));
#else
	//TODO: substitute comme world with foMPI_Comm
	status_gni = GNI_CqCreate(glob_info.comm_world->nic_handle, (*win)->number_of_dest_cq_entries,
			0,
			_foMPI_DST_CQ_MODE, NULL, NULL, &((*win)->destination_cq_handle));
#endif
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
	_foMPI_TRACE_LOG(3, "GNI_CqCreate      destination with %i entries\n",
			(*win)->number_of_dest_cq_entries);

	/*init backup_queue*/
	(*win)->destination_cq_discarded = _fompi_notif_uq_init();
	_foMPI_TRACE_LOG(3, "fompi_oset    Created \n");
#endif

	/* allocate the memory for the remote window information */
	_foMPI_ALIGNED_ALLOC(&memptr, (*win)->commsize * sizeof(foMPI_Win_dynamic_struct_t))
	(*win)->dynamic_list = memptr;
	assert((*win)->dynamic_list != NULL);

	/* set the information for the remote processes */
	win_info.remote_mem_regions = NULL;

	win_info.dynamic_id = 0;

	win_info.win_ptr = *win;
	_foMPI_mem_register(*win, (uint64_t) sizeof(foMPI_Win_desc_t), &(win_info.win_ptr_seg), *win);

	/* PCSW matching */
	_foMPI_ALIGNED_ALLOC(&memptr, (*win)->commsize * sizeof(uint64_t))
	win_info.pscw_matching_exposure = memptr;
	assert(win_info.pscw_matching_exposure != NULL);

	_foMPI_ALIGNED_ALLOC(&memptr, (*win)->commsize * sizeof(uint32_t))
	(*win)->pscw_matching_access = memptr;
	assert((*win)->pscw_matching_access != NULL);

	for (i = 0; i < (*win)->commsize; i++) {
		win_info.pscw_matching_exposure[i] = 0;
		(*win)->pscw_matching_access[i] = 0;
	}
	_foMPI_mem_register(win_info.pscw_matching_exposure, (*win)->commsize * sizeof(uint64_t),
			&(win_info.pscw_matching_exposure_seg), *win);

	/* lock synchronisation */
	(*win)->mutex = foMPI_MUTEX_NONE;
	(*win)->lock_mutex = 0; /* no current access */
	if ((*win)->commrank == MASTER) {
		(*win)->lock_all_mutex = 0; /* no current access */
	}
	(*win)->local_exclusive_count = 0;
	(*win)->excl_locks = NULL;

	(*win)->dynamic_mutex = foMPI_MUTEX_NONE;

	(*win)->dynamic_id = 0;
	(*win)->win_dynamic_mem_regions = NULL;

	(*win)->dynamic_last = NULL;

	/* management of rma operations */
	(*win)->nbi_counter = 0;

	(*win)->create_flavor = foMPI_WIN_FLAVOR_DYNAMIC;

	(*win)->name = NULL;

	MPI_Allgather(&win_info, sizeof(foMPI_Win_dynamic_struct_t), MPI_BYTE,
			&((*win)->dynamic_list[0]), sizeof(foMPI_Win_dynamic_struct_t), MPI_BYTE, comm);

	return MPI_SUCCESS;
}

/* TODO: check for overlapping regions */
int foMPI_Win_attach(foMPI_Win win, void *base, MPI_Aint size) {

	int64_t mutex;
	dmapp_return_t status;

	assert(base != NULL);
	assert(size >= 0);

	/* set mutex */
	do {
		/* TODO: busy waiting, use of dmapp functions for local access */
		status = dmapp_acswap_qw(&mutex,
				(void*) &(win->dynamic_list[win->commrank].win_ptr->dynamic_mutex),
				&(win->dynamic_list[win->commrank].win_ptr_seg.dmapp),
				win->group_ranks[win->commrank], (int64_t) foMPI_MUTEX_NONE,
				(int64_t) win->commrank);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	} while (mutex != foMPI_MUTEX_NONE);

	if (win->dynamic_last == NULL) {
		/*if no memory segments are registered head + tail (win dyn mem reg + dynamic last )*/
		void * memptr;
		_foMPI_ALIGNED_ALLOC(&memptr, sizeof(foMPI_Win_dynamic_element_t))
		win->dynamic_last = memptr;
		assert(win->dynamic_last != NULL);
		win->win_dynamic_mem_regions = win->dynamic_last;

		_foMPI_mem_register(win->win_dynamic_mem_regions, sizeof(foMPI_Win_dynamic_element_t),
				&(win->win_dynamic_mem_regions_seg), win);

	} else {
		/*add segment to tail only*/
		void * memptr;
		_foMPI_ALIGNED_ALLOC(&memptr, sizeof(foMPI_Win_dynamic_element_t))
		win->dynamic_last->next = memptr;
		assert(win->dynamic_last->next != NULL);

		_foMPI_mem_register(win->dynamic_last->next, sizeof(foMPI_Win_dynamic_element_t),
				&(win->dynamic_last->next_seg), win);

		win->dynamic_last = win->dynamic_last->next;
	}

	MPI_Get_address(base, &(win->dynamic_last->base));
	win->dynamic_last->size = size;

	_foMPI_mem_register(base, size, &(win->dynamic_last->seg), win);

	win->dynamic_last->next = NULL;

	win->dynamic_id++;

	/* release the mutex */
	status = dmapp_acswap_qw(&mutex,
			(void*) &(win->dynamic_list[win->commrank].win_ptr->dynamic_mutex),
			&(win->dynamic_list[win->commrank].win_ptr_seg.dmapp), win->group_ranks[win->commrank],
			(int64_t) (int64_t) win->commrank, (int64_t) foMPI_MUTEX_NONE);
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

	return MPI_SUCCESS;
}

int foMPI_Win_detach(foMPI_Win win, const void *base) {

	int64_t mutex;

	dmapp_return_t status;
	foMPI_Win_dynamic_element_t* current;
	foMPI_Win_dynamic_element_t* previous;
	int found;
	MPI_Aint search;

	assert(base != NULL);

	/* set mutex */
	do {
		/* TODO: busy waiting, use of dmapp functions for local access */
		status = dmapp_acswap_qw(&mutex,
				(void*) &(win->dynamic_list[win->commrank].win_ptr->dynamic_mutex),
				&(win->dynamic_list[win->commrank].win_ptr_seg.dmapp),
				win->group_ranks[win->commrank], (int64_t) foMPI_MUTEX_NONE,
				(int64_t) win->commrank);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	} while (mutex != foMPI_MUTEX_NONE);

	current = win->win_dynamic_mem_regions;
	previous = win->win_dynamic_mem_regions;

	MPI_Get_address((void*) base, &search);

	found = 0;
	if (current != NULL) {
		if (current->base == search) {
			found = 1;
		}
		while ((current->next != NULL) && (found == 0)) {
			previous = current;
			current = current->next;
			if (current->base == search) {
				found = 1;
			}
		}
	} else {
		return foMPI_ERROR_RMA_WIN_MEM_NOT_FOUND;
	}

	if (found == 1) {
		if (current == win->win_dynamic_mem_regions) { /* the first one has to be detached */

			_foMPI_mem_unregister(&(current->seg), win);

			if (current == win->dynamic_last) { /* there is only one element in the list */
				win->dynamic_last = NULL;

				_foMPI_mem_unregister(&(win->win_dynamic_mem_regions_seg), win);

				win->win_dynamic_mem_regions = NULL;

			} else {

				_foMPI_mem_unregister(&(win->win_dynamic_mem_regions_seg), win);

				win->win_dynamic_mem_regions = current->next;
				win->win_dynamic_mem_regions_seg = current->next_seg; /* TODO: just use pointer? */

			}

			_foMPI_FREE(current);

		} else { /* there is more than one element and the first one isn't the one to detach */

			_foMPI_mem_unregister(&(previous->next_seg), win);

			if (current == win->dynamic_last) { /* detach the last element */
				win->dynamic_last = previous;
				previous->next = NULL;
			} else {
				previous->next = current->next;
				previous->next_seg = current->next_seg;
			}

			_foMPI_mem_unregister(&(current->seg), win);

			_foMPI_FREE(current);
		}
	} else {
		return foMPI_ERROR_RMA_WIN_MEM_NOT_FOUND;
	}

	win->dynamic_id++;

	/* release the mutex */
	status = dmapp_acswap_qw(&mutex,
			(void*) &(win->dynamic_list[win->commrank].win_ptr->dynamic_mutex),
			&(win->dynamic_list[win->commrank].win_ptr_seg.dmapp), win->group_ranks[win->commrank],
			(int64_t) (int64_t) win->commrank, (int64_t) foMPI_MUTEX_NONE);
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

	return MPI_SUCCESS;
}
