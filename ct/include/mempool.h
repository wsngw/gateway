/*
 * mempool.h
 *
 *  Created on: 2010-11-2
 *      Author: lilei
 */

#ifndef __MEMPOOL_H
#define __MEMPOOL_H

#include <gwtypes.h>
#include <gwmutex.h>

typedef enum __tag_gw_mp_err
{
	GW_MP_ERR_NONE = 0
} gw_mp_err_t;

typedef struct __tag_mp_head_t
{
	void *next;       //next valid memory module
	uint32_t num;   //the number of total memory module
	uint32_t size;    //size of each memory module
	uint32_t avil;     //the number of total memory module
	gw_mutex_t mutex;  //a mutex for the mem pool
}mp_head_t, *mp_head_pt;

mp_head_pt gw_mem_pool_create(uint32_t num, uint32_t size);

bool_t gw_mem_pool_destroy(mp_head_pt head);

void *gw_mem_pool_alloc(mp_head_pt head);

bool_t gw_mem_pool_free(mp_head_pt head, void *block);

#endif /* MEMPOOL_H_ */
