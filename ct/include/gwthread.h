/*
 * gwthread.h
 *
 *  Created on: 2010-11-1
 *      Author: lilei
 */

#ifndef __GWTHREAD_H
#define __GWTHREAD_H

#include <gwtypes.h>

typedef void* gw_thread_t;

typedef enum __tag_gw_thread_err
{
	GW_THREAD_ERR_NONE = 0
} gw_thread_err_t;

gw_thread_t gw_thread_create(void *(* routine)(void *), void *arg);

//
void gw_thread_exit(uint32_t rtval);

int32_t gw_thread_wait(gw_thread_t handle);

gw_thread_t gw_thread_self(void);

bool_t gw_thread_detach(gw_thread_t handle);

bool_t gw_thread_close(gw_thread_t handle);

#endif /* GWTHREAD_H_ */
