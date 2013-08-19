      module fompi

      use mpi

      implicit none

      ! constants for win_create_flavor
      integer, parameter :: fompi_win_flavor_create               =     0
      integer, parameter :: fompi_win_flavor_allocate             =     1
      integer, parameter :: fompi_win_flavor_dynamic              =     2

      ! constants for win_model
      integer, parameter :: fompi_win_separate                    =     0
      integer, parameter :: fompi_win_unified                     =     1

      ! asserts for one-sided communication
      integer, parameter :: fompi_mode_nocheck                    =  1024
      integer, parameter :: fompi_mode_nostore                    =  2048
      integer, parameter :: fompi_mode_noput                      =  4096
      integer, parameter :: fompi_mode_noprecede                  =  8192
      integer, parameter :: fompi_mode_nosucceed                  = 16384

      ! error codes
      integer, parameter :: fompi_error_rma_no_sync               =     1
      integer, parameter :: fompi_error_rma_datatype_mismatch     =     2
      integer, parameter :: fompi_error_rma_no_lock_holding       =     3
      integer, parameter :: fompi_error_rma_sync_conflict         =     4
      integer, parameter :: fompi_error_rma_write_shared_conflict =     5
      integer, parameter :: fompi_error_rma_win_mem_not_found     =     6
      integer, parameter :: fompi_op_not_supported                =     7
      integer, parameter :: fompi_mpi_datatype_not_supported      =     8
      integer, parameter :: fompi_mpi_name_abreviated             =     9

      ! constants for foMPI_OP
      ! TODO: not real MPI_Op objects
      ! the numbers should be contiguous, since some sanity tests rely on that fact
      integer, parameter :: fompi_sum                             =     0
      integer, parameter :: fompi_prod                            =     1
      integer, parameter :: fompi_max                             =     2
      integer, parameter :: fompi_min                             =     3
      integer, parameter :: fompi_land                            =     4
      integer, parameter :: fompi_lor                             =     5
      integer, parameter :: fompi_lxor                            =     6
      integer, parameter :: fompi_band                            =     7
      integer, parameter :: fompi_bor                             =     8
      integer, parameter :: fompi_bxor                            =     9
      integer, parameter :: fompi_maxloc                          =    10
      integer, parameter :: fompi_minloc                          =    11
      integer, parameter :: fompi_replace                         =    12
      integer, parameter :: fompi_no_op                           =    13

      ! constants for lock type
      integer, parameter :: fompi_lock_shared                     =     1
      integer, parameter :: fompi_lock_exclusive                  =     2

      ! constants for attributes
      integer, parameter :: fompi_win_base                        =     0
      integer, parameter :: fompi_win_size                        =     1
      integer, parameter :: fompi_win_disp_unit                   =     2
      integer, parameter :: fompi_win_create_flavor               =     3
      integer, parameter :: fompi_win_model                       =     4

      integer, parameter :: fompi_request_null                    =    -1
      integer, parameter :: fompi_undefined                       = 17383
      integer, parameter :: fompi_win_null                        =    -1

      interface

          ! window creation

!        subroutine foMPI_Win_create( base, size, disp_unit, info, comm, win, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: base ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer(kind=MPI_ADDRESS_KIND), intent(in) :: size
!        integer, intent(in) :: disp_unit
!        integer, intent(in) :: info
!        integer, intent(in) :: comm
!        integer, intent(out) :: win
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Win_create

        
        subroutine foMPI_Win_allocate( size, disp_unit, info, comm, baseptr, win, ierror )

        use mpi

        integer(kind=MPI_ADDRESS_KIND), intent(in) :: size
        integer, intent(in) :: disp_unit
        integer, intent(in) :: info
        integer, intent(in) :: comm
        integer(kind=MPI_ADDRESS_KIND) :: baseptr ! removed intent, so Fortran accepts null()
        integer, intent(out) :: win
        integer, intent(out) :: ierror
  
        end subroutine foMPI_Win_allocate


        subroutine foMPI_Win_allocate_cptr( size, disp_unit, info, comm, baseptr, win, ierror )

        use, intrinsic :: iso_c_binding, only: c_ptr
        use mpi

        integer(kind=MPI_ADDRESS_KIND), intent(in) :: size
        integer, intent(in) :: disp_unit
        integer, intent(in) :: info
        integer, intent(in) :: comm
        type(c_ptr) :: baseptr ! removed intent, so Fortran accepts null()
        integer, intent(out) :: win
        integer, intent(out) :: ierror

        end subroutine


        subroutine foMPI_Win_create_dynamic( info, comm, win, ierror )

        integer, intent(in) :: info
        integer, intent(in) :: comm
        integer, intent(out) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_create_dynamic


        subroutine foMPI_Win_free( win, ierror )

        integer, intent(inout) :: win
        integer, intent(out) :: ierror 

        end subroutine foMPI_Win_free


!        subroutine foMPI_Win_attach( win, base, size, ierror )
!
!        use mpi
!
!        integer, intent(in) :: win
!        integer, dimension(:), intent(inout) :: base ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer(kind=MPI_ADDRESS_KIND), intent(in) :: size
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Win_attach


!        subroutine foMPI_Win_detach( win, base, ierror )
!
!        integer, intent(in) :: win
!        integer, dimension(:), intent(inout) :: base ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Win_detach

        ! synchronization

        subroutine foMPI_Win_fence( assert, win, ierror )

        integer, intent(in) :: assert
        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_fence

        
        subroutine foMPI_Win_start( group, assert, win, ierror )

        integer, intent(in) :: group
        integer, intent(in) :: assert
        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_start


        subroutine foMPI_Win_complete( win, ierror )

        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_complete


        subroutine foMPI_Win_post( group, assert, win, ierror )

        integer, intent(in) :: group
        integer, intent(in) :: assert
        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_post

        
        subroutine foMPI_Win_wait( win, ierror )

        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_wait


        subroutine foMPI_Win_test( win, flag, ierror )

        integer, intent(in) :: win
        logical, intent(out) :: flag
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_test


        subroutine foMPI_Win_lock( lock_type, rank, assert, win, &
          ierror )

        integer, intent(in) :: lock_type
        integer, intent(in) :: rank
        integer, intent(in) :: assert
        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_lock


        subroutine foMPI_Win_unlock( rank, win, ierror )

        integer, intent(in) :: rank
        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_unlock


        subroutine foMPI_Win_lock_all( assert, win, ierror )

        integer, intent(in) :: assert
        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_lock_all


        subroutine foMPI_Win_unlock_all( win, ierror )

        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_unlock_all


        subroutine foMPI_Win_flush( rank, win, ierror )

        integer, intent(in) :: rank
        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_flush


        subroutine foMPI_Win_flush_all( win, ierror )

        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_flush_all


        subroutine foMPI_Win_flush_local( rank, win, ierror )

        integer, intent(in) :: rank
        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_flush_local


        subroutine foMPI_Win_flush_local_all( win, ierror )

        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_flush_local_all


        subroutine foMPI_Win_sync( win, ierror )

        integer, intent(in) :: win
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_sync

        ! communication

!        subroutine foMPI_Put( origin_addr, origin_count, &
!          origin_datatype, target_rank, target_disp, target_count, &
!          target_datatype, win, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: origin_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: origin_count
!        integer, intent(in) :: origin_datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND) :: target_disp
!        integer, intent(in) :: target_count
!        integer, intent(in) :: target_datatype
!        integer, intent(in) :: win
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Put


!        subroutine foMPI_Get( origin_addr, origin_count, &
!          origin_datatype, target_rank, target_disp, target_count, &
!          target_datatype, win, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(out) :: origin_addr ! intent correct? ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: origin_count
!        integer, intent(in) :: origin_datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND) :: target_disp
!        integer, intent(in) :: target_count
!        integer, intent(in) :: target_datatype
!        integer, intent(in) :: win
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Get


!        subroutine foMPI_Accumulate( origin_addr, origin_count, &
!          origin_datatype, target_rank, target_disp, target_count, &
!          target_datatype, op, win, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: origin_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: origin_count
!        integer, intent(in) :: origin_datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND), intent(in) :: target_disp
!        integer, intent(in) :: target_count
!        integer, intent(in) :: target_datatype
!        integer, intent(in) :: op
!        integer, intent(in) :: win
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Accumulate


!        subroutine foMPI_Get_accumulate( origin_addr, origin_count, &
!          origin_datatype, result_addr, result_count, result_datatype, &
!          target_rank, target_disp, target_count, target_datatype, op, &
!          win, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: origin_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: origin_count
!        integer, intent(in) :: origin_datatype
!        integer, dimension(:), intent(out) :: result_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: result_count
!        integer, intent(in) :: result_datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND), intent(in) :: target_disp
!        integer, intent(in) :: target_count
!        integer, intent(in) :: target_datatype
!        integer, intent(in) :: op
!        integer, intent(in) :: win
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Get_accumulate

        
!        subroutine foMPI_Fetch_and_op( origin_addr, result_addr, &
!          datatype, target_rank, target_disp, op, win, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: origin_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, dimension(:), intent(out) :: result_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND), intent(in) :: target_disp
!        integer, intent(in) :: op
!        integer, intent(in) :: win
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Fetch_and_op


!        subroutine foMPI_Compare_and_swap( origin_addr, compare_addr, &
!          result_addr, datatype, target_rank, target_disp, win, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: origin_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, dimension(:), intent(in) :: compare_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, dimension(:), intent(out) :: result_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND), intent(in) :: target_disp
!        integer, intent(in) :: win
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Compare_and_swap
          
        ! request based communication

!        subroutine foMPI_Rput( origin_addr, origin_count, &
!          origin_datatype, target_rank, target_disp, target_count, &
!          target_datatype, win, request, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: origin_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: origin_count
!        integer, intent(in) :: origin_datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND) :: target_disp
!        integer, intent(in) :: target_count
!        integer, intent(in) :: target_datatype
!        integer, intent(in) :: win
!        integer, intent(out) :: request
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Rput


!        subroutine foMPI_Rget( origin_addr, origin_count, &
!          origin_datatype, target_rank, target_disp, target_count, &
!          target_datatype, win, request, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(out) :: origin_addr ! intent correct? ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: origin_count
!        integer, intent(in) :: origin_datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND) :: target_disp
!        integer, intent(in) :: target_count
!        integer, intent(in) :: target_datatype
!        integer, intent(in) :: win
!        integer, intent(out) :: request
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Rget


!        subroutine foMPI_Raccumulate( origin_addr, origin_count, &
!          origin_datatype, target_rank, target_disp, target_count, &
!          target_datatype, op, win, request, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: origin_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: origin_count
!        integer, intent(in) :: origin_datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND), intent(in) :: target_disp
!        integer, intent(in) :: target_count
!        integer, intent(in) :: target_datatype
!        integer, intent(in) :: op
!        integer, intent(in) :: win
!        integer, intent(out) :: request
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Raccumulate


!        subroutine foMPI_Rget_accumulate( origin_addr, origin_count, &
!          origin_datatype, result_addr, result_count, result_datatype, &
!          target_rank, target_disp, target_count, target_datatype, op, &
!          win, request, ierror )
!
!        use mpi
!
!        integer, dimension(:), intent(in) :: origin_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: origin_count
!        integer, intent(in) :: origin_datatype
!        integer, dimension(:), intent(out) :: result_addr ! doesn't work since array is assume-shaped, and also would only be working with integer
!        integer, intent(in) :: result_count
!        integer, intent(in) :: result_datatype
!        integer, intent(in) :: target_rank
!        integer(kind=MPI_ADDRESS_KIND), intent(in) :: target_disp
!        integer, intent(in) :: target_count
!        integer, intent(in) :: target_datatype
!        integer, intent(in) :: op
!        integer, intent(in) :: win
!        integer, intent(out) :: request
!        integer, intent(out) :: ierror
!
!        end subroutine foMPI_Rget_accumulate

        ! window attributes

        subroutine foMPI_Win_get_group( win, group, ierror )

        integer, intent(in) :: win
        integer, intent(out) :: group
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_get_group


        subroutine foMPI_Win_set_name( win, win_name, ierror )

        integer, intent(in) :: win
        character*(*), intent(in) :: win_name
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_set_name


        subroutine foMPI_Win_get_name( win, win_name, result_len, ierror )

        integer, intent(in) :: win
        character*(*), intent(out) :: win_name
        integer, intent(out) :: result_len
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_get_name


        subroutine foMPI_Win_create_keyval( win_copy_attr_fn, win_delete_attr_fn, win_keyval, extra_state, ierror )

        use mpi

        external :: win_copy_attr_fn
        external :: win_delete_attr_fn
        integer, intent(inout) :: win_keyval
        integer(kind=MPI_ADDRESS_KIND) :: extra_state
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_create_keyval


        subroutine foMPI_Win_free_keyval( win_keyval, ierror )

        integer, intent(inout) :: win_keyval
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_free_keyval

        
        subroutine foMPI_Win_set_attr( win, win_keyval, attribute_val, ierror )

        use mpi

        integer, intent(in) :: win
        integer, intent(in) :: win_keyval
        integer(kind=MPI_ADDRESS_KIND), intent(in) :: attribute_val
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_set_attr


        subroutine foMPI_Win_get_attr( win, win_keyval, attribute_val, flag, ierror )

        use mpi

        integer, intent(in) :: win
        integer, intent(in) :: win_keyval
        integer(kind=MPI_ADDRESS_KIND), intent(out) :: attribute_val
        logical, intent(out) :: flag
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_get_attr


        subroutine foMPI_Win_delete_attr(  win, win_keyval, ierror )

        integer, intent(in) :: win
        integer, intent(in) :: win_keyval
        integer, intent(out) :: ierror

        end subroutine foMPI_Win_delete_attr

        ! overloaded functions

        subroutine foMPI_Init( ierror )
      
        integer, intent(out) :: ierror

        end subroutine foMPI_Init


        subroutine foMPI_Finalize( ierror )

        integer, intent(out) :: ierror

        end subroutine foMPI_Finalize

        
        subroutine foMPI_Wait( request, fstatus, ierror )

        use mpi

        integer, intent(inout) :: request
        integer, dimension(MPI_STATUS_SIZE), intent(out) :: fstatus
        integer, intent(out) :: ierror

        end subroutine foMPI_Wait


        subroutine foMPI_Test( request, flag, fstatus, ierror )

        use mpi

        integer, intent(inout) :: request
        logical, intent(out) :: flag
        integer, dimension(MPI_STATUS_SIZE), intent(out) :: fstatus
        integer, intent(out) :: ierror

        end subroutine foMPI_Test


        subroutine foMPI_Type_free( datatype, ierror )

        integer, intent(inout) :: datatype
        integer, intent(out) :: ierror

        end subroutine foMPI_Type_free


      end interface

      interface foMPI_Win_allocate
        procedure foMPI_Win_allocate, foMPI_Win_allocate_cptr
      end interface foMPI_Win_allocate

      end module fompi
