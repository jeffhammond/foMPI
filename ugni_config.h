/*
 * config.h
 *
 *  Created on: Jun 19, 2014
 *      Author: Roberto Belli
 */

#ifndef UGNI_CONFIG_H_
#define UGNI_CONFIG_H_

#define _foMPI_UGNI_MEMORY_ALIGNMENT 64
#define _foMPI_UGNI_GET_ALIGN 4 //sizeof(void*)

/*init configurations*/
#define _foMPI_PTAG_INDEX 		1
#define _foMPI_DEFAULT_DEV_ID 	0

#define _foMPI_BIND_ID_MULTIPLIER       100

/*completion queue configurations*/
#define _foMPI_NUM_SRC_CQ_ENTRIES 	1000
#define _foMPI_NUM_DST_CQ_ENTRIES 	10000
#define _foMPI_WAIT_EVENT_TIMEOUT 	1000
#define _foMPI_SRC_CQ_MODE 			GNI_CQ_BLOCKING
#define _foMPI_DST_CQ_MODE  		GNI_CQ_NOBLOCK

/*notification system*/
//#define _FOMPI_REORDER_NOTIFICATIONS
//#define _FOMPI_NOTIFICATION_SOFTWARE_AGENT //not implemented yet


/*RDMA configurations*/
/*TODO: you should try what method is faster */
/*communication domain conf*/
#define _foMPI_DEFAULT_RDMA_MODES		( GNI_CDM_MODE_DUAL_EVENTS )
//#define _foMPI_DEFAULT_RDMA_MODES 	GNI_CDM_MODE_BTE_SINGLE_CHANNEL

/*RDMA operations conf*/
#define _foMPI_PUT_RDMA_MODE 	 0   /*GNI_RDMAMODE_PHYS_ADDR  or GNI_RDMAMODE_FENCE*/
#define _foMPI_RDMA_THRESHOLD 	( 2048 )
#define _foMPI_MAX_RDMA_TRANFER_LENGHT ( 4294967295 ) /*2^32-1*/

//#define foMPI_UGNI_WIN_RELATED_SRC_CQ


#endif /* CONFIG_H_ */
