/*
 * fompi_not_xpmem.c
 *
 *  Created on: 09/ott/2014
 *      Author: Roberto Belli
 */

#include "fompi.h"
/*
void printlock(fompi_xpmem_notif_queue_t *queue, lock_flags_t *flags_array){
	int i;
	printf("flagarray[ ");
	for( i=0; i<queue->flags_count; i++){
		printf("%"PRIu32" ",flags_array[i]);
	}
	printf("] \n tail: %d\n",queue->tail);
}
*/
inline void xpmem_notif_lock(fompi_xpmem_notif_queue_t *queue,lock_flags_t *flags_array,fompi_xpmem_notif_state_t *queue_state, int *slot, foMPI_Win win) {
	*slot = __sync_fetch_and_add(&(queue_state->tail), 1) ;
	*slot = *slot % win->onnode_size;
	while (!flags_array[*slot]) {
	};
	//printf("lock -> slot: %d ", *slot);
	//printlock(queue,flags_array);
}

inline void xpmem_notif_unlock(fompi_xpmem_notif_queue_t *queue,lock_flags_t *flags_array, int slot, foMPI_Win win) {
	flags_array[slot] = 0;
	flags_array[(slot + 1) % win->onnode_size] = 1;
	//printf("unlock -> slot: %d ", slot);
	//printlock(queue,flags_array);
}


void xpmem_notif_init_queue(foMPI_Win win, int onnode_size) {
	assert(win->onnode_size > 0);
	void * tempPtr;
	assert(sizeof(fompi_xpmem_notif_queue_t) == 64);
	posix_memalign( ( (void**) &tempPtr ) ,_foMPI_PAGE_SIZE_IN_BYTES,  sizeof(fompi_xpmem_notif_queue_t) );
	assert(tempPtr != NULL);
	win->xpmem_notif_queue = (fompi_xpmem_notif_queue_t*) tempPtr;
	posix_memalign( ( (void**) &tempPtr ) ,_foMPI_PAGE_SIZE_IN_BYTES,  sizeof(fompi_xpmem_notif_state_t) + win->onnode_size * sizeof(lock_flags_t) + _foMPI_CACHE_LINE_IN_BYTES );
  	win->xpmem_notif_state_lock = tempPtr;
  	//memset(win->xpmem_notif_state_lock, 0, sizeof(fompi_xpmem_notif_state_t) + sizeof(lock_flags_t) * onnode_size + _foMPI_CACHE_LINE_IN_BYTES);
  	fompi_xpmem_notif_state_t *queue_state = (fompi_xpmem_notif_state_t *) win->xpmem_notif_state_lock;
  	queue_state->tail=0;
  	queue_state->count=0;
  	queue_state->inser_ind=0;
  	lock_flags_t *lock_arr = (lock_flags_t *) (win->xpmem_notif_state_lock + sizeof(fompi_xpmem_notif_state_t));
  	int l;
  	for( l=0;l<win->onnode_size;l++){
  		lock_arr[l]=0;
  	}
  	lock_arr[0] = 1;
  	queue_state->inser_ind = 0;
	win->xpmem_queue_extract_ind = 0;
	queue_state->count = 0;
	//win->xpmem_notif_queue->size = _foMPI_XPMEM_NOTIF_INLINE_BUFF_SIZE;
	queue_state->tail = 0;
}

void xpmem_notif_free_queue(foMPI_Win win) {
	_foMPI_FREE((void*)(win->xpmem_notif_queue));
	_foMPI_FREE(win->xpmem_notif_state_lock);
}

inline int xpmem_notif_push_and_data(int source_rank, MPI_Aint target_offset, size_t size, int target_rank, int target_local_rank, MPI_Aint origin_offset, int tag, foMPI_Win win){
#ifndef NDEBUG
	assert(size<=_foMPI_XPMEM_NOTIF_INLINE_BUFF_SIZE);
#endif
	uint16_t myrank = source_rank;
	uint16_t sent_tag = tag;
		int slot;
		/*if (local_rank == -1) {
			//not local node ERROR
			return -1; //TODO: real errorcode needed
		}*/
		fompi_xpmem_notif_queue_t *queue = (fompi_xpmem_notif_queue_t*) win->xpmem_array[target_local_rank].notif_queue;
		fompi_xpmem_notif_state_t *queue_state = (fompi_xpmem_notif_state_t *) win->xpmem_array[target_local_rank].notif_queue_state;
		lock_flags_t *flags_array = (lock_flags_t *) (((void*)win->xpmem_array[target_local_rank].notif_queue_state) + sizeof(fompi_xpmem_notif_state_t)) ;
		int inserted=0;
		while(!inserted){
		if (queue_state->count < _foMPI_XPMEM_NUM_DST_CQ_ENTRIES) {
		xpmem_notif_lock(queue,flags_array,queue_state, &slot, win);
			queue->queue_array[queue_state->inser_ind].n.tag = sent_tag;
			queue->queue_array[queue_state->inser_ind].n.source = myrank;
			queue->queue_array[queue_state->inser_ind].n.size = size;
			queue->queue_array[queue_state->inser_ind].n.target_addr = target_offset;
			if(size!=0){
			sse_memcpy((char*) &(queue->queue_array[queue_state->inser_ind].buff[0]), (char*) origin_offset, size);
			//memcpy((char*) &(queue->queue_array[queue_state->inser_ind].buff[0]), (char*) origin_offset, size);
			//__sync_synchronize(); Not needed, writes in the same cache line
			}
			queue_state->inser_ind = (queue_state->inser_ind + 1) % _foMPI_XPMEM_NUM_DST_CQ_ENTRIES;
			__sync_fetch_and_add(&(queue_state->count), 1);
			inserted=1;
		xpmem_notif_unlock(queue,flags_array, slot, win);
		}
		}
		return 0;
}

inline int xpmem_notif_push( int16_t myrank, int16_t tag, int target_rank, int target_local_rank, foMPI_Win win) {
//	int source_rank, void* source_addr, int size, int target_rank, uint64_t target_addr, int tag, foMPI_Win win){
	//find target rank
	int slot;
	/*if (local_rank == -1) {
		//not local node ERROR
		return -1; //TODO: real errorcode needed
	}*/
	fompi_xpmem_notif_queue_t *queue = (fompi_xpmem_notif_queue_t*) win->xpmem_array[target_local_rank].notif_queue;
	fompi_xpmem_notif_state_t *queue_state = (fompi_xpmem_notif_state_t *) win->xpmem_array[target_local_rank].notif_queue_state;
	lock_flags_t *flags_array = (lock_flags_t *) (((void*)win->xpmem_array[target_local_rank].notif_queue_state) + sizeof(fompi_xpmem_notif_state_t)) ;

	int inserted=0;
	while(!inserted){
	if (queue_state->count < _foMPI_XPMEM_NUM_DST_CQ_ENTRIES) {
		xpmem_notif_lock(queue,flags_array,queue_state, &slot, win);
	/*insert_ind is volatile, this caching should speedup this phase*/
	uint32_t ind = queue_state->inser_ind;
		queue->queue_array[ind].n.tag = tag;
		queue->queue_array[ind].n.source = myrank;
		queue->queue_array[ind].n.size = 0;
		queue_state->inser_ind = (ind + 1) % _foMPI_XPMEM_NUM_DST_CQ_ENTRIES;
		__sync_fetch_and_add(&(queue_state->count), 1);
		inserted=1;
	xpmem_notif_unlock(queue,flags_array, slot, win);
	}
	}
	return 0;
}

inline int xpmem_notif_pop(int16_t *rank, int16_t *tag, foMPI_Win win) {
	fompi_xpmem_notif_state_t *queue_state = (fompi_xpmem_notif_state_t *) win->xpmem_notif_state_lock;
	if (queue_state->count == 0){
		return -1;
	}
	fompi_xpmem_notif_queue_t *queue = win->xpmem_notif_queue;
	lock_flags_t *flags_array = (lock_flags_t *) (win->xpmem_notif_state_lock + sizeof(fompi_xpmem_notif_state_t));
	uint32_t id_encoded = 0;
	//int slot;
	//xpmem_notif_lock(queue,flags_array, &slot);
	*rank = queue->queue_array[win->xpmem_queue_extract_ind].n.source;
	*tag = queue->queue_array[win->xpmem_queue_extract_ind].n.tag;
	size_t size = queue->queue_array[win->xpmem_queue_extract_ind].n.size;
	size_t target_addr = queue->queue_array[win->xpmem_queue_extract_ind].n.target_addr;
	if(size > 0){
#ifndef NDEBUG
		assert(size < _foMPI_XPMEM_NOTIF_INLINE_BUFF_SIZE);
#endif
		//memcpy((char*) target_addr ,(char*) &(queue->queue_array[queue_state->inser_ind].buff[0]), size);
		sse_memcpy((char*) target_addr ,(char*) &(queue->queue_array[queue_state->inser_ind].buff[0]), size);
	}
	win->xpmem_queue_extract_ind = (win->xpmem_queue_extract_ind + 1) % _foMPI_XPMEM_NUM_DST_CQ_ENTRIES;
	__sync_fetch_and_add(&(queue_state->count), -1);
	//xpmem_notif_unlock(queue,flags_array, slot);
	return 0;
}

