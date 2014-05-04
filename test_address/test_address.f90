      ! Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
      ! Use of this source code is governed by a BSD-style license that can be
      ! found in the LICENSE file.
      
      program test_address

      implicit none

      include 'mpif.h'

      integer, dimension(:), allocatable :: value
      integer(kind=MPI_ADDRESS_KIND) :: location
      integer :: mpierr

      allocate( value(1), stat=mpierr )

      value(1) = 23

      call MPI_Init( mpierr )

      call MPI_Get_address( value(1), location, mpierr )

      write (*,*) "main program: value is", value(1), &
        ", location is", location, "and MPI_BOTTOM is", MPI_BOTTOM

      call fortran_sub( value(1) )

      call c_sub( value )

      call MPI_Finalize( mpierr )

      deallocate( value, stat=mpierr )

      end program test_address



      subroutine fortran_sub( value )

      implicit none

      include 'mpif.h'

      integer, dimension(1), intent(in) :: value
      integer(kind=MPI_ADDRESS_KIND) :: location
      integer :: mpierr

      call MPI_Get_address( value(1), location, mpierr )

      write (*,*) "fortran subroutine: value is", value(1), &
        ", location is", location, "and MPI_BOTTOM is", MPI_BOTTOM

      end subroutine fortran_sub
