/*
 * fompi_notif_uq.c
 *
 *  Created on: Jul 14, 2014
 *      Author: Roberto Belli
 */
#include <stdio.h>

#include "fompi.h"

#ifdef UGNI
#include "fompi_notif_uq.h"

static inline int _uq_remove_tail(fompi_notif_uq_t *uq, fompi_notif_node_t** node);
static inline void _uq_insert_head(fompi_notif_uq_t *uq, fompi_notif_node_t* node);
static inline void _uq_node_detach(fompi_notif_uq_t *uq, fompi_notif_node_t* node);
static inline fompi_notif_node_t* _uq_node_create(uint16_t source, uint16_t tag);
static inline void _uq_node_free(fompi_notif_node_t* node);
static inline int _fompi_notif_uq_search_remove_internal(fompi_notif_uq_t *uq, uint16_t source_in , uint16_t *source_out ,int wildcard_source, uint16_t tag_in, uint16_t *tag_out, int wildcard_tag);


int _fompi_notif_uq_isEmpty(fompi_notif_uq_t *uq){
	if(uq->size==0)
		return _foMPI_TRUE;
	return _foMPI_FALSE;
}


fompi_notif_uq_t* _fompi_notif_uq_init() {
	fompi_notif_uq_t *uq = _foMPI_ALLOC(sizeof(fompi_notif_uq_t));
	uq->head = NULL;
	uq->tail = NULL;
	uq->size = 0;
	return uq;
}

void _fompi_notif_uq_finalize(fompi_notif_uq_t **uq) {
	fompi_notif_node_t *node;
	int res = _uq_remove_tail(*uq, &node);
	while (res != _foMPI_NO_MATCH) {
		_uq_node_free(node);
		res = _uq_remove_tail(*uq, &node);
	}
	*uq = NULL;
	return;
}

int _fompi_notif_uq_push(fompi_notif_uq_t *uq, uint16_t source, uint16_t tag) {

	fompi_notif_node_t *node = _uq_node_create(source, tag);
	_uq_insert_head(uq, node);
	return MPI_SUCCESS;
}

/**/
int _fompi_notif_uq_pop(fompi_notif_uq_t *uq, uint16_t *source, uint16_t *tag) {
	fompi_notif_node_t *node;
	if (_uq_remove_tail(uq, &node) != _foMPI_NO_MATCH) {
		*source = node->source;
		*tag = node->tag;
		_uq_node_free(node);
		return MPI_SUCCESS;
	}

	return _foMPI_NO_MATCH;
}

int _fompi_notif_uq_search_remove(fompi_notif_uq_t *uq, uint16_t source,uint16_t tag){
	uint16_t src = 0, tg= 0; /*dummy values*/
	return _fompi_notif_uq_search_remove_internal(uq, source, &src,0 , tag, &tg, 0);
}

int _fompi_notif_uq_search_remove_tag(fompi_notif_uq_t *uq, uint16_t tag, uint16_t *source) {
	uint16_t src = 0, tg= 0; /*dummy values*/
	return _fompi_notif_uq_search_remove_internal(uq, src , source, 1 , tag, &tg, 0);
}

int _fompi_notif_uq_search_remove_src(fompi_notif_uq_t *uq, uint16_t source, uint16_t *tag) {
	uint16_t src = 0, tg= 0; /*dummy values*/
	return _fompi_notif_uq_search_remove_internal(uq, source , &src, 0 , tg, tag, 1);
}


static inline int _fompi_notif_uq_search_remove_internal(fompi_notif_uq_t *uq, uint16_t source_in , uint16_t *source_out ,int wildcard_source, uint16_t tag_in, uint16_t *tag_out, int wildcard_tag) {
	if (uq->size == 0) {
		return _foMPI_NO_MATCH;
	}
	fompi_notif_node_t *node;
	node = uq->tail;
	while (node != NULL) {
		if ((wildcard_source || node->source == source_in) && (wildcard_tag || node->tag == tag_in)) {
			/*notification found */
			_uq_node_detach(uq, node);
			*source_out = node->source;
			*tag_out = node->tag;
			_uq_node_free(node);
			return MPI_SUCCESS;
		}
		node=node->prev;
	}
	return _foMPI_NO_MATCH;

}

static inline int _uq_remove_tail(fompi_notif_uq_t *uq, fompi_notif_node_t** node) {
	if (uq->size > 0) {
		(*node) = uq->tail;
		if((*node)->prev !=NULL){
			(*node)->prev->next = NULL;
		} else {
			uq->head = NULL;
		}
		uq->tail = (*node)->prev;
		uq->size--;
		return MPI_SUCCESS;
	}
	return _foMPI_NO_MATCH;
}

static inline void _uq_insert_head(fompi_notif_uq_t *uq, fompi_notif_node_t* node) {
	node->next = uq->head;
	if(uq->head != NULL){
		uq->head->prev = node;
	} else {
		uq->tail = node;
	}
	uq->head = node;
	node->prev = NULL;
	uq->size++;
}

static inline void _uq_node_detach(fompi_notif_uq_t *uq, fompi_notif_node_t* node) {

	if (node != uq->tail && node != uq->head) {
		/*the node is in th middle of the list*/
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}
	if (node == uq->tail && node == uq->head) {
		/*the node is the only element on the list*/
		uq->tail = NULL;
		uq->head = NULL;
	}
	if (node == uq->tail) {
		/*the node is the last element on the list*/
		uq->tail = node->prev;
		node->prev->next = NULL;
	}
	if (node == uq->head) {
		/*the node is the first element on the list*/
		uq->head = node->next;
		node->next->prev = NULL;
	}
	uq->size--;

}

static inline fompi_notif_node_t* _uq_node_create(uint16_t source, uint16_t tag) {
	fompi_notif_node_t* node = _foMPI_ALLOC(sizeof(fompi_notif_node_t));
	node->source = source;
	node->tag = tag;
	return node;
}

static inline void _uq_node_free(fompi_notif_node_t* node) {
	_foMPI_FREE(node);
}

#endif
