// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#include <intrinsics.h> 
#include "fompi.h"

/* TODO: syncs too often: for example a win_fence would also sync the rma operations of another window */
int foMPI_Win_fence(int assert, foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif 
  dmapp_return_t status;
 
  if (win->nbi_counter > 0) {
    status = dmapp_gsync_wait();
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    win->nbi_counter = 0;
  }

#ifdef XPMEM
  __sync_synchronize();
#endif

  MPI_Barrier( win->comm );
#ifdef PAPI
  timing_record( 15 );
#endif 
  return MPI_SUCCESS;
}
