/*
 * fompi_req.c
 *
 *  Created on: Jun 30, 2014
 *      Author: Roberto Belli
 */
#include "fompi.h"

/*internal signatures*/
static inline int _foMPI_Test_Wait(foMPI_Request *request, MPI_Status *status, int wait, int *flag);
static inline int _foMPI_Testany_internal(int count, foMPI_Request array_of_requests[], int *index,
		int *flag, MPI_Status *status);

/*
 * int MPI_Wait(MPI_Request *request, MPI_Status *status)
 *
 * A call to MPI_WAIT returns when the operation identified by request is complete.
 * If the request is an active persistent request, it is marked inactive.
 * Any other type of request is and the request handle is set to MPI_REQUEST_NULL.
 * MPI_WAIT is a non-local operation. The call returns, in status, information on the completed operation.
 * The content of the status object for a receive operation can be accessed as described in Section 3.2.5.
 * The status object for a send operation may be queried by a call to MPI_TEST_CANCELLED (see Section 3.8).
 * One is allowed to call MPI_WAIT with a null or inactive request argument.
 * In this case the operation returns immediately with empty status.
 *
 */
int foMPI_Wait(foMPI_Request *request, MPI_Status *status) {
	int flag;
	return _foMPI_Test_Wait(request, status, 1, &flag);
}

/*
 * int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status)
 *
 * A call to MPI_TEST returns flag = true if the operation identified by request is complete.
 * In such a case, the status object is set to contain information on the completed operation.
 * If the request is an active persistent request, it is marked as inactive.
 * Any other type of request is deallocated and the request handle is set to MPI_REQUEST_NULL.
 * The call returns flag = false if the operation identified by request is not complete.
 * In this case, the value of the status object is undefined. MPI_TEST is a local operation.
 * The return status object for a receive operation carries information that can be accessed as described in Section 3.2.5.
 * The status object for a send operation carries information that can be accessed by a call to MPI_TEST_CANCELLED (see Section 3.8).
 * One is allowed to call MPI_TEST with a null or inactive request argument.
 * In such a case the operation returns with flag = true and empty status.
 * */
int foMPI_Test(foMPI_Request *request, int *flag, MPI_Status *status) {
	return _foMPI_Test_Wait(request, status, 0, flag);
}

/*
 * int MPI_Request_free(MPI_Request *request)
 *
 * Mark the request object for deallocation and set request to MPI_REQUEST_NULL.
 * An ongoing communication that is associated with the request will be allowed to complete.
 * The request will be deallocated only after its completion.
 *
 * returns MPI_ERR_REQUEST if the request is not permanent, MPI_SUCCESS otherwise
 */
int foMPI_Request_free(foMPI_Request *request) {
	if (*request == foMPI_REQUEST_NULL) {
		return MPI_SUCCESS;
	}
	if ((*request)->free_fun != NULL) {
		(*request)->free_fun((*request)->extra_state);
	}
	_foMPI_FREE((void *) *request);
	*request = foMPI_REQUEST_NULL;
	return MPI_SUCCESS;
}

/*
 * int MPI_Waitany(int count, MPI_Request array_of_requests[], int *index, MPI_Status *status)
 *
 * Blocks until one of the operations associated with the active requests in the array has completed.
 * If more than one operation is enabled and can terminate, one is arbitrarily chosen.
 * Returns in index the index of that request in the array and returns in status the status of the completing operation.
 * (The array is indexed from zero in C, and from one in Fortran.)
 * If the request is an active persistent request, it is marked inactive.
 * Any other type of request is deallocated and the request handle is set to MPI_REQUEST_NULL.
 * The array_of_requests list may contain null or inactive handles.
 * If the list contains no active handles (list has length zero or all entries are null or inactive), then the call returns
 * immediately with index = MPI_UNDEFINED, and an empty status.
 * The execution of MPI_WAITANY(count, array_of_requests, index, status) has the same
 * effect as the execution of MPI_WAIT(&array_of_requests[i], status),
 * where i is the value returned by index (unless the value of index is MPI_UNDEFINED).
 * MPI_WAITANY with an array containing one active entry is equivalent to MPI_WAIT.
 */
int foMPI_Waitany(int count, foMPI_Request array_of_requests[], int *index, MPI_Status *status) {
	int flag = _foMPI_FALSE;
	*index = MPI_UNDEFINED;
	int test_res = 0;
	while (!(flag == _foMPI_TRUE)) {
		test_res = _foMPI_Testany_internal(count, array_of_requests, index, &flag, status);
	}
	return test_res;
}

/*
 * int MPI_Testany(int count, MPI_Request array_of_requests[], int *index, int *flag, MPI_Status *status)
 *
 * Tests for completion of either one or none of the operations associated with active handles.
 * In the former case, it returns flag = true, returns in index the index of this request in the array,
 * and returns in status the status of that operation. If the request is an active persistent request,
 * it is marked as inactive.
 * Any other type of request is deallocated and the handle is set to MPI_REQUEST_NULL.
 * In the latter case (no operation completed), it returns flag = false, returns a value of MPI_UNDEFINED in index and status is undefined.
 * The array may contain null or inactive handles.
 * If the array contains no active handles then the call returns immediately with flag = true,
 * index = MPI_UNDEFINED, and an empty status.
 * If the array of requests contains active handles then the execution of MPI_TESTANY(count, array_of_requests, index, status)
 * has the same effect as the execution of MPI_TEST( &array_of_requests[i], flag, status), for i=0, 1 ,..., count-1,
 * in some arbitrary order, until one call returns flag = true, or all fail.
 * In the former case, index is set to the last value of i, and in the latter case, it is set to MPI_UNDEFINED.
 * MPI_TESTANY with an array containing one active entry is equivalent to MPI_TEST.
 * */
int foMPI_Testany(int count, foMPI_Request array_of_requests[], int *index, int *flag,
		MPI_Status *status) {
	return _foMPI_Testany_internal(count, array_of_requests, index, flag, status);
}

/*
 * int MPI_Start(MPI_Request *request)
 *
 * The argument, request, is a handle returned by one of the previous five calls.
 * The associated request should be inactive. The request becomes active once the call is made.
 * If the request is for a send with ready mode, then a matching receive should be posted before the call is made.
 * The communication buffer should not be modified after the call, and until the operation completes.
 * The call is local, with similar semantics to the nonblocking communication operations described in Section 3.7.
 * That is, a call to MPI_START with a request created by MPI_SEND_INIT starts a communication in the same manner as a call to MPI_ISEND;
 * a call to MPI_START with a request created by MPI_BSEND_INIT starts a communication in the same manner as a call to MPI_IBSEND;
 * and so on.
 *
 * Returns MPI_ERR_REQUEST if the request is not PERSISTENT
 *
 * */
int foMPI_Start(foMPI_Request *request) {
	int res = MPI_SUCCESS;
	if (*request == foMPI_REQUEST_NULL) {
		return MPI_SUCCESS;
	}
	if ((*request)->type != _foMPI_REQUEST_PERSISTENT) {
		return MPI_ERR_REQUEST;
	}
	if ((*request)->activate_fun != NULL) {
		res = (*request)->activate_fun((*request)->extra_state);
	}
	if (res == MPI_SUCCESS) {
		(*request)->active = _foMPI_TRUE;
	}
	return res;
}

/*internal functions*/

static inline int _foMPI_Testany_internal(int count, foMPI_Request array_of_requests[], int *index,
		int *flag, MPI_Status *status) {

	*index = MPI_UNDEFINED;
	*flag = _foMPI_FALSE;
	/*randomizing the offset for starting every time from a different index*/
	if (glob_info.rand_seed == 0) {
		glob_info.rand_seed = (int) time(NULL);
	}
	int r = rand_r(&(glob_info.rand_seed)) % count;
	int i;
	int test_res = 0;
	int inactive = 0;

	for (i = 0; i < count; i++) {
		int randindex = (i + r) % count;
		if ( array_of_requests[randindex] == foMPI_REQUEST_NULL || (array_of_requests[randindex]->type == _foMPI_REQUEST_PERSISTENT
				&& array_of_requests[randindex]->active == 0)) {
			inactive++;
			continue;
		}
		test_res = foMPI_Test(&(array_of_requests[randindex]), flag, status);
		if (*flag == _foMPI_TRUE) {
			*index = randindex;
			return test_res;
		}else {
			foMPI_Start(&(array_of_requests[randindex]));
		}
	}
	if (inactive == count) {
		*flag = _foMPI_TRUE;
		*index = MPI_UNDEFINED;
		return MPI_SUCCESS;
	}

	/*can be MPI_SUCCESS or MPI_ERR_IN_STATUS or MPI_ERR_PENDING*/
	return test_res;
}

static inline int _foMPI_Test_Wait(foMPI_Request *request, MPI_Status *status, int wait, int *flag) {

	/*
	 * One is allowed to call MPI_WAIT with a null or inactive request argument.
	 * In this case the operation returns immediately with empty status.
	 */
	*flag = _foMPI_FALSE;
	if (*request == foMPI_REQUEST_NULL
			|| ((*request)->type == _foMPI_REQUEST_PERSISTENT && (*request)->active == 0)) {
		if (status != MPI_STATUS_IGNORE) {
			status->MPI_SOURCE = MPI_ANY_SOURCE;
			status->MPI_TAG = MPI_ANY_TAG;
			status->MPI_ERROR = MPI_SUCCESS;
			//MPI_Status_set_elements(status, MPI_DATATYPE_NULL, 0); gives an error
			MPI_Status_set_elements(status, MPI_BYTE , 0);
			MPI_Status_set_cancelled(status, 0);
		}
		*flag = _foMPI_TRUE;
		return MPI_SUCCESS;
	}

	int res;
	if (wait) {
		res = (*request)->wait_fun((*request)->extra_state, status);
	} else {
		/* sets the flag */
		res = (*request)->test_fun((*request)->extra_state, flag, status);

	}

	/*
	 * the status is extracted from the request; in the case of test and wait,
	 * if the request was nonpersistent, it is freed, and becomes inactive if it was persistent.
	 * A communication completes when all participating operations complete.
	 *
	 */
	if ((*request)->type == _foMPI_REQUEST_NOT_PERSISTENT) {
		foMPI_Request_free(request);
	} else {
		(*request)->active = 0;
	}
	/*can return MPI_SUCCESS or MPI_ERR_IN_STATUS  */
	return res;
}

