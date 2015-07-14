/*
 * mod_devmanage.c
 *
 *  Created on: 2010-12-2
 *      Author: lilei
 */
#include <module.h>
#include <dotconf/dotconf.h>

/* module skeleton config information */
typedef struct __tag_devman_config
{
	int32_t unreg_interv;
} devman_conf_t;

typedef struct __tag_devman_stats
{
	int32_t login_request_num;
	int32_t login_success_num;
	int32_t logout_request_num;
	int32_t logout_success_num;
	int64_t info_update_num;
} devman_stats_t;

static gw_mod_t mod_devman;
static gw_mod_data_t devman_data;
static gw_svc_t devman_svc;
static gw_msgq_t *devman_msgq;
static gw_thread_t devman_thread = NULL;
static devman_conf_t devman_conf;
static devman_stats_t devman_stats;

bool_t init(gw_t *gw, gw_dlib_t lib);
bool_t cleanup(gw_t *gw);
//bool_t serialize(gw_t *gw, bstring *buf);
json_object *serialize(gw_t *gw);
static DOTCONF_CB(default_callback);

static const configoption_t options[] =
{
{ "unreg_keep_interval", ARG_INT, default_callback, NULL, 0 },
LAST_OPTION };

static int32_t devman_config(int8_t *path_filename)
{
	configfile_t *configfile;

	if (path_filename == NULL)
	{
		printf("error: NULL input\n");
		return 1;
	}
	configfile = dotconf_create(path_filename, options, NULL, CASE_INSENSITIVE);
	if (!configfile)
	{
		fprintf(stderr, "Error creating configfile\n");
		return 1;
	}

	if (dotconf_command_loop(configfile) == 0)
	{
		fprintf(stderr, "Error parsing config file %s\n", configfile->filename);
		return 1;
	}

	dotconf_cleanup(configfile);

	return 0;
}

DOTCONF_CB(default_callback)
{
	int32_t i, arg_count;
	bstring command;
	bstring tmpcmd;
	int8_t *ret = NULL;

	command = bfromcstr(cmd->name);
	arg_count = cmd->arg_count;
	tmpcmd = bfromcstr("unreg_keep_interval");
	if (bstricmp(command, tmpcmd) == 0)
	{
		devman_conf.unreg_interv = cmd->data.value;
		gw_debug_print("the gw info update interval is %d\n", devman_conf.unreg_interv);
	}

	bdestroy(command);
	bdestroy(tmpcmd);
	return ret;
}

static gw_msg_t *gen_ack_msg(gw_msg_t *msg, uint16_t error_code)
{
	gw_msg_t *rsp_msg;
	rsp_msg = gw_msg_alloc();
	if (rsp_msg != NULL)
	{
		rsp_msg->version.major = msg->version.major;
		rsp_msg->version.minor = msg->version.minor;
		rsp_msg->serial = msg->serial;
		rsp_msg->dstsvc = msg->srcsvc;
		rsp_msg->srcsvc = msg->dstsvc;
		memcpy(rsp_msg->devid, msg->devid, NUI_LEN);
		rsp_msg->retcode = error_code;
		rsp_msg->type = msg->type + 0x8000;
	}
	return rsp_msg;
}

void *devman_svc_handler(void *args)
{
	gw_msg_t *msg, *rsp_msg;
	sn_data_t *tmp_sn_data;
	node_data_t *tmp_node_data;
	gw_t *gw = (gw_t *) args;
	bool_t ret;

	/* initialize infobase */

	while ((msg = gw_msg_recv(devman_msgq)) != NULL)
	{
		if (msg->type == GW_MSG_SYSTEM_QUIT)
		{
			gw_msg_free(msg);
			gw_log_print(GW_LOG_LEVEL_INFO, "DEV management module receive a quit msg!\n");
			break;
		}
		if ((msg->srcsvc >= 0x1000) && (msg->srcsvc < 0x2000))
		{
			/*  this msg is from south interface */
			gw_debug_print("module dev management receive a msg type is %d\n", msg->type);
			switch (msg->type)
			{
			case DEVICE_LOGIN_REQUEST:
			{
				tmp_node_data = NULL;
				tmp_node_data = node_data_get(msg->devid, NUI_LEN);
				if (tmp_node_data == NULL)
				{
					gw_log_print(GW_LOG_LEVEL_INFO, "Receive a new device!\n");
					if((msg->data != NULL) && (msg->datasize == 4))
					{
						uint32_t snet_id = *((uint32_t *)msg->data);
						tmp_sn_data = sn_data_get(snet_id);
						if (tmp_sn_data == NULL)
						{
							gw_log_print(GW_LOG_LEVEL_WARNING, "Sn data does not exist!\n");
							gw_msg_free(msg);
						}
						else
						{
							tmp_node_data = node_data_new();
							memcpy(tmp_node_data->nui, msg->devid, NUI_LEN);
							tmp_node_data->pdata = tmp_sn_data;
							tmp_node_data->state = STA_REG_TO_SN;
							tmp_node_data->last_update_time = time(NULL);
							node_data_add(tmp_sn_data, tmp_node_data);
							devman_stats.login_request_num ++;

							msg->srcsvc = devman_svc.id;
							msg->dstsvc = PLAT_DM_SVC;
							gw_msg_send(gw, msg);
						}
					}
					else
					{
						gw_log_print(GW_LOG_LEVEL_WARNING, "No sn id included in reg msg, data size is %d, data is %d!\n", msg->datasize, msg->data);
						gw_msg_free(msg);
					}
				}
				else
				{
//					if (tmp_node_data->state == STA_REG_TO_PLAT)
//					{
//						/* already registered */
//						rsp_msg = gen_ack_msg(msg, EC_SUCCESS);
//						gw_msg_free(msg);
//						gw_msg_send(gw, rsp_msg);
//						gw_debug_print("node already registered!\n");
//					}
//					else
//					{
						msg->srcsvc = devman_svc.id;
						msg->dstsvc = PLAT_DM_SVC;
						gw_msg_send(gw, msg);
//					}
				}
				break;
			}
			case DEVICE_LOGOUT_REQUEST:
			{
				tmp_node_data = NULL;
				tmp_node_data = node_data_get(msg->devid, NUI_LEN);
				if (tmp_node_data == NULL)
				{
					rsp_msg = gen_ack_msg(msg, EC_DEV_UNJOIN);
					gw_debug_print("Leave msg receive error, node info not exist!\n");
					gw_msg_free(msg);
					gw_msg_send(gw, rsp_msg);
				}
				else
				{
//					if (tmp_node_data->state != STA_REG_TO_PLAT)
//					{
//						/* already registered */
//						rsp_msg = gen_ack_msg(msg, EC_DEV_UNREG);
//						gw_msg_free(msg);
//						gw_msg_send(gw, rsp_msg);
//						gw_debug_print("node not registered!\n");
//					}
//					else
//					{
						devman_stats.logout_request_num ++;
						msg->srcsvc = devman_svc.id;
						msg->dstsvc = PLAT_DM_SVC;
						gw_msg_send(gw, msg);
//					}
				}
				break;
			}
			case DEVICE_UPDATE_REQUEST:
			{
				tmp_node_data = NULL;
				tmp_node_data = node_data_get(msg->devid, NUI_LEN);
//				if ((tmp_node_data == NULL) || (tmp_node_data->state
//						!= STA_REG_TO_PLAT))
//				{
//					//rsp_msg = gen_ack_msg(msg, EC_DEV_UNREG);
//					gw_debug_print("Dev info update error! not registered!\n");
//					gw_msg_free(msg);
//					//gw_msg_send(gw, rsp_msg);
//				}
//				else
//				{
				if (tmp_node_data == NULL)
				{
					//rsp_msg = gen_ack_msg(msg, EC_DEV_UNREG);
					gw_debug_print("Dev info update error! not registered!\n");
					gw_msg_free(msg);
					//gw_msg_send(gw, rsp_msg);
				}
				else
				{

					devman_stats.info_update_num ++;
					tmp_node_data->last_update_time = time(NULL);
					msg->srcsvc = devman_svc.id;
					msg->dstsvc = PLAT_DM_SVC;
					gw_msg_send(gw, msg);
				}
				break;
			}
			case PASSIVE_CONFIG_RESPONSE:
			{
				gw_debug_print("Receive a passive config response packet!\n");
				gw_msg_free(msg);
				break;
			}
			default:
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "Dev management receive unknown msg type from sn!\n");
				gw_msg_free(msg);
			}
			}
		}
		else if (msg->srcsvc >= 0x2000)
		{
			/* this msg is from north interface */
			switch (msg->type)
			{
			case DEVICE_LOGIN_RESPONSE:
			{
				gw_debug_print("dev manage receive a login response!\n");
				tmp_node_data = NULL;
				tmp_node_data = node_data_get(msg->devid, NUI_LEN);
				if (tmp_node_data == NULL)
				{
					gw_log_print(GW_LOG_LEVEL_WARNING, "Node data does not exist!\n");
					gw_msg_free(msg);
				}
				else
				{
					if (msg->retcode == 0)
					{
						/*  node register success */
						devman_stats.login_success_num ++;
						if (tmp_node_data->state != STA_REG_TO_PLAT)
						{
							tmp_node_data->state = STA_REG_TO_PLAT;
							tmp_node_data->pdata->reg_node_num++;
						}
					}
					else
					{
						if (tmp_node_data->state == STA_REG_TO_PLAT)
						{
							tmp_node_data->state = STA_LOST_REG;
							tmp_node_data->pdata->reg_node_num--;
						}
					}
					msg->srcsvc = devman_svc.id;
					msg->dstsvc = SN_DM_SVC;
					ret = gw_msg_send(gw, msg);
					if(ret == FALSE)
					{
						gw_log_print(GW_LOG_LEVEL_WARNING, "msg send error!\n");
						gw_msg_free(msg);	
					}
				}
				break;
			}
			case DEVICE_LOGOUT_RESPONSE:
			{
				gw_debug_print("receive a log out response message!\n");
				tmp_node_data = NULL;
				tmp_node_data = node_data_get(msg->devid, NUI_LEN);
				if (tmp_node_data == NULL)
				{
					gw_log_print(GW_LOG_LEVEL_WARNING, "Node data does not exist!\n");
					gw_msg_free(msg);
				}
				else
				{
					if (tmp_node_data->state == STA_REG_TO_PLAT)
					{
						msg->srcsvc = devman_svc.id;
						msg->dstsvc = SN_DM_SVC;
						ret = gw_msg_send(gw, msg);
 						if(ret == FALSE)
                                        	{
                                                	gw_log_print(GW_LOG_LEVEL_WARNING, "msg send error!\n");
                                                	gw_msg_free(msg);
                                        	}
						if (msg->retcode == 0)
						{
							tmp_node_data->state = STA_LOST_REG;
						}
					}
					else
					{
						gw_msg_free(msg);
						gw_debug_print("Not registered yet!");
					}
				}
				break;
			}
			case PASSIVE_CONFIG_REQUEST:
			{
//				gw_debug_print("dev manage passive config msg!\n");
				tmp_node_data = NULL;
				tmp_node_data = node_data_get(msg->devid, NUI_LEN);
				if (tmp_node_data == NULL)
				{
					gw_log_print(GW_LOG_LEVEL_WARNING, "Node data does not exist!\n");
					gw_msg_free(msg);
				}
				else
				{
					if (tmp_node_data->state == STA_REG_TO_PLAT)
					{
						uint8_t *pt = (uint8_t *)msg->data;
						uint32_t tmp_intv = *((uint32_t *)(pt+6))/1000;
//						if(tmp_intv != tmp_node_data->update_interval)
//						{
							tmp_node_data->update_interval = tmp_intv;
							gw_debug_print("device update interval is set to %d\n", tmp_intv);
							msg->srcsvc = devman_svc.id;
							msg->dstsvc = SN_DM_SVC;
							ret = gw_msg_send(gw, msg);
							if(ret == FALSE)
                                        		{
                                               			gw_log_print(GW_LOG_LEVEL_WARNING, "msg send error!\n");
                                                		gw_msg_free(msg);
                                        		}
//						}else
//						{
//							gw_msg_free(msg);
//						}
					}
					else
					{
						gw_msg_free(msg);
						gw_debug_print("Not registered yet, can not config!");
					}
				}
				break;
			}
			default:
				gw_log_print(GW_LOG_LEVEL_WARNING, "unknown message type!\n");
				gw_msg_free(msg);
				break;
			}
		}
	}

}

bool_t init(gw_t *gw, gw_dlib_t lib)
{
	bool_t ret = TRUE;

	/* initialize module data */
	memset(&devman_data, 0, sizeof(devman_data));
	memset(&devman_conf, 0, sizeof(devman_conf));
	memset(&devman_stats, 0, sizeof(devman_stats));

	/* read config infromation */
	//devman_config("/home/makui/projects/gw/i386-linux-gcc-debug-mod-skeleton/devman.conf");

	devman_data.config = &devman_conf;
	devman_data.stats = &devman_stats;
	/* initialize module structure */
	memset(&mod_devman, 0, sizeof(mod_devman));
	mod_devman.name = "mod_devmanage";
	mod_devman.desc = "mod device management";
	mod_devman.vendor = "WSN";
	mod_devman.modver.major = 1;
	mod_devman.modver.minor = 0;
	mod_devman.sysver.major = 1;
	mod_devman.sysver.minor = 0;
	mod_devman.init = init;
	mod_devman.cleanup = cleanup;
	mod_devman.serialize = serialize;
	mod_devman.lib = lib;
	mod_devman.data = &devman_data;
	mod_devman.lock = gw_mutex_create();
	INIT_LIST_HEAD(&mod_devman.services);

	devman_msgq = gw_msgq_create();
	memset(&devman_svc, 0, sizeof(devman_svc));
	devman_svc.id = DEV_MAN_SVC;
	devman_svc.name = "devman_service";
	devman_svc.mod = &mod_devman;
	devman_svc.mq = devman_msgq;

	devman_thread = gw_thread_create(devman_svc_handler, (void *) gw);

	//TODO: initialize another stuff
	if (mod_devman.lock != NULL)
	{
		ret = mod_register(gw, &mod_devman);
		if (ret)
		{
			ret = mod_register_service(gw, &mod_devman, &devman_svc);
			if (ret != TRUE)
			{
				mod_unregister(gw, &mod_devman);
				gw_mutex_destroy(mod_devman.lock);
				gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_devman.name);
			}
			else
			{
				gw_log_print(GW_LOG_LEVEL_INFO, "module %s initialized\n", mod_devman.name);
			}
		}
	}

	return ret;
}

bool_t cleanup(gw_t *gw)
{
	//TODO: cleanup module stuff
	gw_msg_t *msg;

	json_object *my_object;
	my_object = serialize(gw);
	printf("my_object.to_string()=%s\n", json_object_to_json_string(my_object));
	json_object_put(my_object);

	msg = gw_msg_alloc();
	msg->type = GW_MSG_SYSTEM_QUIT;
	msg->dstsvc = devman_svc.id;
	msg->srcsvc = devman_svc.id;
	gw_msg_send(gw, msg);

	gw_thread_wait(devman_thread);

	mod_unregister_service(gw, &mod_devman, &devman_svc);
	mod_unregister(gw, &mod_devman);

	gw_msgq_destroy(devman_msgq);
	gw_mutex_destroy(mod_devman.lock);

	return TRUE;
}

json_object *serialize(gw_t *gw)
{
	json_object *output = NULL;
	int32_t i;
	sn_data_t *curr = NULL;
	sn_data_t *next = NULL;
	output = json_object_new_object();
	json_object_object_add(output, "NODE_LIGIN_PACKET_NUM",
			json_object_new_int(devman_stats.login_request_num));
	json_object_object_add(output, "NODE_LIGIN_SUCCESS_NUM",
			json_object_new_int(devman_stats.login_success_num));
	json_object_object_add(output, "NODE_LIGOUT_PACKET_NUM",
			json_object_new_int(devman_stats.logout_request_num));
	json_object_object_add(output, "NODE_LIGOUT_SUCCESS_NUM",
			json_object_new_int(devman_stats.logout_success_num));
	json_object_object_add(output, "NODE_INFO_PACKET_NUM", json_object_new_int(
			devman_stats.info_update_num));
	json_object_object_add(output, "WSN_STATUS", dev_infobase_serialize(0));
	return output;
}

