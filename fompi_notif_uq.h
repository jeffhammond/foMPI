/*
 * fompi_notif_uq.h
 *
 *  Created on: Jul 14, 2014
 *      Author: Roberto Belli
 *
 *      Interface that defines all the operation of the discarded notification queue.
 */

#ifndef FOMPI_NOTIF_UQ_H_
#define FOMPI_NOTIF_UQ_H_

#ifdef UGNI
typedef struct fompi_notif_uq fompi_notif_uq_t;
typedef struct fompi_notif_node fompi_notif_node_t;

typedef struct fompi_notif_node {
	fompi_notif_node_t *next, *prev;
	uint16_t tag;
	uint16_t source;
} fompi_notif_node_t;


struct fompi_notif_uq {
	fompi_notif_node_t *head, *tail;
	unsigned int size;
};

/*creates a new ordered set*/
fompi_notif_uq_t* _fompi_notif_uq_init() ;

/**/
void _fompi_notif_uq_finalize(fompi_notif_uq_t **uq) ;


int _fompi_notif_uq_isEmpty(fompi_notif_uq_t *uq);

/*inserts a new notification in the fompi_uq*/
int _fompi_notif_uq_push(fompi_notif_uq_t *uq, uint16_t source, uint16_t tag);

/*returns _foMPI_NO_MATCH if the uq is empty, otherwise MPI_SUCCESS. In the latter case updates variables source and tag
 * with the actual values of the found notification (that is the oldest)*/
int _fompi_notif_uq_pop(fompi_notif_uq_t *uq, uint16_t *source, uint16_t *tag);

/*
 * returns MPI_SUCCESS if founds a notification that match the requested tag and source. Removes from the uq the oldest
 * notification that matches. Otherwise returns _foMPI_NO_MATCH
 * */
int _fompi_notif_uq_search_remove(fompi_notif_uq_t *uq, uint16_t source,uint16_t tag);

/*
 * returns MPI_SUCCESS if founds a notification that match the requested tag.
 * Updates the variable source with the rank of the sender of the oldest notification that match
 * and removes that notification from the uq.
 * Otherwise returns _foMPI_NO_MATCH
 * */
int _fompi_notif_uq_search_remove_tag(fompi_notif_uq_t *uq, uint16_t tag, uint16_t *source);

/*
 * returns MPI_SUCCESS if founds a notification that match the requested source.
 * Updates the variable tag with the tag specified by the oldest notification sent by source
 * and removes that notification from the uq.
 * Otherwise returns _foMPI_NO_MATCH
 * */
int _fompi_notif_uq_search_remove_src(fompi_notif_uq_t *uq, uint16_t source, uint16_t *tag) ;
#endif

#endif /* FOMPI_NOTIF_UQ_H_ */
