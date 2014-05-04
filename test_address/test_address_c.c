// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mpi.h>
#include <stdio.h>

#define F77_FUNC_(name,NAME) name ## _

void F77_FUNC_(c_sub,C_SUB)(int *value ) {

  MPI_Aint location;

  MPI_Get_address( value, &location );

  printf(" c_sub: value is %i, location is %lu and MPI_BOTTOM is %lu\n", *value, location, MPI_BOTTOM);

}
