/*
 * gwdlib.h
 *
 *  Created on: 2010-11-2
 *      Author: lilei
 */

#ifndef __GWDLIB_H
#define __GWDLIB_H

#include <gwtypes.h>

typedef void* gw_dlib_t;

typedef enum __tag_gw_dlib_err
{
	GW_DLIB_ERR_NONE = 0
} gw_dlib_err_t;

gw_dlib_t gw_dlib_open(const char_t *path);

void * gw_dlib_sym(gw_dlib_t handle, const char_t *sym);

bool_t gw_dlib_close (gw_dlib_t handle);

const char_t * gw_dlib_error(void);

#endif /* GWDLIB_H_ */
