/*
 * gwmutex.h
 *
 *  Created on: 2010-11-2
 *      Author: lilei
 */

#ifndef __GWMUTEX_H
#define __GWMUTEX_H

#include <gwtypes.h>

typedef void* gw_mutex_t;

typedef enum __tag_gw_mutex_err
{
	GW_MUTEX_ERR_NONE = 0
} gw_mutex_err_t;

gw_mutex_t gw_mutex_create(void);

bool_t gw_mutex_destroy(gw_mutex_t mutex);

bool_t gw_mutex_lock(gw_mutex_t mutex);

bool_t gw_mutex_unlock(gw_mutex_t mutex);

#endif /* GWMUTEX_H_ */
