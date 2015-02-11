/*
 * fompi_not.c
 *
 *  Created on: Jul 3, 2014
 *      Author: Roberto Belli
 */
#include "fompi.h"
#ifdef UGNI
/*signature internal functions*/
static inline int _foMPI_NotificationReceived_direct(void *extra_state, int *flag,
		MPI_Status *status, int wait);

//static inline int _foMPI_DecodeID(uint16_t *rank, uint16_t *id_msg, uint32_t id_encoded);
static int _foMPI_NotificationReceived_direct_test(void *extra_state, int *flag, MPI_Status *status);
static int _foMPI_NotificationReceived_direct_wait(void *extra_state, MPI_Status *status);
static int _foMPI_Notification_free(void *extra_state);
static int _foMPI_Notification_activate(void *extra_state);

int foMPI_Notify_init(foMPI_Win win, int src_rank, int tag, int count, foMPI_Request *request) {
#ifndef NOCHECKS
	if (!(tag == foMPI_ANY_TAG || (tag >= 0 && tag <= foMPI_TAG_UB))) {
		return MPI_ERR_TAG;
	}
	if (!(src_rank == foMPI_ANY_SOURCE || (src_rank >= 0 && src_rank <= win->commsize))) {
		return MPI_ERR_RANK;
	}
#endif
	*request = (foMPI_Request_t *) _foMPI_ALLOC(sizeof(foMPI_Request_t));
	(*request)->test_fun = &_foMPI_NotificationReceived_direct_test;
	(*request)->wait_fun = &_foMPI_NotificationReceived_direct_wait;
	(*request)->free_fun = &_foMPI_Notification_free;
	(*request)->activate_fun = &_foMPI_Notification_activate;
	(*request)->cancel_fun = NULL;
	(*request)->type = _foMPI_REQUEST_PERSISTENT;
	(*request)->active = 0;

	foMPI_Notification_request_t *arg = _foMPI_ALLOC(sizeof(foMPI_Notification_request_t));
	(*request)->extra_state = (void *) arg;
	assert((*request)->extra_state != NULL);
	arg->count = count;
	arg->found = 0;
	arg->win = win;
	arg->source_rank = src_rank;
	arg->tag = tag;
	return MPI_SUCCESS;
}

static int _foMPI_NotificationReceived_direct_test(void *extra_state, int *flag, MPI_Status *status) {
	return _foMPI_NotificationReceived_direct(extra_state, flag, status, 0);
}

static int _foMPI_NotificationReceived_direct_wait(void *extra_state, MPI_Status *status) {
	int dummy_flag = 0;
	return _foMPI_NotificationReceived_direct(extra_state, &dummy_flag, status, 1);
}

static int _foMPI_Notification_free(void *extra_state) {
	_foMPI_FREE(extra_state);
	return MPI_SUCCESS;
}

static int _foMPI_Notification_activate(void *extra_state) {
	/*if i already found all the notifications, I reset the found counter*/
	if (((foMPI_Notification_request_t *) extra_state)->found
			== ((foMPI_Notification_request_t *) extra_state)->count) {
		((foMPI_Notification_request_t *) extra_state)->found = 0;
	}
	return MPI_SUCCESS;
}

/*
 * Internal functions
 *
 */
static inline int _foMPI_NotificationReceived_direct(void *args_ptr, int *flag, MPI_Status *status,
		int wait) {
	foMPI_Notification_request_t * fun_args = (foMPI_Notification_request_t *) args_ptr;
	*flag = 0;
	_foMPI_TRACE_LOG(1, "Waiting for notification from:%d tag:%d \n", fun_args->source_rank,
			fun_args->tag);
	/* checking the queue of unexpected events for the event */
	fompi_notif_uq_t *dscqueue = fun_args->win->destination_cq_discarded;
	uint16_t found_source, found_tag;
	int local_rank = -1;
	if (fun_args->source_rank != foMPI_ANY_SOURCE) {
		local_rank = foMPI_onnode_rank_global_to_local(fun_args->source_rank, fun_args->win);
	}
	/*Check on the set of unexpected received notification */

	if (_fompi_notif_uq_isEmpty(dscqueue) == _foMPI_FALSE) {
//TODO: If software agent is needed, here you have to wait only till you collect all the notifications, maybe with Exponential Backoff.
		if (fun_args->source_rank == foMPI_ANY_SOURCE && fun_args->tag == foMPI_ANY_TAG) {
			/*i'm searching for the oldest available notification */
			while (fun_args->count > fun_args->found
					&& _fompi_notif_uq_pop(dscqueue, &found_source, &found_tag) != _foMPI_NO_MATCH) {
				fun_args->found++;
				_foMPI_TRACE_LOG(2,
						"Notification %d/%d found (unexpected queue) from:%"PRIu16" tag:%"PRIu16"\n",
						fun_args->found, fun_args->count, found_source, found_tag);
			}
		} else if (fun_args->tag == foMPI_ANY_TAG) {
			/*i'm searching for the oldest available notification with specified sender*/
			while (fun_args->count > fun_args->found
					&& _fompi_notif_uq_search_remove_src(dscqueue, (uint16_t) fun_args->source_rank,
							&found_tag) != _foMPI_NO_MATCH) {
				fun_args->found++;
				found_source = fun_args->source_rank;
				/*found_tag filled by procedure*/
				_foMPI_TRACE_LOG(2,
						"Notification %d/%d found (unexpected queue) from:%"PRIu16" tag:%"PRIu16"\n",
						fun_args->found, fun_args->count, found_source, found_tag);
			}
		} else if (fun_args->source_rank == foMPI_ANY_SOURCE) {
			/*i'm searching for the oldest available notification with specified tag from any sender*/
			while (fun_args->count > fun_args->found
					&& _fompi_notif_uq_search_remove_tag(dscqueue, (uint16_t) fun_args->tag,
							&found_source) != _foMPI_NO_MATCH) {
				fun_args->found++;
				found_tag = fun_args->tag;
				_foMPI_TRACE_LOG(2,
						"Notification %d/%d found (unexpected queue) from:%"PRIu16" tag:%"PRIu16"\n",
						fun_args->found, fun_args->count, found_source, found_tag);
			}
		} else {
			/*source and tag are both specified */
			while (fun_args->count > fun_args->found
					&& _fompi_notif_uq_search_remove(dscqueue, (uint16_t) fun_args->source_rank,
							(uint16_t) fun_args->tag) != _foMPI_NO_MATCH) {
				fun_args->found++;
				found_tag = fun_args->tag;
				found_source = fun_args->source_rank;
				_foMPI_TRACE_LOG(2,
						"Notification %d/%d found (unexpected queue) from:%"PRIu16" tag:%"PRIu16"\n",
						fun_args->found, fun_args->count, found_source, found_tag);
			}
		}
		_foMPI_TRACE_LOG(2, "Notifications (%d/%d) Found in unexpected set \n", fun_args->found,
				fun_args->count);
	}

#ifndef NOTIFICATION_SOFTWARE_AGENT
	/* if we want to enable a software agent to poll the queue, the conjunction point can be the previous queue
	 * the software agent poll the NIC queue and fill the unexpected queue of events
	 * */

	/*event not found in discarded queue: checking in the completion queue */
	gni_cq_entry_t event_data;
	uint64_t event_inst_id; /*only least significative 4bytes are source-defined*/
	gni_return_t status_gni = GNI_RC_SUCCESS;

	_foMPI_TRACE_LOG(3, "check uGNI source completion queue\n");
	while (fun_args->count > fun_args->found) {
#ifdef XPMEM
		/*check XPMEM*/
		if (xpmem_notif_pop(&found_source, &found_tag, fun_args->win) == 0) {
			/*found notification*/
			if ((found_source == fun_args->source_rank || fun_args->source_rank == foMPI_ANY_SOURCE)
					&& (found_tag == fun_args->tag || fun_args->tag == foMPI_ANY_TAG)) {
				fun_args->found++;
				_foMPI_TRACE_LOG(2, "XPMEM_queue found %d/%d \n", fun_args->found, fun_args->count);
				/*found source & found_tag already set by the previous decodeID*/
				continue;
			} else {
				/*not interesting element, insert in backup_queue*/
				_fompi_notif_uq_push(dscqueue, found_source, found_tag);
				_foMPI_TRACE_LOG(2, "Discarded queue insert from:%"PRIu16" tag:%"PRIu16"\n",
						fun_args->win->group_ranks[found_source], found_tag);
			}
			/*in this case if it finds the tag continue starting again from this queue,
			 *  else it goes to the one of the  NIC. I hope that this helps in reducing the starvation*/
		}
#endif
		if (local_rank == -1) {
			/* Get the next event from the NIC source completion queue. */
			status_gni = GNI_CqGetEvent(fun_args->win->destination_cq_handle, &event_data);
			if (status_gni == GNI_RC_SUCCESS) {
				_foMPI_TRACE_LOG(5, "GNI_CqGetEvent SUCCESS\n");
				/* we retrieved a valid event : getting the instance id only last 4 bytes are source-defined */
				event_inst_id = GNI_CQ_GET_REM_INST_ID(event_data);
#ifndef NDEBUG
				uint64_t event_type = GNI_CQ_GET_TYPE(event_data);
				_foMPI_TRACE_LOG(5, "Received event of type :  %d\n", (int ) event_type);
#endif
				_foMPI_DecodeID(&found_source, &found_tag, event_inst_id);
				if ((found_source == fun_args->source_rank
						|| fun_args->source_rank == foMPI_ANY_SOURCE)
						&& (found_tag == fun_args->tag || fun_args->tag == foMPI_ANY_TAG)) {
					fun_args->found++;
					_foMPI_TRACE_LOG(2, "uGNI_queue found %d/%d \n", fun_args->found,
							fun_args->count);
					/*found source & found_tag already set by the previous decodeID*/
				} else {
					/*not interesting element, insert in backup_queue*/
					_fompi_notif_uq_push(dscqueue, found_source, found_tag);
					_foMPI_TRACE_LOG(2, "Discarded queue insert from:%"PRIu16" tag:%"PRIu16"\n",
							fun_args->win->group_ranks[found_source], found_tag);
				}
				/*something happened : loop again */
				continue;
			} else if (status_gni == GNI_RC_NOT_DONE) {
				_foMPI_TRACE_LOG(5, "GNI_CqGetEvent GNI_RC_NOT_DONE (wait = %d)\n", wait);
				if (wait) {
					/*busy wait should decrease latency*/
					continue;
				} else {
					/*status is set to 0*/
					return MPI_SUCCESS;
				}
			} else {
				/* An error occurred getting the event. */
				char *cqErrorStr;
				char *cqOverrunErrorStr = "";
				gni_return_t tmp_status = GNI_RC_SUCCESS;
#ifdef CRAY_CONFIG_GHAL_ARIES
				uint32_t status_code;

				status_code = GNI_CQ_GET_STATUS(event_data);
				if (status_code == A_STATUS_AT_PROTECTION_ERR) {
					abort();
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
					_foMPI_ERR_LOG(
							"ERROR CQ_OVERRUN detected : you can increase the size increasing the value in ugni_config.h \n");
				}
				cqErrorStr = (char *) _foMPI_ALLOC(256);
				if (cqErrorStr != NULL) {
					/* Print a user understandable error message. */
					tmp_status = GNI_CqErrorStr(event_data, cqErrorStr, 256);
					if (tmp_status == GNI_RC_SUCCESS) {
						_foMPI_ERR_LOG(
								"GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx GNI_CqErrorStr: %s\n",
								cqOverrunErrorStr, gni_err_str[status_gni], status_gni,
								GNI_CQ_GET_REM_INST_ID(event_data), event_data, cqErrorStr);
					} else {
						/* Print the error number. */
						_foMPI_ERR_LOG(
								"GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
								cqOverrunErrorStr, gni_err_str[status_gni], status_gni,
								GNI_CQ_GET_REM_INST_ID(event_data), event_data);
					}
					_foMPI_FREE(cqErrorStr);
				} else {
					/* Print the error number.*/
					_foMPI_ERR_LOG(
							"GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
							cqOverrunErrorStr, gni_err_str[status_gni], status_gni,
							GNI_CQ_GET_REM_INST_ID(event_data), event_data);
				}
				abort();
			}
		}
	}
#endif

	if (fun_args->count == fun_args->found) {
		/*if we reach that point we should have found all the notification needed*/
		if (status != MPI_STATUS_IGNORE) {
			if (fun_args->count == 1) {
				/*if the requested notification was only one, i set the exact value in the status field*/
				status->MPI_ERROR = MPI_SUCCESS;
				status->MPI_SOURCE = found_source;
				status->MPI_TAG = found_tag;
			} else {
				/*we cannot set all the values received here, we set the requested ones*/
				status->MPI_ERROR = MPI_SUCCESS;
				status->MPI_SOURCE = fun_args->source_rank;
				status->MPI_TAG = fun_args->tag;
			}
		}
		/*found tag, found source already set by function*/
		_foMPI_TRACE_LOG(1, "Notifications (%d/%d) Received source:%d tag:%d \n", fun_args->count,
				fun_args->found,
				(fun_args->count == 1) ? fun_args->win->group_ranks[found_source] : -1,
				(fun_args->count == 1) ? found_tag : -1);
		*flag = 1;
		return MPI_SUCCESS;
	}
	/* flag is already set to 0*/
	_foMPI_TRACE_LOG(1, "Notifications (%d/%d) Received -> test not ready \n", fun_args->count,
			fun_args->found);
	return MPI_SUCCESS;
}

inline int _foMPI_DecodeID(uint16_t *rank, uint16_t *id_msg, uint32_t id_encoded) {
	*id_msg = id_encoded & 0xffff;
	*rank = id_encoded >> 16;
	return MPI_SUCCESS;
}

#ifdef NOTIFICATION_SOFTWARE_AGENT
/*
 * TODO: this code is kept because was tried before, but now needs some changes to get it working again.
 * The fastest way to retrieve notifications was setting the handler during the creation of the
 *  queue and polling with a thread (fast dispatcher) the queue.*/

void foMPI_NotificationHandler(gni_cq_entry_t *event_data, void * win_handle) {
	assert(event_data != NULL || communicator_handle != NULL);
	uint16_t found_source, found_tag;
	foMPI_Win win = (foMPI_Win) win_handle;
	fompi_notif_uq_t *dscqueue = win->destination_cq_discarded;
	uint64_t event_inst_id; /*only least significative 6bytes are source-defined*/
	/* we retrieved a valid event : getting the instance id only last 4 bytes are source-defined */
	event_inst_id = GNI_CQ_GET_REM_INST_ID(event_data);
#ifndef NDEBUG
	uint64_t event_type = GNI_CQ_GET_TYPE(event_data);
	_foMPI_TRACE_LOG(5, "Received event of type :  %d\n", (int ) event_type);
#endif
	_foMPI_DecodeID(&found_source, &found_tag, event_inst_id);
	/* insert in backup_queue*/
	_fompi_notif_uq_push(dscqueue, found_source, found_tag);
	_foMPI_TRACE_LOG(2, "Discarded queue insert notification from:%"PRIu16" tag:%"PRIu16"\n",
			fun_args->win->group_ranks[found_source], found_tag);

	/*something happened : loop again */
}

static void* foMPI_NotificationDispatcherFast(void *communicator_handle) {

	gni_cq_entry_t event_data;
	gni_return_t status_gni = GNI_RC_SUCCESS;
	foMPI_Comm comm_handle = (foMPI_Comm) communicator_handle;
	PMI_job_desc_t *job = &(glob_info.curr_job);

	TRACE_LOG(1,"[%s] Rank: %4i NotificationDispatcherFast START destination software agent communicator : %p \n",
			job->uts_info.nodename, job->rank_id, comm_handle);

	while (comm_handle->exit_softwareAgents == 0) {

		/*
		 * Get the next event from the specified completion queue handle.
		 */
#if CQ_MODE_DESTINATION == GNI_CQ_NOBLOCK
		status_gni = GNI_CqGetEvent(comm_handle->destination_cq_handle, &event_data);
		if (status_gni != GNI_RC_NOT_DONE && status_gni != GNI_RC_SUCCESS ) {
			_foMPI_ERR_LOG(" GNI_CqWaitEvent Error %s code %d \n", gni_err_str[status_gni], status_gni);
			goto QUEUE_ERROR;
		}
		/*busy wait*/
#else
		status_gni = GNI_CqWaitEvent(comm_handle->destination_cq_handle, foMPI_WAIT_EVENT_TIMEOUT,
				&event_data);
		if (status_gni != GNI_RC_SUCCESS && status_gni != GNI_RC_TIMEOUT ) {
			_foMPI_ERR_LOG("GNI_CqWaitEvent Error %s code %d \n",
					gni_err_str[status_gni], status_gni);
			goto QUEUE_ERROR;
		}
		/*blocking wait*/
#endif

	}

	TRACE_LOG(1,"[%s] Rank: %4i NotificationDispatcherFast EXIT destination software agent\n",
			job->uts_info.nodename, job->rank_id);

	return NULL;

	/* An error occurred getting the event. */
	char *cqErrorStr;
	char *cqOverrunErrorStr = "";
	gni_return_t tmp_status = GNI_RC_SUCCESS;
#ifdef CRAY_CONFIG_GHAL_ARIES
	uint32_t status_code;

	status_code = GNI_CQ_GET_STATUS(event_data);
	if (status_code == A_STATUS_AT_PROTECTION_ERR) {
		abort();
	}
#endif
	QUEUE_ERROR:
	/*
	 * Did the event queue overrun condition occurred?
	 * This means that all of the event queue entries were used up
	 * and another event occurred, i.e. there was no entry available
	 * to put the new event into.
	 */

	if (GNI_CQ_OVERRUN(event_data)) {
		cqOverrunErrorStr = "CQ_OVERRUN detected ";
		_foMPI_ERR_LOG(
				"ERROR CQ_OVERRUN detected : you can increase the size increasing the value in ugni_config.h \n");
	}
	cqErrorStr = (char *) _foMPI_ALLOC(256);
	if (cqErrorStr != NULL) {
		/* Print a user understandable error message. */
		tmp_status = GNI_CqErrorStr(event_data, cqErrorStr, 256);
		if (tmp_status == GNI_RC_SUCCESS) {
			_foMPI_ERR_LOG(
					"GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx GNI_CqErrorStr: %s\n",
					cqOverrunErrorStr, gni_err_str[status_gni], status_gni,
					GNI_CQ_GET_REM_INST_ID(event_data), event_data, cqErrorStr);
		} else {
			/* Print the error number. */
			_foMPI_ERR_LOG(
					"GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
					cqOverrunErrorStr, gni_err_str[status_gni], status_gni,
					GNI_CQ_GET_REM_INST_ID(event_data), event_data);
		}
		_foMPI_FREE(cqErrorStr);
	} else {
		/* Print the error number.*/
		_foMPI_ERR_LOG(
				"GNI_CqGetEvent    ERROR %s status: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
				cqOverrunErrorStr, gni_err_str[status_gni], status_gni,
				GNI_CQ_GET_REM_INST_ID(event_data), event_data);
	}
	abort();
}

#endif
#endif
