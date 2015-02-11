/*
 * xpmem_config.h
 *
 *  Created on: Jun 19, 2014
 *      Author: Roberto Belli
 */

#ifndef XPMEM_CONFIG_H_
#define XPMEM_CONFIG_H_

/*init configurations*/

/*completion queue configurations*/

#define _foMPI_XPMEM_NUM_DST_CQ_ENTRIES 	1000
#define _foMPI_CACHE_LINE_IN_BYTES 64
#define _foMPI_PAGE_SIZE_IN_BYTES 4096
#define _foMPI_NOTIF_FIXED_FIELDS_SIZE ( sizeof(fompi_xpmem_notif_t) )
#define _foMPI_XPMEM_NOTIF_INLINE_BUFF_SIZE ( _foMPI_CACHE_LINE_IN_BYTES - _foMPI_NOTIF_FIXED_FIELDS_SIZE )

/*notification system*/


#endif /* XPMEM_CONFIG_H_ */
