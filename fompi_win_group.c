// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fompi.h"

int foMPI_Win_get_group(foMPI_Win win, MPI_Group *group) {

  MPI_Comm_group( win->comm, group );

  return MPI_SUCCESS;
}
