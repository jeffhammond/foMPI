      ! Copyright (c) 2012 The Trustees of University of Illinois. All 
      ! rights reserved. Use of this source code is governed by a 
      ! BSD-style license that can be found in the LICENSE file.

      include 'mpif.h'

      ! constants for win_create_flavor
      integer, parameter :: fompi_win_flavor_create   = 0
      integer, parameter :: fompi_win_flavor_allocate = 1
      integer, parameter :: fompi_win_flavor_dynamic  = 2

      ! constants for win_model
      integer, parameter :: fompi_win_separate = 0
      integer, parameter :: fompi_win_unified  = 1

      ! asserts for one-sided communication
      integer, parameter :: fompi_mode_nocheck   =  1024
      integer, parameter :: fompi_mode_nostore   =  2048
      integer, parameter :: fompi_mode_noput     =  4096
      integer, parameter :: fompi_mode_noprecede =  8192
      integer, parameter :: fompi_mode_nosucceed = 16384

      ! error codes
      integer, parameter :: fompi_error_rma_no_sync               = 1
      integer, parameter :: fompi_error_rma_datatype_mismatch     = 2
      integer, parameter :: fompi_error_rma_no_lock_holding       = 3
      integer, parameter :: fompi_error_rma_sync_conflict         = 4
      integer, parameter :: fompi_error_rma_write_shared_conflict = 5
      integer, parameter :: fompi_error_rma_win_mem_not_found     = 6
      integer, parameter :: fompi_op_not_supported                = 7
      integer, parameter :: fompi_mpi_datatype_not_supported      = 8
      integer, parameter :: fompi_mpi_name_abreviated             = 9

      ! constants for foMPI_OP
      ! TODO: not real MPI_Op objects
      ! the numbers should be contiguous, since some sanity tests rely 
      ! on that fact
      integer, parameter :: fompi_sum     =  0
      integer, parameter :: fompi_prod    =  1
      integer, parameter :: fompi_max     =  2
      integer, parameter :: fompi_min     =  3
      integer, parameter :: fompi_land    =  4
      integer, parameter :: fompi_lor     =  5
      integer, parameter :: fompi_lxor    =  6
      integer, parameter :: fompi_band    =  7
      integer, parameter :: fompi_bor     =  8
      integer, parameter :: fompi_bxor    =  9
      integer, parameter :: fompi_maxloc  = 10
      integer, parameter :: fompi_minloc  = 11
      integer, parameter :: fompi_replace = 12
      integer, parameter :: fompi_no_op   = 13

      ! constants for lock type
      integer, parameter :: fompi_lock_shared    = 1
      integer, parameter :: fompi_lock_exclusive = 2

      ! constants for attributes
      integer, parameter :: fompi_win_base          = 0
      integer, parameter :: fompi_win_size          = 1
      integer, parameter :: fompi_win_disp_unit     = 2
      integer, parameter :: fompi_win_create_flavor = 3
      integer, parameter :: fompi_win_model         = 4

      integer, parameter :: fompi_request_null =    -1
      integer, parameter :: fompi_undefined    = 17383
      integer, parameter :: fompi_win_null     =    -1
