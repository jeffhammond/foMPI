      ! Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
      ! Use of this source code is governed by a BSD-style license that can be
      ! found in the LICENSE file.

      program fence
      
      implicit none

      include 'fompif.h'

      integer :: mpierr, x, win, numtasks, taskid

      integer(kind=MPI_ADDRESS_KIND) :: target_disp
      integer(kind=MPI_ADDRESS_KIND) :: win_size = 8

      call foMPI_Init(mpierr)

      call MPI_Comm_size(MPI_COMM_WORLD, numtasks, mpierr)
      call MPI_Comm_rank(MPI_COMM_WORLD, taskid, mpierr)

      x = 10
      target_disp = 0

      if (taskid .eq. 0) then
        write (6,*) 'resolution of mpi_wtime', MPI_Wtick()
      endif

      write (6,*) 'before', taskid, x 

      call foMPI_Win_create(x, win_size, 8, MPI_INFO_NULL, MPI_COMM_WORLD, win, mpierr)

      call foMPI_Win_fence(0, win, mpierr)

      if(taskid.eq.0) then
        call foMPI_Accumulate(x, 1, MPI_INTEGER, 1, target_disp, 1, MPI_INTEGER, foMPI_SUM, win, mpierr)
      endif

      call foMPI_Win_fence(0, win, mpierr)

      call foMPI_Win_free(win, mpierr)

      write (6,*) 'after', taskid, x 

      call foMPI_Finalize(mpierr)

      end program fence
