/*
 * fompi_comm.c
 *
 *  Created on: Jul 1, 2014
 *      Author: Roberto Belli
 *
 * foMPI Communicator
 * Actually this data structure is utilized only for instantiating a comm_world communicator
 * The idea is that keeping this data structure separated give us in future more flexibility for trying multiple
 * solution (we can reimplement all the calls in the COmmunicators sections for reducing over synchronization for example)
 * adding also process group for dmapp for using collectives, reducing oversynchronization on dmapp using calls with SYNC_ID
 * (maybe is possible to associate an id for synchronizing only on RMA events on a specified Window)
 *
 * In the precedent version of foMPI there are a lot of variables kept in the Win datastructure that are communicator related.
 * Different windows object created using the same communicator keeps copies of the same data structures
 * */

#include "fompi.h"

#ifdef UGNI
static int _calcBindID(int cdm_id, int target_rank);
static unsigned int get_gni_nic_address(int device_id);
static inline int ugni_Dequeue_local_event(_foMPI_Comm comm);
static inline void _foMPI_Comm_free_ugni_data_descriptor(gni_post_descriptor_t * data_descr, _foMPI_Comm comm);
#endif
static int _foMPI_Comm_free_internal(char const *str, _foMPI_Comm *communicator);

/*int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm) */
/*creates a foMPI communicator with same group*/
int _foMPI_Comm_create(MPI_Comm comm, _foMPI_Comm *newcomm) {

	/*allocation of comm ds*/
	*newcomm = (foMPI_Comm_desc_t *) _foMPI_ALLOC(sizeof(foMPI_Comm_desc_t));
	assert(*newcomm != NULL);

	glob_info.num_ugni_comm_instances++;
	/*as communication domain id we use the rank of the process in the job + an offset (number of instance of ugni communicators)*/

	/* rank calculation  */

	int* temp;
	int i = 0;
	MPI_Group group_comm_world, group;

	/* the communicator specific informations */
	(*newcomm)->mpi_comm = comm;
	MPI_Comm_size(comm, &((*newcomm)->commsize));
	MPI_Comm_rank(comm, &((*newcomm)->commrank));

	/* get all ranks from the members of the group */
	(*newcomm)->group_ranks = _foMPI_ALLOC((*newcomm)->commsize * sizeof(int32_t));
	assert((*newcomm)->group_ranks != NULL);

	temp = _foMPI_ALLOC((*newcomm)->commsize * sizeof(int));
	assert(temp != NULL);
	for (i = 0; i < (*newcomm)->commsize; i++) {
		temp[i] = i;
	}
	MPI_Comm_group(comm, &group);
	MPI_Comm_group(MPI_COMM_WORLD, &group_comm_world);
	MPI_Group_translate_ranks(group, (*newcomm)->commsize, &temp[0], group_comm_world,
			&((*newcomm)->group_ranks[0]));

	_foMPI_FREE(temp);
	MPI_Group_free(&group_comm_world);

#ifdef UGNI
	int cdm_id = glob_info.num_ugni_comm_instances + glob_info.comm_world->commrank;
	/*
	 * now is possible to use only one device but in future would be possible
	 * to use at the same time different devices at the same time
	 * IF not is possible to move local_nic_address and keep all
	 * the remote_nic_addresses in the global data structure avoiding  to invoke the
	 * MPI_ALLGATHER at every communicator creation
	 */
	int device_id = glob_info.default_ugni_dev_id;
	gni_return_t status_gni = GNI_RC_SUCCESS;
	unsigned int *all_nic_addresses;
	unsigned int remote_address;
	uint32_t bind_id;

	/*init uGNI-related variables*/
	(*newcomm)->endpoint_handles_array = NULL;
	(*newcomm)->number_of_cq_entries = _foMPI_NUM_SRC_CQ_ENTRIES;
	//TODO: for not wasting space, is possible to create a cache that allocate descriptors only if needed
	_foMPI_ALIGNED_ALLOC( ((void**) &((*newcomm)->data_desc_array) ), _foMPI_NUM_SRC_CQ_ENTRIES * sizeof(gni_post_descriptor_t))
	assert((*newcomm)->data_desc_array != NULL);
	_foMPI_ALIGNED_ALLOC( ((void**) &((*newcomm)->data_desc_ptr) ), _foMPI_NUM_SRC_CQ_ENTRIES * sizeof(gni_post_descriptor_t*))
	assert((*newcomm)->data_desc_ptr != NULL);
	int pi;
	void * memptr;
	for(pi=0;pi<_foMPI_NUM_SRC_CQ_ENTRIES;pi++){
		(*newcomm)->data_desc_ptr[pi]=&((*newcomm)->data_desc_array[pi]);
	}

	(*newcomm)->local_nic_address = 0;
	(*newcomm)->counter_ugni_nbi = 0;
	(*newcomm)->ins_ind = 0;
	(*newcomm)->extr_ind = 0;
	(*newcomm)->device_id = device_id;

	/*
	 * Create a handle to the communication domain.
	 *    cdm_id is the rank of this instance of the job.
	 *    ptag is the protection tab for the job.
	 *    cookie is a unique identifier created by the system.
	 *    modes is a bit mask used to enable various flags.
	 *            GNI_CDM_MODE_BTE_SINGLE_CHANNEL states to do RDMA posts
	 *            using only one BTE channel.
	 *    cdm_handle is the handle that is returned pointing to the
	 *        communication domain.
	 */

	status_gni = GNI_CdmCreate(cdm_id, glob_info.curr_job.ptag, glob_info.curr_job.cookie,
			glob_info.default_ugni_rdma_modes, &((*newcomm)->cdm_handle));
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
	_foMPI_TRACE_LOG(3, "GNI_CdmCreate     inst_id: %i ptag: %u cookie: 0x%x\n", cdm_id,
			glob_info.curr_job.ptag, glob_info.curr_job.cookie);

	/*
	 * Attach the communication domain handle to the NIC.
	 *    cdm_handle is the handle pointing to the communication domain.
	 *    device_id is the device identifier of the NIC that be attached to.
	 *    local_address is the PE address that is returned for the
	 *        communication domain that this NIC is attached to.
	 *    nic_handle is the handle that is returned pointing to the NIC.
	 */

	status_gni = GNI_CdmAttach((*newcomm)->cdm_handle, device_id, &((*newcomm)->local_nic_address),
			&((*newcomm)->nic_handle));
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
	/*
	 validate_gni_cleanup_return(status_gni, "GNI_CdmCreate",
	 _foMPI_Comm_free_internal("EXIT_DOMAIN", newcomm));
	 */
	_foMPI_TRACE_LOG(3, "GNI_CdmAttach  to NIC\n");

	//TODO: Moving the cqcreation and the Epcreation and binding to the window creation:
	/*
	 * CONS: multiple End Points for the same pair of PEs (wasting memory resources)
	 *
	 * PRO: local completion queue - window related (no over synchronization)
	 * */

	/*
	 * Create the completion queue.
	 *     nic_handle is the NIC handle that this completion queue will be
	 *          associated with.
	 *     number_of_cq_entries is the size of the completion queue.
	 *     zero is the delay count is the number of allowed events before an
	 *          interrupt is generated.
	 *     GNI_CQ_NOBLOCK states that the operation mode is non-blocking.
	 *     NULL states that no user supplied callback function is defined.
	 *     NULL states that no user supplied pointer is passed to the callback
	 *          function.
	 *     cq_handle is the handle that is returned pointing to this newly
	 *          created completion queue.
	 */

	/*
	 * GNI_CDM_MODE_DUAL_EVENTS
	 * Must be used when local and global completion events are needed for RDMA post operations.
	 * By default, users may post transactions with either local or global completion notification, not both.
	 *  If receipt of both local and global events is requested users must set this flag.
	 *  Performing a post operation with local and global events enabled without this
	 *  flag set will yield an error GNI_RC_INVALID_PARAM.
	 *  In addition, during an EpBind in default mode, transfer requests are allocated equal in size to the number
	 *   of events in the associated source CQ. When this flag is set transfer requests are allocated 1 per 2 CQ event slots.
	 *    Therefore, a user is limited to posting half as many transactions as CQ events when this flag is set.
	 *     Exceeding this limit will yield an error GNI_RC_ERROR_RESOURCE.
	 */

	status_gni = GNI_CqCreate((*newcomm)->nic_handle, (glob_info.default_ugni_rdma_modes == GNI_CDM_MODE_DUAL_EVENTS ) ?  (*newcomm)->number_of_cq_entries*2 : (*newcomm)->number_of_cq_entries , 0,
	_foMPI_SRC_CQ_MODE, NULL, NULL, &((*newcomm)->cq_handle));
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
	/*
	 * validate_gni_cleanup_return(status_gni, "GNI_CqCreate",
			_foMPI_Comm_free_internal("EXIT_DOMAIN", newcomm));
	*/
	_foMPI_TRACE_LOG(3, "GNI_CqCreate      source with %i entries\n",
			(*newcomm)->number_of_cq_entries);

	/*
	 * Allocate the endpoint handles array.
	 */

	(*newcomm)->endpoint_handles_array = (gni_ep_handle_t *) _foMPI_ALLOC(
			(*newcomm)->commsize * sizeof(gni_ep_handle_t));
	assert((*newcomm)->endpoint_handles_array != NULL);

	/*
	 * Get all of the NIC address for all of the ranks.
	 */
	(*newcomm)->local_nic_address = get_gni_nic_address(device_id);

	all_nic_addresses = (unsigned int *) _foMPI_ALLOC((*newcomm)->commsize * sizeof(unsigned int));
	MPI_Allgather((void *) &(*newcomm)->local_nic_address, 1, MPI_UNSIGNED, all_nic_addresses, 1,
	MPI_UNSIGNED, comm);

	for (i = 0; i < (*newcomm)->commsize; i++) {
		if (i == (*newcomm)->commrank) {
			continue;
		}

		/*
		 * You must do an EpCreate for each endpoint pair.
		 * That is for each remote node that you will want to communicate with.
		 * The EpBind request updates some fields in the endpoint_handle so
		 * this is the reason that all pairs of endpoints need to be created.
		 *
		 * Create the logical endpoint for each rank.
		 *     nic_handle is our NIC handle.
		 *     cq_handle is our completion queue handle.
		 *     endpoint_handles_array will contain the handle that is returned
		 *         for this endpoint instance.
		 */

		status_gni = GNI_EpCreate((*newcomm)->nic_handle, (*newcomm)->cq_handle,
				&((*newcomm)->endpoint_handles_array[i]));
		_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
		/*
		validate_gni_cleanup_return(status_gni, "GNI_EpCreate",
				_foMPI_Comm_free_internal("EXIT_ENDPOINT", newcomm));
		*/
		_foMPI_TRACE_LOG(3, "GNI_EpCreate      remote rank: %4i NIC: %p, CQ: %p, EP: %p\n", i,
				(*newcomm)->nic_handle, (*newcomm)->cq_handle,
				(*newcomm)->endpoint_handles_array[i]);

		/*
		 * Get the remote address to bind to.
		 */

		remote_address = all_nic_addresses[i];
		/*cdm_id is used in order to have a unique bind_id*/
		bind_id = _calcBindID(cdm_id, i);

		/*
		 * Bind the remote address to the endpoint handler.
		 *     endpoint_handles_array is the endpoint handle that is being bound
		 *     remote_address is the address that is being bound to this
		 *         endpoint handler
		 *     bind_id is an unique user specified identifier for this bind.
		 *
		 */

		status_gni = GNI_EpBind((*newcomm)->endpoint_handles_array[i], remote_address, bind_id);
		_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
		/*
		validate_gni_cleanup_return(status_gni, "GNI_EpBind",
				_foMPI_Comm_free_internal("EXIT_ENDPOINT", newcomm));
		*/
		_foMPI_TRACE_LOG(3,
				"GNI_EpBind        remote rank: %4i EP:  %p remote_address: %u, remote_id: %u\n", i,
				(*newcomm)->endpoint_handles_array[i], remote_address, bind_id);

	}
	_foMPI_FREE(all_nic_addresses);
#endif
	_foMPI_TRACE_LOG(3, "GNI_Ep Binded\n");
	//TODO: is that necessary?
	MPI_Barrier(comm);
	_foMPI_TRACE_LOG(1, "_foMPI_Comm_Create Completed \n");
	return MPI_SUCCESS;
}

/*
 * int MPI_Comm_free(MPI_Comm *comm)
 * This collective operation marks the communication object for deallocation.
 * The handle is set to MPI_COMM_NULL. Any pending operations that use this communicator will complete normally;
 * the object is actually deallocated only if there are no other active references to it.
 * This call applies to intra- and inter-communicators.
 * The delete callback functions for all cached attributes (see Section 6.7) are called in arbitrary order.
 */
int _foMPI_Comm_free(_foMPI_Comm *communicator) {
	if (*communicator == foMPI_COMM_NULL)
		return MPI_ERR_COMM;
	return _foMPI_Comm_free_internal("", communicator);
}

/*
 * Internal Functions
 */
inline int _foMPI_Comm_flush_all_internal(_foMPI_Comm communicator) {
#ifdef UGNI
	/*
	 * Check the completion queue to verify that the message request has
	 * been sent.  The source completion queue needs to be checked and
	 * events to be removed so that it does not become full and cause
	 * succeeding calls to PostCqWrite to fail.
	 */

	/*
	 * Get all of the data completion queue events.
	 */
	while (communicator->counter_ugni_nbi > 0) {

		ugni_Dequeue_local_event(communicator);

		_foMPI_TRACE_LOG(5, "checking %"PRIu64" CQ events\n", communicator->counter_ugni_nbi);

	}
#endif
	_foMPI_TRACE_LOG(1, "_foMPI_Comm_all_flush_internal Completed\n");
	return MPI_SUCCESS;
}

static int _foMPI_Comm_free_internal(char const *str, _foMPI_Comm *communicator) {

	gni_return_t status_gni = GNI_RC_SUCCESS;


#ifdef UGNI
	int i = 0;
	if (strcmp(str, "EXIT_MEMORY") == 0)
		goto EXIT_MEMORY;
	if (strcmp(str, "EXIT_ENDPOINT") == 0)
		goto EXIT_ENDPOINT;
	if (strcmp(str, "EXIT_CQ") == 0)
		goto EXIT_CQ;
	if (strcmp(str, "EXIT_DOMAIN") == 0)
		goto EXIT_DOMAIN;

#ifdef NOTIFICATION_SOFTWARE_AGENT
	(*communicator)->exit_softwareAgents = 1;
	pthread_join((*communicator)->destination_queue_software_agent,
			NULL /* void ** return value could go here */);
#endif
	/*correct cleanup requires empty local completion queue*/
	_foMPI_Comm_flush_all_internal(*communicator);
	EXIT_MEMORY:
	/*if we need some service memory exposed*/

	EXIT_ENDPOINT:

	/*
	 * Remove the endpoints to all of the ranks.
	 *
	 * Note: if there are outstanding events in the completion queue,
	 *       the endpoint can not be unbound.
	 */

	for (i = 0; i < (*communicator)->commsize; i++) {
		if (i == (*communicator)->commrank) {
			continue;
		}

		if ((*communicator)->endpoint_handles_array[i] == 0) {

			/*
			 * This endpoint does not exist.
			 */

			continue;
		}

		/*
		 * Unbind the remote address from the endpoint handler.
		 *     endpoint_handles_array is the endpoint handle that is being unbound
		 */
		status_gni = GNI_EpUnbind((*communicator)->endpoint_handles_array[i]);
		_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

		_foMPI_TRACE_LOG(5, "GNI_EpUnbind      remote rank: %4i EP:  %p\n", i,
				(*communicator)->endpoint_handles_array[i]);

		/*
		 * You must do an EpDestroy for each endpoint pair.
		 *
		 * Destroy the logical endpoint for each rank.
		 *     endpoint_handles_array is the endpoint handle that is being
		 *         destroyed.
		 */

		status_gni = GNI_EpDestroy((*communicator)->endpoint_handles_array[i]);
		_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

		_foMPI_TRACE_LOG(5, "GNI_EpDestroy     remote rank: %4i EP:  %p\n", i,
				(*communicator)->endpoint_handles_array[i]);

	}
	_foMPI_TRACE_LOG(3, "GNI_Ep Destroy Completed");
	/*
	 * Free allocated memory.
	 */

	_foMPI_FREE((void * ) (*communicator)->endpoint_handles_array);

	EXIT_CQ:

	/*
	 * Destroy the completion queue.
	 *     cq_handle is the handle that is being destroyed.
	 */

	status_gni = GNI_CqDestroy((*communicator)->cq_handle);

	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

	_foMPI_TRACE_LOG(3, "GNI_CqDestroy     source\n");

	EXIT_DOMAIN:

	/*
	 * Clean up the communication domain handle.
	 */

	status_gni = GNI_CdmDestroy((*communicator)->cdm_handle);
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

	_foMPI_TRACE_LOG(3, "GNI_CdmDestroy\n");
#endif

	_foMPI_FREE((void * ) *communicator);
	*communicator = foMPI_COMM_NULL;

	return status_gni;
}

#ifdef UGNI

static int _calcBindID(int cdm_id, int target_rank) {
	/* In this test bind_id refers to the instance id of the remote
	 *         communication domain that we are binding to.
	 */
	return (cdm_id * _foMPI_BIND_ID_MULTIPLIER) + target_rank;
}

static inline int ugni_Dequeue_local_event(_foMPI_Comm comm) {

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

		status = GNI_CqGetEvent(comm->cq_handle, &event_data);
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
				status = GNI_GetCompleted(comm->cq_handle, event_data, &event_post_desc_ptr);
				_check_gni_status(status, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

				_foMPI_Comm_free_ugni_data_descriptor(event_post_desc_ptr,comm);

				_foMPI_TRACE_LOG(3,
						"GNI_CqGetCompleted    source      type: %lu inst_id: %lu tid: %lu event: 0x%16.16lx\n",
						event_type, GNI_CQ_GET_INST_ID(event_data), GNI_CQ_GET_TID(event_data),
						event_data);

			} else {
				_foMPI_ERR_LOG("GNI_CqGetEvent    source      type: %lu inst_id: %lu event: 0x%16.16lx\n",
						event_type,
						GNI_CQ_GET_DATA(event_data), event_data);


			}
		} else if (status != GNI_RC_NOT_DONE) {
			//TODO: use dmapp

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

				_foMPI_ERR_LOG("ERROR CQ_OVERRUN detected\n");

			}

			cqErrorStr = (char *) _foMPI_ALLOC(256);
			if (cqErrorStr != NULL) {

				/*
				 * Print a user understandable error message.
				 */

				tmp_status = GNI_CqErrorStr(event_data, cqErrorStr, 256);
				if (tmp_status == GNI_RC_SUCCESS) {
					_foMPI_ERR_LOG("GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx GNI_CqErrorStr: %s\n",
							cqOverrunErrorStr, gni_err_str[status], status,
							GNI_CQ_GET_INST_ID(event_data), event_data, cqErrorStr);
				} else {

					/*
					 * Print the error number.
					 */

					_foMPI_ERR_LOG("GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
							cqOverrunErrorStr, gni_err_str[status], status,
							GNI_CQ_GET_INST_ID(event_data), event_data);
				}

				_foMPI_FREE(cqErrorStr);

			} else {

				/*
				 * Print the error number.
				 */

				_foMPI_ERR_LOG("GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
						cqOverrunErrorStr, gni_err_str[status], status,
						GNI_CQ_GET_INST_ID(event_data), event_data);
			}
			abort();
		} else {

			/*
			 * An event has not been received yet.
			 */
			// busy Wait
		}
	}

	return MPI_SUCCESS;
}

//TODO: not safe, buffer of pointers is better
inline gni_post_descriptor_t * _foMPI_Comm_get_ugni_data_descriptor(_foMPI_Comm comm){
	if (comm->counter_ugni_nbi >= comm->number_of_cq_entries) {
			//TODO: you can flush only ugni
			_foMPI_Comm_flush_all_internal(comm);
	}
	gni_post_descriptor_t *res = comm->data_desc_ptr[comm->extr_ind];
	//needs to be erased?
	memset(comm->data_desc_ptr[comm->extr_ind], 0, sizeof(gni_post_descriptor_t));
	comm->extr_ind= (comm->extr_ind + 1)  % comm->number_of_cq_entries;
	//calloc(1 , sizeof(gni_post_descriptor_t));
	//TODO: re enable this
	comm->counter_ugni_nbi++;
	return res;
}

static inline void _foMPI_Comm_free_ugni_data_descriptor(gni_post_descriptor_t * data_descr, _foMPI_Comm comm){
	//_foMPI_FREE(data_descr);
	comm->data_desc_ptr[comm->ins_ind]=data_descr;
	comm->ins_ind= (comm->ins_ind + 1) % comm->number_of_cq_entries ;
	comm->counter_ugni_nbi--;
}

/*The following function is copyrighted by CRAY  */
/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

static unsigned int get_gni_nic_address(int device_id) {
	int alps_address = -1;
	int alps_dev_id = -1;
	unsigned int address, cpu_id;
	gni_return_t status;
	int i;
	char *token, *p_ptr;

	p_ptr = getenv("PMI_GNI_DEV_ID");
	if (!p_ptr) {

		/*
		 * Get the nic address for the specified device.
		 */

		status = GNI_CdmGetNicAddress(device_id, &address, &cpu_id);
		_check_gni_status(status, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
	} else {

		/*
		 * Get the ALPS device id from the PMI_GNI_DEV_ID environment
		 * variable.
		 */

		while ((token = strtok(p_ptr, ":")) != NULL) {
			alps_dev_id = atoi(token);
			if (alps_dev_id == device_id) {
				break;
			}

			p_ptr = NULL;
		}

		assert(alps_dev_id != -1);

		p_ptr = getenv("PMI_GNI_LOC_ADDR");
		assert(p_ptr != NULL);

		i = 0;

		/*
		 * Get the nic address for the ALPS device.
		 */

		while ((token = strtok(p_ptr, ":")) != NULL) {
			if (i == alps_dev_id) {
				alps_address = atoi(token);
				break;
			}

			p_ptr = NULL;
			++i;
		}

		assert(alps_address != -1);
		address = alps_address;
	}

	return address;
}

#endif
