/*
 * gwsem.h
 *
 *  Created on: 2010-11-2
 *      Author: lilei
 */

#ifndef __GWSEM_H
#define __GWSEM_H

#include <gwtypes.h>

typedef void* gw_sem_t;

typedef enum __tag_gw_sem_err
{
	GW_SEM_ERR_NONE = 0
} gw_sem_err_t;

gw_sem_t gw_sem_create(uint32_t value);

bool_t gw_sem_destroy(gw_sem_t sem);

bool_t gw_sem_wait(gw_sem_t sem);

bool_t gw_sem_post(gw_sem_t sem);


#endif /* GWSEM_H_ */
