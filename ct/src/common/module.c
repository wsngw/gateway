/**
 * @brief module management routines
 *
 * @file
 * @author makui (2010-11-1)
 *
 * @addtogroup GW_MODULES Gateway Module Management
 * @{
 */
#include <stdlib.h>

#include <module.h>
#include <gwdlib.h>
#include <gwmutex.h>
#include <log.h>
#include <gw.h>

static gw_mod_head_t modules;
static gw_svc_head_t services;

/**
 * @brief Initialize gateway module system.
 *
 * Call this function before loading any module.
 *
 * @param gw pointer to gateway context
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t mod_sys_init(gw_t *gw)
{
	bool_t ret = FALSE;

	modules.lock = gw_mutex_create();
	ret = (modules.lock != NULL);

	if (ret)
	{
		services.lock = gw_mutex_create();
		ret = (services.lock != NULL);
		if (!ret)
		{
			gw_mutex_destroy(modules.lock);
		}
	}

	if (ret)
	{
		services.chains = NULL;
		gw->modules = &modules;
		gw->services = &services;
		INIT_LIST_HEAD(&gw->modules->lhead);
	}

	return ret;
}

/**
 * @brief Destroy gateway module system.
 *
 * Be sure that all modules are unloaded before calling
 * this function.
 *
 * @param gw pointer to gateway context
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t mod_sys_destroy(gw_t *gw)
{
	bool_t ret = TRUE;

	if (modules.lock)
	{
		gw_mutex_destroy(modules.lock);
	}

	if (services.lock)
	{
		gw_mutex_destroy(services.lock);
	}

	gw->modules = NULL;
	gw->services = NULL;

	return ret;
}

/**
 * @brief Load a gateway module from disk.
 *
 * @param gw pointer to gateway context
 * @param path module file path
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t mod_load(gw_t *gw, const char_t *path)
{
	bool_t ret = FALSE;
	gw_dlib_t lib = NULL;

	if ((lib = gw_dlib_open(path)) != NULL)
	{
		ret = mod_call_init(gw, lib);
		if (!ret)
		{
			gw_dlib_close(lib);
		}
	}
	else
		gw_debug_print("No such .so file\n");

	return ret;
}

/**
 * @brief Unload a gateway module.
 *
 * @param gw pointer to gateway context
 * @param mod pointer to module data structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t mod_free(gw_t *gw, gw_mod_t *mod)
{
	bool_t ret = FALSE;

	if (gw && mod && mod->lib)
	{
		mod_call_cleanup(gw, mod);
		gw_log_print(GW_LOG_LEVEL_INFO, "module %s unloaded\n", mod->name);
		gw_dlib_close(mod->lib);
		ret = TRUE;
	}

	return ret;
}

bool_t mod_call_init(gw_t *gw, gw_dlib_t lib)
{
	bool_t ret = FALSE;
	mod_init_func init = NULL;

	init = (mod_init_func)gw_dlib_sym(lib, "init");
	if (init)
	{
		ret = (*init)(gw, lib);
	}

	return ret;
}

bool_t mod_call_cleanup(gw_t *gw, gw_mod_t *mod)
{
	bool_t ret = FALSE;

	if (mod && mod->cleanup)
	{
		ret = (*(mod->cleanup))(gw);
	}

	return ret;
}

//bool_t mod_call_serialize(gw_t *gw, gw_mod_t *mod, bstring *buf)
json_object *mod_call_serialize(gw_t *gw, gw_mod_t *mod)
{
	//bool_t ret = FALSE;
	json_object *output;
	if (mod && mod->serialize)
	{
		//ret = (*(mod->serialize))(gw, buf);
		output = (*(mod->serialize))(gw);
	}

	return output;
}

/**
 * @brief Register a gateway module.
 *
 * Module call this function to register itself to
 * module management system.
 *
 * @param gw pointer to gateway context
 * @param mod pointer to module data structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t mod_register(gw_t *gw, gw_mod_t *mod)
{
	bool_t ret = FALSE;
	struct list_head *pos;
	gw_mod_t *curr_mod;

	/* validate module info */
	ret = mod && mod->cleanup && mod->serialize &&
		(mod->sysver.major == MODSYS_VER_MAJOR) &&
		(mod->sysver.minor == MODSYS_VER_MINOR);

	if (ret)
	{
		gw_mutex_lock(gw->modules->lock);

		/* check for duplication */
		list_for_each(pos, &gw->modules->lhead)
		{
			curr_mod = list_entry(pos, gw_mod_t, lhead);
			if (curr_mod == mod)
			{
				ret = FALSE;
				break;
			}
		}

		/* register module */
		if (ret)
		{
			list_add(&mod->lhead, &gw->modules->lhead);
			ret = TRUE;
		}

		gw_mutex_unlock(gw->modules->lock);
	}

	if (ret)
	{
		gw_log_print(GW_LOG_LEVEL_INFO, "module %s registered\n", mod->name);
	}

	return ret;
}

/**
 * @brief Unregister a gateway module.
 *
 * Module call this function to unregister itself
 * from gateway module management system.
 *
 * @param gw pointer to gateway context
 * @param mod pointer to module data structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t mod_unregister(gw_t *gw, gw_mod_t *mod)
{
	bool_t ret = FALSE;

	if (mod)
	{
		list_del(&mod->lhead);
		ret = TRUE;
	}

	return ret;
}

/**
 * @brief Get the first gateway module.
 *
 * This function is used to iterate registered gateway modules.
 *
 * @param gw pointer to gateway context
 *
 * @return the first module pointer on success, NULL on failure
 */
gw_mod_t *mod_get_first(gw_t *gw)
{
	gw_mod_t *mod = NULL;

	if (gw->modules->lhead.next != &gw->modules->lhead)
	{
		mod = list_entry(gw->modules->lhead.next, gw_mod_t, lhead);
	}

	return mod;
}

/**
 * @brief Get next gateway module.
 *
 * This function is called after mod_get_first.
 *
 * @param gw pointer to gateway context
 * @param curr pointer to current gateway module, if NULL, the first module will be returned
 *
 * @return the next module pointer on success, NULL on failure
 */
gw_mod_t *mod_get_next(gw_t *gw, gw_mod_t *curr)
{
	gw_mod_t *mod = NULL;

	if (curr == NULL)
	{
		mod = mod_get_first(gw);
	}
	else if (curr->lhead.next != &gw->modules->lhead)
	{
		mod = list_entry(curr->lhead.next, gw_mod_t, lhead);
	}

	return mod;
}

/**
 * @brief Get next gateway module.
 *
 * This function is called after mod_get_first.
 *
 * @param gw pointer to gateway context
 * @param curr pointer to current gateway module, if NULL, the first module will be returned
 *
 * @return the next module pointer on success, NULL on failure
 */
gw_mod_t *mod_get_next_safe(gw_t *gw, gw_mod_t *curr, gw_mod_t **next)
{
	gw_mod_t *mod = NULL;

	if (curr == NULL)
	{
		mod = mod_get_first(gw);
	}
	else
	{
		mod = curr;
	}

	if (mod && (mod->lhead.next != &gw->modules->lhead))
	{
		*next = list_entry(mod->lhead.next, gw_mod_t, lhead);
	}
	else
	{
		*next = NULL;
	}

	return mod;
}

static gw_svc_chain_t * mod_service_chain_alloc(uint32_t id)
{
	gw_svc_chain_t *chain = calloc(1, sizeof(gw_svc_chain_t));
	INIT_LIST_HEAD(&chain->lhead);
	chain->id = id;
	chain->num = 0;
	chain->lock = gw_mutex_create();

	return chain;
}

static void mod_service_chain_free(gw_svc_chain_t *chain)
{
	if (chain)
	{
		gw_mutex_lock(chain->lock);

		if (chain->num != 0)
			gw_log_print(GW_LOG_LEVEL_WARNING, "Service chain entry count %d is not zero\n", chain->num);

		gw_mutex_unlock(chain->lock);
		gw_mutex_destroy(chain->lock);
		free(chain);
	}
}

/**
 * @brief Register a module service.
 *
 * Module call this function to register a module service to gateway
 * service repository.
 *
 * @param gw pointer to gateway context
 * @param mod pointer to current gateway module
 * @param svc pointer to service to be registered
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t mod_register_service(gw_t *gw, gw_mod_t *mod, gw_svc_t *svc)
{
	bool_t ret = FALSE;
	gw_svc_chain_t *chain = NULL;

	/* alloc service chain if not exist */
	HASH_FIND_INT(gw->services->chains, &svc->id, chain);
	if (!chain)
	{
		chain = mod_service_chain_alloc(svc->id);
		if (chain)
		{
			HASH_ADD_INT(gw->services->chains, id, chain);
		}
	}

	/* add serivce to chain */
	if (chain)
	{
		/* add service to service chain [id] */
		gw_mutex_lock(chain->lock);
		list_add(&svc->chain, &chain->lhead);
		chain->num++;
		gw_mutex_unlock(chain->lock);

		/* add service to module's service list */
		list_add(&svc->lhead, &mod->services);

		ret = TRUE;
		gw_log_print(GW_LOG_LEVEL_INFO, "service %u registered by module %s\n", svc->id, mod->name);
	}

	return ret;
}

/**
 * @brief Unregister a module service.
 *
 * Module call this function to unregister a module service from gateway
 * service repository.
 *
 * @param gw pointer to gateway context
 * @param mod pointer to current gateway module
 * @param svc pointer to service to be unregistered
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t mod_unregister_service(gw_t *gw, gw_mod_t *mod, gw_svc_t *svc)
{
	bool_t ret = FALSE;
	gw_svc_chain_t *chain = NULL;

	if (gw && mod && svc)
	{
		/* remove service from service chain */
		HASH_FIND_INT(gw->services->chains, &svc->id, chain);
		if (chain)
		{
			/* remove service from chain[id] */
			gw_mutex_lock(chain->lock);
			chain->num--;
			list_del(&svc->chain);
			gw_mutex_unlock(chain->lock);

			/* remove and free chain[id] from gw service chain hash table if chain is empty */
			if (chain->num == 0)
			{
				gw_mutex_lock(gw->services->lock);
				HASH_DEL(gw->services->chains, chain);
				gw_mutex_unlock(gw->services->lock);
				mod_service_chain_free(chain);
			}
		}

		/* remove service from module's service list */
		list_del(&svc->lhead);
	}

	return ret;
}

/**
 * @brief Get the first service registered by a module.
 *
 * This function is used to iterate registered services by a
 * given module.
 *
 * @param gw pointer to gateway context
 * @param mod pointer to current gateway module
 *
 * @return pointer to module service on success, NULL on failure
 */
gw_svc_t *mod_get_mod_service_first(gw_t *gw, gw_mod_t *mod)
{
	gw_svc_t *svc = NULL;

	if (!(bool_t)list_empty(&mod->services))
	{
		svc = list_entry(mod->services.next, gw_svc_t, lhead);
	}

	return svc;
}

/**
 * @brief Get next service registered by a module.
 *
 * This function is used to iterate registered services by a
 * given module.
 *
 * @param gw pointer to gateway context
 * @param mod pointer to current gateway module
 * @param curr pointer to current module service
 *
 * @return pointer to module service on success, NULL on failure
 */
gw_svc_t *mod_get_mod_service_next(gw_t *gw, gw_mod_t *mod, gw_svc_t *curr)
{
	gw_svc_t *svc = NULL;

	if (gw && mod && curr && curr->lhead.next != &mod->services)
	{
		svc = list_entry(curr->lhead.next, gw_svc_t, lhead);
	}

	return svc;
}

/**
 * @brief Get module service by service id.
 *
 * This function is used to find a registered services by
 * service id.
 *
 * @param gw pointer to gateway context
 * @param id service id
 *
 * @return pointer to module service on success, NULL on failure
 */
gw_svc_t *mod_get_service_by_id(gw_t *gw, uint32_t id)
{
	gw_svc_t *svc = NULL;
	gw_svc_chain_t *chain = NULL;

	HASH_FIND_INT(gw->services->chains, &id, chain);
	if (chain)
	{
		svc = list_entry(chain->lhead.next, gw_svc_t, chain);
	}
//	printf("svc basic information is name %s, id %d\n", svc->name, svc->id);
	return svc;
}

void mod_test(gw_t *gw, const char_t *path)
{
	bool_t ret = FALSE;
	gw_mod_t *mod = NULL;

	ret = mod_load(gw, path);
	mod = mod_get_first(gw);
	ret = mod_free(gw, mod);
}

/**
 * @}
 */
