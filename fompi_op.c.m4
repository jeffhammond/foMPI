dnl/*
dnl * Copyright (c) 2012 The Trustees of University of Illinois.
dnl *                    All rights reserved.
dnl * Copyright (c) 2006 The Trustees of Indiana University and Indiana
dnl *                    University Research and Technology
dnl *                    Corporation.  All rights reserved.
dnl * Copyright (c) 2006 The Technical University of Chemnitz. All 
dnl *                    rights reserved.
dnl */
dnl
dnl gerro:
dnl Code copied from libNBC by Torsten Hoefler.
dnl Changes were applied.
dnl
dnl this m4 code generate all MPI intrinsic operations
dnl every macro is prefixed with m4_ to retain clarity (this means that
dnl everything prefixed with m4_ will be replaced by m4!)
dnl
dnl ########## define all MPI intrinsic Operations and appropriate C code #############
define(m4_OP_foMPI_MIN, `if(m4_ARG1$1 > m4_ARG2$1) m4_ARG3$1 = m4_ARG2$1; else m4_ARG3$1 = m4_ARG1$1;')dnl
define(m4_OP_foMPI_MAX, `if(m4_ARG1$1 < m4_ARG2$1) m4_ARG3$1 = m4_ARG2$1; else m4_ARG3$1 = m4_ARG1$1;')dnl
define(m4_OP_foMPI_SUM, `m4_ARG3$1 = m4_ARG1$1 + m4_ARG2$1;')dnl
define(m4_OP_foMPI_PROD, `m4_ARG3$1 = m4_ARG1$1 * m4_ARG2$1;')dnl
define(m4_OP_foMPI_LAND, `m4_ARG3$1 = m4_ARG1$1 && m4_ARG2$1;')dnl
define(m4_OP_foMPI_BAND, `m4_ARG3$1 = m4_ARG1$1 & m4_ARG2$1;')dnl
define(m4_OP_foMPI_LOR, `m4_ARG3$1 = m4_ARG1$1 || m4_ARG2$1;')dnl
define(m4_OP_foMPI_BOR, `m4_ARG3$1 = m4_ARG1$1 | m4_ARG2$1;')dnl
define(m4_OP_foMPI_LXOR, `m4_ARG3$1 =  ( !m4_ARG1$1 != !m4_ARG2$1 );')dnl
define(m4_OP_foMPI_BXOR, `m4_ARG3$1 = ((m4_ARG1$1) ^ (m4_ARG2$1));')dnl
define(m4_OP_foMPI_MINLOC, `if(m4_ARG1$1_VAL > m4_ARG2$1_VAL) { 
          m4_ARG3$1_VAL = m4_ARG2$1_VAL; m4_ARG3$1_RANK = m4_ARG2$1_RANK; 
        } else {
		      if(m4_ARG1$1_VAL < m4_ARG2$1_VAL) {
            m4_ARG3$1_VAL = m4_ARG1$1_VAL; m4_ARG3$1_RANK = m4_ARG1$1_RANK; 
		      } else {
            if (m4_ARG1$1_RANK > m4_ARG2$1_RANK) {
			        m4_ARG3$1_VAL = m4_ARG2$1_VAL; m4_ARG3$1_RANK = m4_ARG2$1_RANK;
			      } else {
              m4_ARG3$1_VAL = m4_ARG1$1_VAL; m4_ARG3$1_RANK = m4_ARG1$1_RANK;
			      }
		      }
        }')dnl
define(m4_OP_foMPI_MAXLOC, `if(m4_ARG1$1_VAL < m4_ARG2$1_VAL) { 
          m4_ARG3$1_VAL = m4_ARG2$1_VAL; m4_ARG3$1_RANK = m4_ARG2$1_RANK; 
        } else {
		      if (m4_ARG1$1_VAL > m4_ARG2$1_VAL) { 
            m4_ARG3$1_VAL = m4_ARG1$1_VAL; m4_ARG3$1_RANK = m4_ARG1$1_RANK; 
		      } else {
            if(m4_ARG1$1_RANK < m4_ARG2$1_RANK) { 
			        m4_ARG3$1_VAL = m4_ARG1$1_VAL; m4_ARG3$1_RANK = m4_ARG1$1_RANK;
			      } else {
              m4_ARG3$1_VAL = m4_ARG2$1_VAL; m4_ARG3$1_RANK = m4_ARG2$1_RANK;
			      }
		      }
        }')dnl
dnl ########### THIS is faster as the unrolled code :-(( #####
define(m4_IF, `if(op == $1) {
      for(i=0; i<count; i++) {
define(m4_ARG1_$2, `*(((m4_CTYPE_$2*)buf1) + i)')dnl
define(m4_ARG2_$2, `*(((m4_CTYPE_$2*)buf2) + i)')dnl
define(m4_ARG3_$2, `*(((m4_CTYPE_$2*)buf3) + i)')dnl
        m4_OP_$1(_$2) 
      }
    }')dnl
dnl ############################################### MINLOC and MAXLOC
define(m4_LOCIF, `if(op == $1) {
      for(i=0; i<count; i++) {
        typedef struct {
          m4_CTYPE1_$2 val;
          m4_CTYPE2_$2 rank;
        } m4_CTYPE3_$2;
        m4_CTYPE3_$2 *ptr1, *ptr2, *ptr3;
                            
        ptr1 = ((m4_CTYPE3_$2*)buf1) + i;
        ptr2 = ((m4_CTYPE3_$2*)buf2) + i;
        ptr3 = ((m4_CTYPE3_$2*)buf3) + i;
      
define(m4_ARG1_VAL, ptr1->val)dnl
define(m4_ARG2_VAL, ptr2->val)dnl
define(m4_ARG3_VAL, ptr3->val)dnl
define(m4_ARG1_RANK, ptr1->rank)dnl
define(m4_ARG2_RANK, ptr2->rank)dnl
define(m4_ARG3_RANK, ptr3->rank)dnl
        m4_OP_$1 
      }  
    }')dnl
dnl ############################################### complex numbers foMPI_SUM (Fortran only)
define(m4_COMPSUMIF, `if(op == foMPI_SUM) {
      for(i=0; i<count; i++) {
        typedef struct {
          m4_CTYPE1_$1 real;
          m4_CTYPE1_$1 imag;
        } m4_CTYPE2_$1;
        m4_CTYPE2_$1 *ptr1, *ptr2, *ptr3;
                            
        ptr1 = ((m4_CTYPE2_$1*)buf1) + i;
        ptr2 = ((m4_CTYPE2_$1*)buf2) + i;
        ptr3 = ((m4_CTYPE2_$1*)buf3) + i;

        ptr3->real = ptr2->real + ptr1->real; 
        ptr3->imag = ptr2->imag + ptr1->imag; 
      }  
  }')dnl
dnl ############################################### complex numbers foMPI_PROD (Fortran only)
define(m4_COMPPRODIF, `if(op == foMPI_PROD) {
      for(i=0; i<count; i++) {
        typedef struct {
          m4_CTYPE1_$1 real;
          m4_CTYPE1_$1 imag;
        } m4_CTYPE2_$1;
        m4_CTYPE2_$1 *ptr1, *ptr2, *ptr3;

        ptr1 = ((m4_CTYPE2_$1*)buf1) + i;
        ptr2 = ((m4_CTYPE2_$1*)buf2) + i;
        ptr3 = ((m4_CTYPE2_$1*)buf3) + i;

        ptr3->real = ptr1->real * ptr2->real - ptr1->imag * ptr2->imag; 
        ptr3->imag = ptr1->real * ptr2->imag + ptr1->imag * ptr2->real; 
      }  
  }')dnl
dnl ############################################### complex numbers foMPI_SUM (C only)
define(m4_C_COMPSUMIF, `if(op == foMPI_SUM) {
      for(i=0; i<count; i++) {
        m4_CTYPE_$1 *ptr1, *ptr2, *ptr3;
                            
        ptr1 = ((m4_CTYPE_$1*)buf1) + i;
        ptr2 = ((m4_CTYPE_$1*)buf2) + i;
        ptr3 = ((m4_CTYPE_$1*)buf3) + i;

        *ptr3 = *ptr2 + *ptr1; 
      }  
  }')dnl
dnl ############################################### complex numbers foMPI_PROD (C only)
define(m4_C_COMPPRODIF, `if(op == foMPI_PROD) {
      for(i=0; i<count; i++) {
        m4_CTYPE_$1 *ptr1, *ptr2, *ptr3;

        ptr1 = ((m4_CTYPE_$1*)buf1) + i;
        ptr2 = ((m4_CTYPE_$1*)buf2) + i;
        ptr3 = ((m4_CTYPE_$1*)buf3) + i;

        *ptr3 = *ptr1 * *ptr2; 
      }  
  }')dnl
dnl ########################################################## 
define(m4_TYPE, `if(type == $1) { 
    m4_OPTYPE_$1($1) 
  }')dnl
dnl ########## define possible operations for each type 
dnl
dnl /* C integer: */
dnl ####### MPI_INT ########
define(m4_OPTYPE_MPI_INT, `define(m4_CTYPE_$1, `signed int')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_LONG ########
define(m4_OPTYPE_MPI_LONG, `define(m4_CTYPE_$1, `signed long')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_SHORT ########
define(m4_OPTYPE_MPI_SHORT, `define(m4_CTYPE_$1, `signed short int')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UNSIGNED_SHORT ########
define(m4_OPTYPE_MPI_UNSIGNED_SHORT, `define(m4_CTYPE_$1, `unsigned short')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UNSIGNED ########
define(m4_OPTYPE_MPI_UNSIGNED, `define(m4_CTYPE_$1, `unsigned int')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UNSIGNED_LONG ########
define(m4_OPTYPE_MPI_UNSIGNED_LONG, `define(m4_CTYPE_$1, `unsigned long')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_LONG_LONG_INT ########
define(m4_OPTYPE_MPI_LONG_LONG_INT, `define(m4_CTYPE_$1, `signed long long')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_LONG_LONG ########
define(m4_OPTYPE_MPI_LONG_LONG, `define(m4_CTYPE_$1, `long long')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UNSIGNED_LONG_LONG ########
define(m4_OPTYPE_MPI_UNSIGNED_LONG_LONG, `define(m4_CTYPE_$1, `unsigned long long')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_SIGNED_CHAR ########
define(m4_OPTYPE_MPI_SIGNED_CHAR, `define(m4_CTYPE_$1, `signed char')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UNSIGNED_CHAR ########
define(m4_OPTYPE_MPI_UNSIGNED_CHAR, `define(m4_CTYPE_$1, `unsigned char')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_INT8_T ########
define(m4_OPTYPE_MPI_INT8_T, `define(m4_CTYPE_$1, `int8_t')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_INT16_T ########
define(m4_OPTYPE_MPI_INT16_T, `define(m4_CTYPE_$1, `int16_t')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_INT32_T ########
define(m4_OPTYPE_MPI_INT32_T, `define(m4_CTYPE_$1, `int32_t')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl 
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_INT64_T ########
define(m4_OPTYPE_MPI_INT64_T, `define(m4_CTYPE_$1, `int64_t')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UINT8_T ########
define(m4_OPTYPE_MPI_UINT8_T, `define(m4_CTYPE_$1, `uint8_t')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UINT16_T ########
define(m4_OPTYPE_MPI_UINT16_T, `define(m4_CTYPE_$1, `uint16_t')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UINT32_T ########
define(m4_OPTYPE_MPI_UINT32_T, `define(m4_CTYPE_$1, `uint32_t')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_UINT64_T ########
define(m4_OPTYPE_MPI_UINT64_T, `define(m4_CTYPE_$1, `uint64_t')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_LAND, $1) else dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_BOR, $1) else dnl
m4_IF(foMPI_LXOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl /* FORTRAN integer */
dnl ####### MPI_INTEGER ########
define(m4_OPTYPE_MPI_INTEGER, `define(m4_CTYPE_$1, `int')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_BAND, $1) else
m4_IF(foMPI_BOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_INTEGER1 ########
define(m4_OPTYPE_MPI_INTEGER1, `define(m4_CTYPE_$1, `int8_t')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_BAND, $1) else
m4_IF(foMPI_BOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_INTEGER2 ########
define(m4_OPTYPE_MPI_INTEGER2, `define(m4_CTYPE_$1, `int16_t')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_BAND, $1) else
m4_IF(foMPI_BOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_INTEGER4 ########
define(m4_OPTYPE_MPI_INTEGER4, `define(m4_CTYPE_$1, `int32_t')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_BAND, $1) else
m4_IF(foMPI_BOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_INTEGER8 ########
define(m4_OPTYPE_MPI_INTEGER8, `define(m4_CTYPE_$1, `int64_t')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_BAND, $1) else
m4_IF(foMPI_BOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl /* TODO: What to do with handles from MPI_TYPE_CREATE_F90_INTEGER and
dnl  * MPI_INTEGER16?
dnl  */
dnl
dnl /* Floating point */
dnl ####### MPI_FLOAT ########
define(m4_OPTYPE_MPI_FLOAT, `define(m4_CTYPE_$1, `float')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_DOUBLE ########
define(m4_OPTYPE_MPI_DOUBLE, `define(m4_CTYPE_$1, `double')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_REAL ########
define(m4_OPTYPE_MPI_REAL, `define(m4_CTYPE_$1, `float')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_DOUBLE_PRECISION ########
define(m4_OPTYPE_MPI_DOUBLE_PRECISION, `define(m4_CTYPE_$1, `double')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_LONG_DOUBLE ########
define(m4_OPTYPE_MPI_LONG_DOUBLE, `define(m4_CTYPE_$1, `long double')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_REAL4 ########
define(m4_OPTYPE_MPI_REAL4, `define(m4_CTYPE_$1, `float')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_REAL8 ########
define(m4_OPTYPE_MPI_REAL8, `define(m4_CTYPE_$1, `double')dnl 
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_REAL16 ########
define(m4_OPTYPE_MPI_REAL16, `define(m4_CTYPE_$1, `long double')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl /* TODO: What to do with handles from MPI_TYPE_CREATE_F90_REAL and
dnl  * MPI_REAL2?
dnl  */
dnl 
dnl /* Logical */
dnl ####### MPI_LOGICAL ########
define(m4_OPTYPE_MPI_LOGICAL, `define(m4_CTYPE_$1, `int32_t')dnl 
m4_IF(foMPI_LAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_LXOR, $1) return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_C_BOOL ########
define(m4_OPTYPE_MPI_C_BOOL, `define(m4_CTYPE_$1, `_Bool')dnl 
m4_IF(foMPI_LAND, $1) else m4_IF(foMPI_LOR, $1) else m4_IF(foMPI_LXOR, $1) return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl /* TODO: What to do with MPI_CXX_BOOL? */
dnl
dnl /* Complex */
dnl ####### MPI_COMPLEX (Fortran) ########
define(m4_OPTYPE_MPI_COMPLEX, `define(m4_CTYPE1_$1, `float')define(m4_CTYPE2_$1, `complexfloat')dnl 
m4_COMPSUMIF($1) else m4_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_C_COMPLEX ########
define(m4_OPTYPE_MPI_C_COMPLEX, `define(m4_CTYPE_$1, `float _Complex')dnl 
m4_C_COMPSUMIF($1) else m4_C_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_C_FLOAT_COMPLEX ########
define(m4_OPTYPE_MPI_C_FLOAT_COMPLEX, `define(m4_CTYPE_$1, `float _Complex')dnl 
m4_C_COMPSUMIF($1) else m4_C_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_C_DOUBLE_COMPLEX ########
define(m4_OPTYPE_MPI_C_DOUBLE_COMPLEX, `define(m4_CTYPE_$1, `double _Complex')dnl 
m4_C_COMPSUMIF($1) else m4_C_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_C_LONG_DOUBLE_COMPLEX ########
define(m4_OPTYPE_MPI_C_LONG_DOUBLE_COMPLEX, `define(m4_CTYPE_$1, `long double _Complex')dnl 
m4_C_COMPSUMIF($1) else m4_C_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_DOUBLE_COMPLEX (Fortran) ########
define(m4_OPTYPE_MPI_DOUBLE_COMPLEX, `define(m4_CTYPE1_$1, `double')define(m4_CTYPE2_$1, `complexdouble')dnl 
m4_COMPSUMIF($1) else m4_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_COMPLEX8 (Fortran) ########
define(m4_OPTYPE_MPI_COMPLEX8, `define(m4_CTYPE1_$1, `float')define(m4_CTYPE2_$1, `complexfloat')dnl 
m4_COMPSUMIF($1) else m4_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_COMPLEX16 (Fortran) ########
define(m4_OPTYPE_MPI_COMPLEX16, `define(m4_CTYPE1_$1, `double')define(m4_CTYPE2_$1, `complexdouble')dnl 
m4_COMPSUMIF($1) else m4_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_COMPLEX32 (Fortran) ########
define(m4_OPTYPE_MPI_COMPLEX32, `define(m4_CTYPE1_$1, `long double')define(m4_CTYPE2_$1, `complexlongdouble')dnl 
m4_COMPSUMIF($1) else m4_COMPPRODIF($1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl /* TODO: What to do with handles from MPI_TYPE_CREATE_F90_COMPLEX and MPI_COMPLEX4, 
dnl  * MPI_CXX_FLOAT_COMPLEX, MPI_CXX_DOUBLE_COMPLEX, MPI_CXX_LONG_DOUBLE_COMPLEX?
dnl  */
dnl
dnl /* Byte */
dnl ####### MPI_BYTE ########
define(m4_OPTYPE_MPI_BYTE, `define(m4_CTYPE_$1, `char')dnl
m4_IF(foMPI_BAND, $1) else m4_IF(foMPI_BOR, $1) else dnl 
m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl /* Multi-language types */
dnl ####### MPI_AINT ########
define(m4_OPTYPE_MPI_AINT, `define(m4_CTYPE_$1, `MPI_Aint')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_BAND, $1) else dnl 
m4_IF(foMPI_BOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_OFFSET ########
define(m4_OPTYPE_MPI_OFFSET, `define(m4_CTYPE_$1, `MPI_Offset')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_BAND, $1) dnl
m4_IF(foMPI_BOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_COUNT ########
define(m4_OPTYPE_MPI_COUNT, `define(m4_CTYPE_$1, `MPI_Count')dnl
m4_IF(foMPI_MIN, $1) else m4_IF(foMPI_MAX, $1) else dnl 
m4_IF(foMPI_SUM, $1) else m4_IF(foMPI_PROD, $1) else m4_IF(foMPI_BAND, $1) dnl 
m4_IF(foMPI_BOR, $1) else m4_IF(foMPI_BXOR, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl /* C composed datatypes */
dnl ####### MPI_FLOAT_INT ########
define(m4_OPTYPE_MPI_FLOAT_INT, `define(m4_CTYPE1_$1, `float')define(m4_CTYPE2_$1, `int')define(m4_CTYPE3_$1, `float_int')dnl 
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_DOUBLE_INT ########
define(m4_OPTYPE_MPI_DOUBLE_INT, `define(m4_CTYPE1_$1, `double')define(m4_CTYPE2_$1, `int')define(m4_CTYPE3_$1, `double_int')dnl
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_LONG_INT ########
define(m4_OPTYPE_MPI_LONG_INT, `define(m4_CTYPE1_$1, `long')define(m4_CTYPE2_$1, `int')define(m4_CTYPE3_$1, `long_int')dnl 
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_2INT ########
define(m4_OPTYPE_MPI_2INT, `define(m4_CTYPE1_$1, `int')define(m4_CTYPE2_$1, `int')define(m4_CTYPE3_$1, `int_int')dnl 
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_SHORT_INT ########
define(m4_OPTYPE_MPI_SHORT_INT, `define(m4_CTYPE1_$1, `short')define(m4_CTYPE2_$1, `int')define(m4_CTYPE3_$1, `short_int')dnl
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_LONG_DOUBLE_INT ########
define(m4_OPTYPE_MPI_LONG_DOUBLE_INT, `define(m4_CTYPE1_$1, `long double')define(m4_CTYPE2_$1, `int')define(m4_CTYPE3_$1, `long_double_int')dnl 
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl /* FORTRAN composed datatypes */
dnl ####### MPI_2REAL ########
define(m4_OPTYPE_MPI_2REAL, `define(m4_CTYPE1_$1, `float')define(m4_CTYPE2_$1, `float')define(m4_CTYPE3_$1, `float_float')dnl
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_2DOUBLE_PRECISION ########
define(m4_OPTYPE_MPI_2DOUBLE_PRECISION, `define(m4_CTYPE1_$1, `double')define(m4_CTYPE2_$1, `double')define(m4_CTYPE3_$1, `double_double')dnl
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### MPI_2INTEGER ########
define(m4_OPTYPE_MPI_2INTEGER, `define(m4_CTYPE1_$1, `int')define(m4_CTYPE2_$1, `int')define(m4_CTYPE3_$1, `integer_integer')dnl 
m4_LOCIF(foMPI_MAXLOC, $1) else m4_LOCIF(foMPI_MINLOC, $1) else return foMPI_OP_NOT_SUPPORTED;')dnl
dnl
dnl ####### begin the real program :-) #########
dnl

/****************** THIS FILE is automatically generated *********************
 * changes will be deleted at the next generation of this file - see fompi_op.c.m4 */

#include <complex.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdint.h>

#include "fompi.h"

int foMPI_RMA_op(void *buf3, void *buf1, void *buf2, foMPI_Op op, MPI_Datatype type, int count) {
  int i;

dnl /* C integer */
  m4_TYPE(MPI_INT) else dnl
m4_TYPE(MPI_LONG) else dnl
m4_TYPE(MPI_SHORT) else dnl
m4_TYPE(MPI_UNSIGNED_SHORT) else dnl
m4_TYPE(MPI_UNSIGNED) else dnl
m4_TYPE(MPI_UNSIGNED_LONG) else dnl
m4_TYPE(MPI_LONG_LONG_INT) else dnl
m4_TYPE(MPI_LONG_LONG) else dnl
m4_TYPE(MPI_UNSIGNED_LONG_LONG) else dnl
m4_TYPE(MPI_SIGNED_CHAR) else dnl
m4_TYPE(MPI_UNSIGNED_CHAR) else dnl
m4_TYPE(MPI_INT8_T) else dnl
m4_TYPE(MPI_INT16_T) else dnl
m4_TYPE(MPI_INT32_T) else dnl
m4_TYPE(MPI_INT64_T) else dnl
m4_TYPE(MPI_UINT8_T) else dnl
m4_TYPE(MPI_UINT16_T) else dnl
m4_TYPE(MPI_UINT32_T) else dnl
m4_TYPE(MPI_UINT64_T) else dnl
dnl /* FORTRAN integer */
m4_TYPE(MPI_INTEGER) else dnl
m4_TYPE(MPI_INTEGER1) else dnl
m4_TYPE(MPI_INTEGER2) else dnl
m4_TYPE(MPI_INTEGER4) else dnl
m4_TYPE(MPI_INTEGER8) else dnl
dnl /* Floating point */
m4_TYPE(MPI_FLOAT) else dnl
m4_TYPE(MPI_DOUBLE) else dnl
m4_TYPE(MPI_REAL) else dnl
m4_TYPE(MPI_DOUBLE_PRECISION) else dnl
m4_TYPE(MPI_LONG_DOUBLE) else dnl
m4_TYPE(MPI_REAL4) else dnl
m4_TYPE(MPI_REAL8) else dnl
m4_TYPE(MPI_REAL16) else dnl
dnl /* Logical */
m4_TYPE(MPI_LOGICAL) else dnl
m4_TYPE(MPI_C_BOOL) else dnl
dnl /* Complex */
m4_TYPE(MPI_COMPLEX) else dnl
m4_TYPE(MPI_C_COMPLEX) else dnl
m4_TYPE(MPI_C_FLOAT_COMPLEX) else dnl
m4_TYPE(MPI_C_DOUBLE_COMPLEX) else dnl
m4_TYPE(MPI_C_LONG_DOUBLE_COMPLEX) else dnl
m4_TYPE(MPI_DOUBLE_COMPLEX) else dnl
m4_TYPE(MPI_COMPLEX8) else dnl
m4_TYPE(MPI_COMPLEX16) else dnl
m4_TYPE(MPI_COMPLEX32) else dnl
dnl /* Byte */
m4_TYPE(MPI_BYTE) else dnl
dnl /* Multi-language types */
m4_TYPE(MPI_AINT) else dnl
m4_TYPE(MPI_OFFSET) else dnl
dnl m4_TYPE(MPI_COUNT) else dnl
dnl /* Fortran composed datatypes */
m4_TYPE(MPI_2REAL) else dnl
m4_TYPE(MPI_2DOUBLE_PRECISION) else dnl
m4_TYPE(MPI_2INTEGER) else dnl
dnl /* C composed datatypes */
m4_TYPE(MPI_FLOAT_INT) else dnl
m4_TYPE(MPI_DOUBLE_INT) else dnl
m4_TYPE(MPI_LONG_INT) else dnl
m4_TYPE(MPI_2INT) else dnl
m4_TYPE(MPI_SHORT_INT) else dnl
m4_TYPE(MPI_LONG_DOUBLE_INT) else dnl
return foMPI_DATATYPE_NOT_SUPPORTED;
  
  return MPI_SUCCESS;
}
