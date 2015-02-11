/*
 * fompi_seg.c
 *
 *  Created on: Jul 3, 2014
 *      Author: Roberto Belli
 */
#include "fompi.h"

/*
 * These functions permit to handle only here the functions of
 *  registering and unregistering of memory segments on the NIC
 *
 * */


void _foMPI_mem_register(void * addr, uint64_t size, fompi_seg_desc_t *seg, foMPI_Win win) {
	seg->addr = addr;
	seg->size = size;
	dmapp_return_t status = dmapp_mem_register(seg->addr, seg->size, &(seg->dmapp));
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
#ifdef UGNI
	 /*
		  * Register the memory associated for the send buffer with the NIC.
		  * We are sending the data from this buffer not receiving into it.
		  	 *     nic_handle is our NIC handle.
		  	 *     send_buffer is the memory location of the send buffer.
		  	 *     TRANSFER_LENGTH_IN_BYTES is the size of the memory allocated to the
		  	 *         send buffer.
		  	 *     NULL means that no completion queue handle is specified.
		  	 *     GNI_MEM_READWRITE is the read/write attribute for the flag's
		  	 *         memory region.
		  	 *     -1 specifies the index within the allocated memory region,
		  	 *         a value of -1 means that the GNI library will determine this index.
		  	 *     source_memory_handle is the handle for this memory region.
		  	 */
#ifndef NDEBUG
	if(! is_aligned(seg->addr, _foMPI_MEMORY_ALIGNMENT) ){

			/*
		     * uGNI needs alignment? in all the example the memory is aligned.
		     */
		//_check_gni_status(GNI_RC_ALIGNMENT_ERROR , GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
		_foMPI_TRACE_LOG(1, "_foMPI_mem_register -> memory not aligned \n");
	}
#endif
	gni_return_t status_gni = GNI_MemRegister(win->fompi_comm->nic_handle,
			(uint64_t) seg->addr, seg->size,
			win->destination_cq_handle, GNI_MEM_READWRITE, -1, &(seg->ugni));
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
#endif
	_foMPI_TRACE_LOG(1, "_foMPI_mem_register -> success  range: [%p,%p] size: %"PRIu64" ugni_hndl:%"PRIu64"\n", seg->addr, seg->addr + seg->size , seg->size , seg->ugni.qword1);
}

void _foMPI_mem_unregister(fompi_seg_desc_t *seg, foMPI_Win win) {
	dmapp_return_t status = dmapp_mem_unregister(&(seg->dmapp));
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
#ifdef UGNI
	/*
	 * Deregister the memory associated for the buffer with the NIC.
	 *     nic_handle is our NIC handle.
	 *     receive_memory_handle is the handle for this memory region.
	 */
	gni_return_t status_gni = GNI_MemDeregister(win->fompi_comm->nic_handle, &(seg->ugni));
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
#endif
	_foMPI_TRACE_LOG(1, "_foMPI_mem_unregister[%p] -> success   \n", seg->addr);
}

inline int _foMPI_is_addr_in_seg(void * addr, fompi_seg_desc_t *seg ){
	if(addr >= seg->addr && addr <= (seg->addr + seg->size)){
		return _foMPI_TRUE;
	}
	return _foMPI_FALSE;
}
