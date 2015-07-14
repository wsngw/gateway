/*
 * module.h
 *
 *  Created on: 2010-11-1
 *      Author: makui
 *   Copyright: 2010, WSN
 */

#ifndef __MODULE_H
#define __MODULE_H

#include <msg.h>
#include <json/json.h>

#define MODSYS_VER_MAJOR	1
#define MODSYS_VER_MINOR	0

typedef bool_t (*mod_init_func)(gw_t *gw, gw_dlib_t lib);
typedef bool_t (*mod_cleanup_func)(gw_t *gw);
//typedef bool_t (*mod_serialize_func)(gw_t *gw, bstring *buf);
typedef json_object *(*mod_serialize_func)(gw_t *gw);

typedef struct __tag_gw_module
{
	/* list header*/
	struct list_head	lhead;

	/* basic info */
	char_t 				*name;
	char_t				*vendor;
	char_t				*desc;

	struct
	{
		uint16_t		major;
		uint16_t		minor;
	} 					modver, sysver;

	/* module data */
	void				*data;
	gw_handle_t			lib;
	struct list_head	services;

	/* module functions */
	mod_init_func		init;
	mod_cleanup_func	cleanup;
	mod_serialize_func	serialize;

	gw_mutex_t			lock;
} gw_mod_t;

/* module service structures */
typedef struct __tag_gw_service
{
	struct list_head	lhead;
	struct list_head	chain;
	uint32_t			id;
	char_t				*name;
	gw_mod_t			*mod;
	gw_msgq_t			*mq;
} gw_svc_t;

/* module private data */
typedef struct __tag_gw_mod_data
{
	void	*config;
	void	*stats;
} gw_mod_data_t;


/* module system init/destory */
bool_t mod_sys_init(gw_t *gw);
bool_t mod_sys_destroy(gw_t *gw);

/* module functions */
bool_t mod_load(gw_t *gw, const char_t *path);
bool_t mod_free(gw_t *gw, gw_mod_t *mod);
bool_t mod_call_init(gw_t *gw, gw_dlib_t lib);
bool_t mod_call_cleanup(gw_t *gw, gw_mod_t *mod);
//bool_t mod_call_serialize(gw_t *gw, gw_mod_t *mod, bstring *buf);
json_object *mod_call_serialize(gw_t *gw, gw_mod_t *mod);

bool_t mod_register(gw_t *gw, gw_mod_t *mod);
bool_t mod_unregister(gw_t *gw, gw_mod_t *mod);
gw_mod_t *mod_get_first(gw_t *gw);
gw_mod_t *mod_get_next(gw_t *gw, gw_mod_t *curr);
gw_mod_t *mod_get_next_safe(gw_t *gw, gw_mod_t *curr, gw_mod_t **next);

bool_t mod_register_service(gw_t *gw, gw_mod_t *mod, gw_svc_t *svc);
bool_t mod_unregister_service(gw_t *gw, gw_mod_t *mod, gw_svc_t *svc);
gw_svc_t *mod_get_mod_service_first(gw_t *gw, gw_mod_t *mod);
gw_svc_t *mod_get_mod_service_next(gw_t *gw, gw_mod_t *mod, gw_svc_t *curr);
gw_svc_t *mod_get_service_by_id(gw_t *gw, uint32_t id);

void mod_test(gw_t *gw, const char_t *path);

#endif /* __MODULE_H */
