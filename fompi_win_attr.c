#include <stdio.h>

#include "fompi.h"

int foMPI_Win_create_keyval(MPI_Win_copy_attr_function *win_copy_attr_fn, MPI_Win_delete_attr_function *win_delete_attr_fn, int *win_keyval, void *extra_state) {

  printf("foMPI doesn't support MPI_Win_create_keyval at the moment.\n");

  return MPI_ERR_KEYVAL; /* TODO: Not consistent with the MPI-3.0 standard. */
}

int foMPI_Win_free_keyval(int *win_keyval) {

  printf("foMPI doesn't support MPI_Win_free_keyval at the moment.\n");

  return MPI_ERR_KEYVAL;
}

int foMPI_Win_set_attr(foMPI_Win win, int win_keyval, void *attribute_val) {

  printf("foMPI doesn't support MPI_Win_set_attr at the moment.\n");

  return MPI_ERR_KEYVAL;
}

int foMPI_Win_get_attr(foMPI_Win win, int win_keyval, void *attribute_val, int *flag) {

  switch (win_keyval) {
    case foMPI_WIN_BASE:
      if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
        *(void**)attribute_val = MPI_BOTTOM;
      } else {
        *(void**)attribute_val = win->win_array[win->commrank].base;
      }
      *flag = 1;
      break;
    case foMPI_WIN_SIZE:
      if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
        *(MPI_Aint*)attribute_val = 0;
      } else {
        *(MPI_Aint*)attribute_val = win->win_array[win->commrank].size;
      }
      *flag = 1;
      break;
    case foMPI_WIN_DISP_UNIT:
      if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
        *(int*)attribute_val = 1;
      } else {
        *(int*)attribute_val = win->win_array[win->commrank].disp_unit;
      }
      *flag = 1;
      break;
    case foMPI_WIN_CREATE_FLAVOR:
      *(int*) attribute_val = win->create_flavor;
      *flag = 1;
      break;
    case foMPI_WIN_MODEL:
      *(int*)attribute_val = foMPI_WIN_UNIFIED;
      *flag = 1;
      break;
    default:
      *flag = 0;
  }

  return MPI_SUCCESS;
}

int foMPI_Win_delete_attr(foMPI_Win win, int win_keyval) {

  printf("foMPI doesn't support MPI_Win_delete_attr at the moment.\n");

  return MPI_ERR_KEYVAL;
}
