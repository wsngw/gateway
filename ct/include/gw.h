/*
 * gw.h
 *
 *  Created on: 2010-11-5
 *      Author: makui
 *   Copyright: 2010, WSN
 */

#ifndef __GW_H
#define __GW_H
#include <time.h>
#include <msgdef.h>
#include <gwtypes.h>
#include <list.h>
#include <hash.h>
#include <bstrlib.h>
#include <gwmutex.h>
#include <gwthread.h>
#include <gwdlib.h>
#include <gwsem.h>
#include <infobase.h>
#include <log.h>

typedef struct
{
	struct list_head	lhead;
	UT_hash_handle		hh;
	uint32_t			id;
	uint32_t			num;
	gw_mutex_t			lock;
} gw_svc_chain_t;

/* module related definition */
typedef struct __tag_gw_mod_head
{
	struct list_head lhead;
	gw_mutex_t lock;
} gw_mod_head_t;

typedef struct __tag_gw_svc_head
{
	gw_svc_chain_t		*chains;
	gw_mutex_t			lock;
} gw_svc_head_t;


typedef struct __tag_gw
{
	gw_data_t *data;
	sn_head_t *sn;
	gw_config_t *config;

	gw_mod_head_t *modules;
	gw_svc_head_t *services;
} gw_t;

#endif /* __GW_H */
