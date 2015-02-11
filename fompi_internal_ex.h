/*
 * fompi_internal_ex.h
 *
 *  Created on: Jul 1, 2014
 *      Author: Roberto Belli
 */

#ifndef FOMPI_INTERNAL_EX_H_
#define FOMPI_INTERNAL_EX_H_

/*Memory management Macros*/
#define _foMPI_MEMORY_ALIGNMENT 64

#ifndef NDEBUG
#ifdef UGNI
#define _foMPI_ALIGNED_ALLOC(addrptrptr, size)  if( posix_memalign( ( (void**) addrptrptr ) ,_foMPI_MEMORY_ALIGNMENT, ( size) )!=0) { \
									fprintf(stderr, "[%s] Rank: %4i (%s %d): Failure on posix_memalign \n",glob_info.curr_job.uts_info.nodename, glob_info.curr_job.rank_id, __FILE__, __LINE__); \
									abort(); \
									}
#else
#define _foMPI_ALIGNED_ALLOC(addrptrptr, size)   posix_memalign( ( addrptrptr ) , _foMPI_MEMORY_ALIGNMENT , ( size) );
#endif
#define _foMPI_ALLOC(size) (malloc(size))
#define _foMPI_FREE(ptr) 	(free(ptr))
#else
#ifdef UGNI
#define _foMPI_ALIGNED_ALLOC(addrptrptr, size)   posix_memalign( ( addrptrptr ) , _foMPI_UGNI_MEMORY_ALIGNMENT , ( size) );
#else
#define _foMPI_ALIGNED_ALLOC(addrptrptr, size)   posix_memalign( ( addrptrptr ) , _foMPI_MEMORY_ALIGNMENT , ( size) );
#endif
#define _foMPI_ALLOC(size)  (malloc(size))
#define _foMPI_FREE(ptr) 	(free(ptr))
#endif

#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

/* logging macros */
#ifndef NDEBUG
#define _foMPI_TRACE_LOG(trace_level, fmt, args...) \
		if( glob_info.v_option >= trace_level) { \
			fprintf(stdout, "[Rank: %4i] (%s %d):", glob_info.debug_pe, __FILE__, __LINE__); \
			fprintf(stdout, fmt, ##args); \
		}

#define _foMPI_TRACE_LOG_FLUSH if ( glob_info.v_option > 0) fflush(stdout)

#else
#define _foMPI_TRACE_LOG(trace_level, fmt, args...)
#define _foMPI_TRACE_LOG_FLUSH
#endif

/*error macros*/
#define _foMPI_ERR_LOG(fmt, args...) fprintf(stderr, "[Rank: %4i] (%s %d):", glob_info.debug_pe, __FILE__, __LINE__); \
			fprintf(stderr, fmt, ##args);

#endif /* FOMPI_INTERNAL_EX_H_ */
