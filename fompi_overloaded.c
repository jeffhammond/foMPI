// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "fompi.h"

/*Global variable*/
foMPI_global_t glob_info;

/*"local" variables*/
int need_to_mpi_finalize;
int need_to_dmapp_finalize;

/*foMPI helper functions*/
static int dcache_init();
static int dcache_clean_up();
#ifdef UGNI
/*PMI_UGNI utility functions*/
static uint32_t get_cookie(void);
static uint8_t get_ptag(void);
static void _PMI_init_internal();
#endif
/* the overloaded MPI functions */

int foMPI_Type_free(MPI_Datatype *datatype) {
	int i;

	i = 0;
	do {
		if (glob_info.dcache_blocks[i].type == *datatype) {
			_foMPI_FREE(glob_info.dcache_blocks[i].displ);
			_foMPI_FREE(glob_info.dcache_blocks[i].blocklen);
			glob_info.dcache_blocks[i].type = MPI_DATATYPE_NULL;
		}
	} while (++i < 10);

	return MPI_Type_free(datatype);
}

int foMPI_Init(int *argc, char ***argv) {

	dmapp_return_t status;
	dmapp_rma_attrs_ext_t actual_args = { 0 }, rma_args = { 0 };
	dmapp_jobinfo_t job;
	int myrank;
	int flag;

	rma_args.put_relaxed_ordering = DMAPP_ROUTING_ADAPTIVE;
	rma_args.get_relaxed_ordering = DMAPP_ROUTING_ADAPTIVE;
	rma_args.max_outstanding_nb = DMAPP_DEF_OUTSTANDING_NB;
	rma_args.offload_threshold = DMAPP_OFFLOAD_THRESHOLD;
	rma_args.max_concurrency = 1;
	rma_args.PI_ordering = DMAPP_PI_ORDERING_RELAXED;

    /* Jeff: These appear to have been added later... */
    rma_args.queue_depth = DMAPP_QUEUE_DEFAULT_DEPTH;
    rma_args.queue_nelems = DMAPP_QUEUE_DEFAULT_NELEMS;

	MPI_Initialized(&flag);
	putenv("_DMAPP_DLA_PURE_DMAPP_JOB=0");
	if (flag == 0) {
		MPI_Init(argc, argv);
		need_to_mpi_finalize = 1;
	} else {
		need_to_mpi_finalize = 0;
	}

	status = dmapp_initialized(&flag);
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

	if (flag == 0) {
		if (!need_to_mpi_finalize) {
			putenv("_DMAPP_DLA_PURE_DMAPP_JOB=0");
		}
		status = dmapp_init_ext(&rma_args, &actual_args);
		assert(status == DMAPP_RC_SUCCESS);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		need_to_dmapp_finalize = 1;
	} else {
		status = dmapp_set_rma_attrs_ext(&rma_args, &actual_args);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		need_to_dmapp_finalize = 0;
	}

	/* TODO: only debug purpose, debug_pe is not needed as a global variable */
	status = dmapp_get_jobinfo(&job);
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

	glob_info.debug_pe = job.pe;
	glob_info.v_option = _foMPI_V_OPTION;

	/* end only debug purpose */

	/* TODO: translate table if this assumption is not correct */
	MPI_Comm_rank( MPI_COMM_WORLD, &myrank);
	if (myrank != glob_info.debug_pe) {
		fprintf(stderr,
				"Error on rank %i in %s line %i: pe rank doesn't match rank %i in MPI_COMM_WORLD.\n",
				glob_info.debug_pe, __FILE__, __LINE__, myrank);
		exit(EXIT_FAILURE);
	}

	/* init datatype block cache */
	dcache_init();

#ifdef XPMEM
	/* create a MPI communicator to identify onnode processes */
	int num;
	TPD_Get_num_hier_levels(&num, MPI_COMM_WORLD);
	assert( num == 1 );

	MPI_Comm onnode_comm;
	TPD_Get_hier_levels(&onnode_comm, num, MPI_COMM_WORLD);

	MPI_Comm_group( onnode_comm, &(glob_info.onnode_group) );
#endif

	/* Fortran data init */
	glob_info.Fortran_Windows = NULL;
	glob_info.Fortran_Requests = NULL;
	glob_info.Fortran_win_sz = 100;
	glob_info.Fortran_req_sz = 100;
	glob_info.Fortran_free_win = NULL;
	glob_info.Fortran_free_req = NULL;

#ifdef UGNI
	/*uGNI settings*/
	_PMI_init_internal();
	if (glob_info.curr_job.rank_id != glob_info.debug_pe) {
		fprintf(stderr,
				"Error on rank %i in %s line %i: pe rank doesn't match rank %i in MPI_COMM_WORLD.\n",
				glob_info.debug_pe, __FILE__, __LINE__, myrank);
		exit(EXIT_FAILURE);
	}

	gni_return_t status_gni = GNI_RC_SUCCESS;
	glob_info.default_ugni_dev_id = _foMPI_DEFAULT_DEV_ID;
	glob_info.default_ugni_rdma_modes = _foMPI_DEFAULT_RDMA_MODES;
	glob_info.num_ugni_comm_instances = 0;
	status_gni = _foMPI_Comm_create(MPI_COMM_WORLD, &(glob_info.comm_world));
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
#endif

#ifdef PAPI
	timing_init();
#endif

#ifdef LIBPGT
	PGT_Init("fompi-ex", 0);
#endif

	return MPI_SUCCESS;
}

int foMPI_Alloc_mem(MPI_Aint size, MPI_Info info, void *baseptr) {
	void * memptr;
	_foMPI_ALIGNED_ALLOC(&memptr, size)
	*((void **) baseptr) = memptr;
	return MPI_SUCCESS;
}

int foMPI_Finalize() {

	dmapp_return_t status;

	/* TODO: Could this be removed? */
	MPI_Barrier(MPI_COMM_WORLD);

#ifdef PAPI
	timing_close();
#endif

#ifdef LIBPGT
	PGT_Finalize();
#endif
	if (need_to_dmapp_finalize == 1) {
		status = dmapp_finalize();
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		need_to_dmapp_finalize = 0;
	}

	/* clean up the datatype block cache */
	dcache_clean_up();

	/* clean up Fortran data */
	_foMPI_FREE(glob_info.Fortran_Windows);
	_foMPI_FREE(glob_info.Fortran_Requests);
	glob_info.Fortran_win_sz = 0;
	glob_info.Fortran_req_sz = 0;
	int index;
	do {
		index = get_free_elem(&glob_info.Fortran_free_win);
	} while (index != -1);
	do {
		index = get_free_elem(&glob_info.Fortran_free_req);
	} while (index != -1);

	if (need_to_mpi_finalize == 1) {
		MPI_Finalize();
		need_to_mpi_finalize = 0;
	}

#ifdef UGNI
	_foMPI_Comm_free(&(glob_info.comm_world));
	PMI_Finalize();
#endif

	return MPI_SUCCESS;
}

/* some helper functions */
static int dcache_init() {
	int i;

	glob_info.dcache_last_entry = 0;
	for (i = 0; i < 10; i++) {
		glob_info.dcache_blocks[i].type = MPI_DATATYPE_NULL;
	}
	glob_info.dcache_origin_last_entry = 0;
	glob_info.dcache_target_last_entry = 0;

	return MPI_SUCCESS;
}

static int dcache_clean_up() {
	int i;

	for (i = 0; i < 10; i++) {
		if (glob_info.dcache_blocks[i].type != MPI_DATATYPE_NULL) {
			_foMPI_FREE(glob_info.dcache_blocks[i].displ);
			_foMPI_FREE(glob_info.dcache_blocks[i].blocklen);
		}
	}

	return MPI_SUCCESS;
}

#ifdef UGNI
/* PMI init */
static void _PMI_init_internal() {
	int rc;
	PMI_BOOL initialized;
	glob_info.rand_seed = 0;

	if ((rc = uname(&(glob_info.curr_job.uts_info))) != 0) {
		fprintf(stderr, "uname(2) failed, errno=%d\n", errno);
		exit(1);
	}

	rc = PMI_Initialized(&initialized);
	assert(rc == PMI_SUCCESS);

	if (initialized == PMI_FALSE) {
		rc = PMI_Init(&(glob_info.curr_job.first_spawned));
		assert(rc == PMI_SUCCESS);
	}

	rc = PMI_Get_size(&(glob_info.curr_job.number_of_ranks));
	assert(rc == PMI_SUCCESS);

	rc = PMI_Get_rank(&(glob_info.curr_job.rank_id));
	assert(rc == PMI_SUCCESS);

	/*
	 * Get job attributes from PMI.
	 */

	glob_info.curr_job.ptag = get_ptag();
	glob_info.curr_job.cookie = get_cookie();
	_foMPI_TRACE_LOG(3, "PMI initialized #ranks: %i ptag: %u cookie: 0x%x\n",
			glob_info.curr_job.number_of_ranks, glob_info.curr_job.ptag, glob_info.curr_job.cookie);

}

/*The following function is copyrighted by CRAY  */
/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */
/*
 * get_cookie will get the cookie value associated with this process.
 *
 * By default, it will get the second cookie to match the second ptag,
 * if more than one is available.
 * Environment variable PTAG_INDEX=n will override it to use the 'n' index.
 *
 * Returns: the cookie value.
 */

static uint32_t get_cookie(void) {
	uint32_t cookie;
	char *p_ptr;
	char *copy, *p_copy;
	char *token;
	int index = 0, ptag_index = _foMPI_PTAG_INDEX;

	/* Which ptag/cookie index should we pick? Default is index 1 (second cookie).
	 If DMAPP is being used, index 1 *must* be used on Aries.
	 */
	p_ptr = getenv("PTAG_INDEX");
	if (p_ptr != NULL)
		ptag_index = atoi(p_ptr);

	p_ptr = getenv("PMI_GNI_COOKIE");
	assert(p_ptr != NULL);

	/* Copy the environment variable string because strtok is destructive.*/
	p_copy = copy = strdup(p_ptr);

	/* Find the desired cookie, or the last one available */
	while ((token = strtok(p_copy, ":")) != NULL) {
		p_copy = NULL; /*for subsequent calls to strtok*/
		cookie = (uint32_t) atoi(token);
		if (index++ == ptag_index)
			break;
	}

	_foMPI_FREE(copy);
	return cookie;
}

/*
 * get_ptag will get the ptag value associated with this process                                                                                     .
 *                                                                                                                                                   .
 * By default, it will get the second ptag if more than one is available.
 * Environment variable PTAG_INDEX=n will override it to get the 'n' index ptag.                                                                     .
 *                                                                                                                                                   .
 * Returns: the ptag value.
 */

static uint8_t get_ptag(void) {
	char *p_ptr;
	char *copy, *p_copy;
	uint8_t ptag;
	char *token;
	int index = 0, ptag_index = _foMPI_PTAG_INDEX;

	/* Which ptag index should we pick? Default is index 1 (second ptag).
	 If DMAPP is being used, index 1 *must* be used on Aries.
	 */
	p_ptr = getenv("PTAG_INDEX");
	if (p_ptr != NULL)
		ptag_index = atoi(p_ptr);

	p_ptr = getenv("PMI_GNI_PTAG");
	assert(p_ptr != NULL);

	/* Copy the environment variable string because strtok is destructive.*/
	p_copy = copy = strdup(p_ptr);

	/* Find the desired ptag, or the last one available */
	while ((token = strtok(p_copy, ":")) != NULL) {
		p_copy = NULL; /*for subsequent calls to strtok*/
		ptag = (uint8_t) atoi(token);
		if (index++ == ptag_index)
			break;
	}

	_foMPI_FREE(copy);
	return ptag;
}
#endif

