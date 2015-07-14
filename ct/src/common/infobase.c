/**
 * @brief Gateway information Library Control Function
 *
 * @file
 * @author lilei (2010-11-15)
 *
 * @addtogroup GW_INFO_BASE Gateway Information Library Control Function
 * @{
 */

#include <infobase.h>
#include <msgdef.h>
#include <log.h>
#include <stdio.h>

/*--------------- Public info APIs  --------------*/

static gw_data_t gw_data;
static sn_head_t gw_sn_head;
static gw_config_t gw_config;

static int32_t reg_node_cnt(void)
{
	struct list_head *pos, *tmp;
	sn_data_t *tmp_sn_data;
	int32_t ret=0;
	list_for_each_safe(pos, tmp, &(gw_sn_head.sn_list))
	{
		tmp_sn_data = list_entry(pos, sn_data_t, sn_list);
		ret += tmp_sn_data->reg_node_num;
	}
	return ret;
}

/**
 * @brief Gateway information base initialization.
 *
 * This function only initializes the structures correlated to the gateway.
 *
 * @param NON
 *
 * @return NON
 */
void gw_infobase_init(void)
{
	memset(&gw_data, 0, sizeof(gw_data_t));
	gw_data.gw_state = STA_INIT;
	memset(&gw_config, 0, sizeof(gw_config_t));

	gw_config.daemon = TRUE;
	gw_config.nui = bfromcstr("12345678");
	gw_config.cfgpath = bfromcstr("/etc/gw/gw.conf");
	gw_config.logpath = bfromcstr("/var/log/gw.log");
	gw_config.pidpath = bfromcstr("/var/run/gw.pid");
	utarray_new(gw_config.module, &ut_str_icd);

	return;
}

/**
 * @brief Gateway information base cleanup.
 *
 * This function only clean the structures correlated to the gateway.
 *
 * @param NON
 *
 * @return NON
 */
void gw_infobase_destroy(void)
{
	//TODO: destroy infobase data structures

	bdestroy(gw_config.nui);
	bdestroy(gw_config.cfgpath);
	bdestroy(gw_config.logpath);
	bdestroy(gw_config.pidpath);
	utarray_free(gw_config.module);

	return;
}

/**
 * @brief Get the gateway base information.
 *
 * @param NON
 *
 * @return pointer to the gateway information data structure on success, NULL on failure
 */
gw_data_t * gw_data_get(void)
{
	return (&gw_data);
}

/**
 * @brief Get the gateway configuration information.
 *
 * @param NON
 *
 * @return pointer to the gateway configuration data structure on success, NULL on failure
 */
gw_config_t *gw_config_get(void)
{
	return &gw_config;
}

/**
 * @brief Device information base initialization.
 *
 * This function only initializes the structures correlated to the devices.
 *
 * @param NON
 *
 * @return NON
 */
void dev_infobase_init(void)
{
	/* initialize dev management info base head -- sn_data_head */
	INIT_LIST_HEAD(&(gw_sn_head.sn_list));
	gw_sn_head.sn_num = 0;
	gw_sn_head.mutex = gw_mutex_create();
	gw_sn_head.hash_head = NULL;
}

/**
 * @brief Device information base clean up.
 *
 * This function only clean up the structures correlated to the devices.
 *
 * @param NON
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t dev_infobase_destroy(void)
{
	bool_t ret = TRUE;
	sn_data_t *tmp_sn_data;
	struct list_head *pos, *tmp;
	/* free all the sn data and node data */
	list_for_each_safe(pos, tmp, &(gw_sn_head.sn_list))
	{
		tmp_sn_data = list_entry(pos, sn_data_t, sn_list);
		sn_data_del(tmp_sn_data);
		sn_data_free(tmp_sn_data);
		gw_sn_head.sn_num--;
	}
	ret = gw_mutex_destroy(gw_sn_head.mutex);
	return ret;
}

/**
 * @brief Output status information of the device infobase.
 *
 * @param flag	output option
 *
 * @return pointer to the serialized json string on success, NULL on failure
 */
json_object *dev_infobase_serialize(uint32_t flag)
{
	json_object *output;
	int32_t i;
	struct list_head *pos, *tmp, *npos, *ntmp;
	sn_data_t *tmp_sn_data;
	node_data_t *tmp_node_data;
	int8_t buff[100];
	output = json_object_new_object();
	json_object_object_add(output, "TOTAL_WSN_NUM", json_object_new_int(gw_sn_head.sn_num));
	json_object_object_add(output, "TOTAL_NODE_NUM", json_object_new_int(HASH_COUNT(gw_sn_head.hash_head)));
	json_object_object_add(output, "TOTAL_REG_NODE_NUM", json_object_new_int(reg_node_cnt()));
	list_for_each_safe(pos, tmp, &(gw_sn_head.sn_list))
	{
		tmp_sn_data = list_entry(pos, sn_data_t, sn_list);
		json_object *sn_obj = sn_data_serialize(tmp_sn_data, flag);
		memset(buff, 0, 100);
		sprintf(buff, "WSN_SEQ_%d", tmp_sn_data->sn_id);
		json_object_object_add(output, buff, sn_obj);
	}
	return output;
}

/**
 * @brief Get a south network base information from the device info base according to the service id.
 *
 * @param sn_id id of the south network
 *
 * @return pointer to the sn information structure on success, NULL on failure
 */
sn_data_t *sn_data_get(uint32_t sn_id)
{
	sn_data_t *tmp_sn_data = NULL;
	bool_t tmatch = FALSE;
	struct list_head *pos, *tmp;
	list_for_each_safe(pos, tmp, &(gw_sn_head.sn_list))
	{
		tmp_sn_data = list_entry(pos, sn_data_t, sn_list);
		if (tmp_sn_data->sn_id == sn_id)
		{
			tmatch = TRUE;
			break;
		}
	}
	if(!tmatch)
	{
		gw_log_print(GW_LOG_LEVEL_INFO, "SN data does not exist, not created yet!\n");
		tmp_sn_data = NULL;
	}
	return tmp_sn_data;
}

/**
 * @brief Get the first south network information of the device info base.
 *
 * @param NON
 *
 * @return pointer to the sn data structure on success, NULL on failure
 */
sn_data_t *sn_data_get_first(void)
{
	if (gw_sn_head.sn_list.next != &(gw_sn_head.sn_list))
	{
		return list_entry(gw_sn_head.sn_list.next, sn_data_t, sn_list);
	}
	else
	{
		return NULL;
	}
}

/**
 * @brief Get the head structure of the device info base.
 *
 * @param NON
 *
 * @return pointer to the head structure on success, NULL on failure
 */
sn_head_t *sn_data_head(void)
{
	return &gw_sn_head;
}

/**
 * @brief Create and initialize a south network data structure.
 *
 * @param NON
 *
 * @return pointer to a south network data structure on success, NULL on failure
 */
sn_data_t *sn_data_new(void)
{
	sn_data_t *sn;
	sn = (sn_data_t *) malloc(sizeof(sn_data_t));
	if (sn == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_CRITICAL, "SN data create error, not enough memory!\n");
		return NULL;
	}
	memset(sn, 0, sizeof(sn_data_t));

	INIT_LIST_HEAD(&(sn->sn_list));
	sn->protocal = PROTO_UNKNOWN;
	sn->pan_id = 0;
	sn->sn_id = 0; // used for sn hash key
	sn->node_num = 0;
	sn->profile_id = 0;
	sn->up_data = 0;
	sn->down_data = 0;
	INIT_LIST_HEAD(&(sn->node_list));
	sn->comment = NULL;
	return sn;
}

/**
 * @brief Add a south network information into the device info base.
 *
 * @param sn_data pointer to a south network data structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t sn_data_add(sn_data_t *sn_data)
{
	struct list_head *pos, *tmp;
	bool_t tmatch = FALSE;
	sn_data_t *tmp_sn_data;
	bool_t ret;
	gw_debug_print("enter sn data add\n");
	if (sn_data == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "SN data add error, sn data is NULL!\n");
		ret = FALSE;
	}
	else
	{
		list_for_each_safe(pos, tmp, &(gw_sn_head.sn_list))
		{
			tmp_sn_data = list_entry(pos, sn_data_t, sn_list);
			if (tmp_sn_data->sn_id == sn_data->sn_id)
			{
				tmatch = TRUE;
				break;
			}
		}
		if(tmatch)
		{
			gw_log_print(GW_LOG_LEVEL_WARNING, "SN data already exist!!\n");
			ret = FALSE;
		}
		else
		{
			gw_mutex_lock(gw_sn_head.mutex);
			list_add_tail(&(sn_data->sn_list), &(gw_sn_head.sn_list));
			gw_sn_head.sn_num++;
			gw_mutex_unlock(gw_sn_head.mutex);
			ret = TRUE;
		}
	}
	return ret;
}

/**
 * @brief Delete a south network information from the device info base.
 *
 * @param sn_data pointer to a south network data structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t sn_data_del(sn_data_t * sn_data)
{
	struct list_head *pos, *tmp;
	node_data_t *tmp_node_data;
	if (sn_data == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "SN data del error, sn data is NULL!\n");
		return FALSE;
	}
	gw_mutex_lock(gw_sn_head.mutex);
	list_for_each_safe(pos, tmp, &(sn_data->node_list))
	{
		tmp_node_data = list_entry(pos, node_data_t, entry);
		HASH_DEL(gw_sn_head.hash_head, tmp_node_data);
	}
	list_del(&(sn_data->sn_list));
	gw_sn_head.sn_num--;
	gw_mutex_unlock(gw_sn_head.mutex);
	return TRUE;
}

/**
 * @brief Free a south network data structure.
 *
 * This function not only free the sn data structure,
 * but also free the node data structure attached to the sn data structure.
 *
 * @param sn_data pointer to a south network data structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t sn_data_free(sn_data_t * sn_data)
{
	struct list_head *pos, *tmp;
	node_data_t *tmp_node_data;
	if (sn_data == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "SN data free error, sn data is NULL!\n");
		return FALSE;
	}
	list_for_each_safe(pos, tmp, &(sn_data->node_list))
	{
		tmp_node_data = list_entry(pos, node_data_t, entry);
		list_del(&(tmp_node_data->entry));
		node_data_free(tmp_node_data);
		sn_data->node_num--;
	}
	if (sn_data->comment != NULL)
	{
		bdestroy(sn_data->comment);
	}
	free(sn_data);
	sn_data = NULL;
	return TRUE;
}

/**
 * @brief Get next sn data struct safely.
 *
 * @param gw pointer to gateway context
 * @param curr pointer to current sn data struct, if NULL, the first sn data will be returned
 *
 * @return the next module pointer on success, NULL on failure
 */
sn_data_t *sn_get_next_safe(sn_data_t *curr, sn_data_t **next)
{
	sn_data_t *sn = NULL;

	if (curr == NULL)
	{
		sn = sn_data_get_first();
	}
	else
	{
		sn = curr;
	}

	if (sn && (sn->sn_list.next != &gw_sn_head.sn_list))
	{
		*next = list_entry(sn->sn_list.next, sn_data_t, sn_list);
	}
	else
	{
		*next = NULL;
	}

	return sn;
}

/**
 * @brief Output status information of a south network.
 *
 * @param sn_data	pointer to the sn data structure
 * @param flag	no idea!!!
 *
 * @return pointer to the serialized json string on success, NULL on failure
 */
json_object *sn_data_serialize(sn_data_t *sn_data, uint32_t flag)
{
	int32_t i;
	struct list_head *npos, *ntmp;
	node_data_t *tmp_node_data;
	int8_t buff[100];
	json_object *sn_obj = json_object_new_object();
	json_object_object_add(sn_obj, "NODE_NUM", json_object_new_int(sn_data->node_num));
	json_object_object_add(sn_obj, "REG_NODE_NUM", json_object_new_int(sn_data->reg_node_num));
	i=0;
	list_for_each_safe(npos, ntmp, &(sn_data->node_list))
	{
		tmp_node_data = list_entry(npos, node_data_t, entry);
		memset(buff, 0, 100);
		sprintf(buff, "NODE_SEQ_%d", ++i);
		json_object_object_add(sn_obj, buff, node_data_serialize(tmp_node_data, flag));
	}
	return sn_obj;
}

/**
 * @brief Create and initialize a node data structure.
 *
 * @param NON
 *
 * @return pointer to a node data structure on success, NULL on failure
 */
node_data_t *node_data_new(void)
{
	node_data_t *nd;
	nd = (node_data_t *) malloc(sizeof(node_data_t));
	if (nd == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_CRITICAL, "ND data create error, not enough memory!\n");
		return NULL;
	}
	/*   nd data init  */
	memset(nd, 0, sizeof(node_data_t));
	return nd;
}

/**
 * @brief Get a node data information from the device info base according to the nui of the node.
 *
 * @param nui network unified id of the the node
 * @param len length of the nui
 *
 * @return pointer to the node data structure on success, NULL on failure
 */
node_data_t *node_data_get(int8_t *nui, int32_t len)
{
	node_data_t *tmp_node_data = NULL;
	HASH_FIND(hh, gw_sn_head.hash_head, nui, len, tmp_node_data);
	return tmp_node_data;
}

/**
 * @brief Free a node network data structure.
 *
 * @param node_data pointer to a node data structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t node_data_free(node_data_t *node_data)
{
	if (node_data == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "ND data free error, nd data does not exist!\n");
		return FALSE;
	}
	if (node_data->comment != NULL)
	{
		bdestroy(node_data->comment);
	}
	free(node_data);
	node_data = NULL;
	return TRUE;
}

/**
 * @brief Add a node information into device info base.
 *
 * @param sn_data pointer to a south network data structure the node attaches
 * @param node_data pointer to a node data structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t node_data_add(sn_data_t *sn_data, node_data_t *node_data)
{
	node_data_t *tmp_data = NULL;
	int32_t i;
	if ((node_data == NULL) || (sn_data == NULL))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "ND data add error, nd data or sn data does not exist!\n");
		return FALSE;
	}
	HASH_FIND(hh, gw_sn_head.hash_head, node_data->nui, NUI_LEN, tmp_data);
	if (tmp_data != NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "ND data already exist in the hash table!\n");
		return FALSE;
	}
	node_data->pdata = sn_data;
	gw_mutex_lock(gw_sn_head.mutex);
	list_add_tail(&(node_data->entry), &(node_data->pdata->node_list));
	node_data->pdata->node_num++;
	if (node_data->state == STA_REG_TO_PLAT)
	{
		node_data->pdata->reg_node_num++;
	}
	HASH_ADD_KEYPTR(hh, gw_sn_head.hash_head, node_data->nui, NUI_LEN, node_data);
	gw_mutex_unlock(gw_sn_head.mutex);
	gw_debug_print("add a new data nui: ");
	for(i=0; i<NUI_LEN; i++)
	{
		printf("%02x ", (uint8_t)node_data->nui[i]);
	}
	printf("to sn %d\n", node_data->pdata->sn_id);
	if(node_data_get(node_data->nui, NUI_LEN) == NULL)
	{
		gw_debug_print("dev info base add node data error!\n");
	}
}

/**
 * @brief Delete a node information from device info base.
 *
 * @param node_data pointer to the node data information structure
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t node_data_del(node_data_t *node_data)
{
	node_data_t *tmp_data = NULL;
	if ((node_data == NULL) || (node_data->pdata == NULL))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "ND data free error, nd data or sn data does not exist!\n");
		return FALSE;
	}
	HASH_FIND(hh, gw_sn_head.hash_head, node_data->nui, NUI_LEN, tmp_data);
	if (tmp_data != node_data)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "ND data does not exist in the hash table!\n");
		return FALSE;
	}
	gw_mutex_lock(gw_sn_head.mutex);
	list_del(&(node_data->entry));
	node_data->pdata->node_num--;
	if (node_data->state == STA_REG_TO_PLAT)
	{
		node_data->pdata->reg_node_num--;
	}
	HASH_DEL(gw_sn_head.hash_head, node_data);
	gw_mutex_unlock(gw_sn_head.mutex);
}

/**
 * @brief Output status information of a node.
 *
 * @param node_data pointer to the node data information structure
 * @param flag	no idea!!!
 *
 * @return pointer to the serialized json string on success, NULL on failure
 */
json_object * node_data_serialize(node_data_t *node_data, uint32_t flag)
{
	json_object *my_object = NULL;
	my_object = json_object_new_object();
	int32_t i;
	int8_t buff[100];
	int8_t tmp_buff[10];
	memset(buff, 0, 100);
	for(i=0; i<NUI_LEN; i++)
	{
		sprintf(tmp_buff, "%02x", (uint8_t)node_data->nui[i]);
		strcat(buff, tmp_buff);
	}
	json_object_object_add(my_object, "NUI", json_object_new_string(buff));
	json_object_object_add(my_object, "UPDATE_INTERVAL", json_object_new_int(node_data->update_interval));
	if(node_data->state == STA_REG_TO_PLAT)
	{
		json_object_object_add(my_object, "LOG_STATUS", json_object_new_string("REGISTERED"));
		json_object_object_add(my_object, "LAST_UPDATE_TIME", json_object_new_string(
				ctime(&node_data->last_update_time)));
	}
	else
	{
		json_object_object_add(my_object, "LOG_STATUS", json_object_new_string("UNREGISTERED"));
		json_object_object_add(my_object, "LAST_UPDATE_TIME", json_object_new_string("0"));
	}
	return my_object;
}



/**
 * @}
 */
