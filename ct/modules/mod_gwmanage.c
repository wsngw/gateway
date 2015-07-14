/*
 * mod_gwmanage.c
 *
 *  Created on: 2010-11-20
 *      Author: lilei
 *   Copyright: 2010, WSN
 */
#include <sys/stat.h>
#include <fcntl.h>
#include <module.h>
#include <dotconf/dotconf.h>
#include <libevent/event.h>

static uint32_t dmp_serial;
static uint32_t dcp_serial;

typedef struct __tag_send_list
{
	struct list_head header;
	gw_mutex_t mutex;
	int num;
} send_list_t;

/***** msg already send *******/
typedef struct __tag_send_msg_info
{
	struct list_head entry;
	int32_t serial;
	int16_t svcid;
} send_msg_info_t;

/***** north interface config information *******/
typedef struct __tag_ni_config
{
	uint32_t info_update_interval;

} gwmanage_conf_t;

/***** north interface config information *******/
typedef struct __tag_ni_stats
{
	/***** dmp status *****/
	int32_t info_update_send_num;
	time_t last_update_time;
} gwmanage_stats_t;

static gw_mod_t mod_gwmanage;
static gw_svc_t dcp_svc;
static gw_svc_t dmp_svc;
static gw_mod_data_t gwmanage_data;
static gw_msgq_t *dcp_msgq;
static gw_msgq_t *dmp_msgq;
static gwmanage_conf_t gwmanage_conf;
static gwmanage_stats_t gwmanage_stats;
//static send_list_t send_list;
static gw_thread_t send_thread;
static gw_thread_t dcp_recv_thread;
static gw_thread_t dmp_recv_thread;
static int8_t *gwman_fifo = "/tmp/gw_man.fifo";

bool_t init(gw_t *gw, gw_dlib_t lib);
bool_t cleanup(gw_t *gw);
//bool_t serialize(gw_t *gw, bstring *buf);
json_object *serialize(gw_t *gw);
static DOTCONF_CB(default_callback);

static const configoption_t options[] =
{
	{ "update_interval", ARG_INT, default_callback, NULL, 0 },
	LAST_OPTION
};

/* load configuration parameters */
static bool_t load_config(char *path_filename)
{
	configfile_t *configfile;
	bool_t ret;

	if (path_filename == NULL)
	{
		gw_debug_print("error: NULL input\n");
		ret = FALSE;
	}
	else
	{
		configfile = dotconf_create(path_filename, options, NULL,
				CASE_INSENSITIVE);
		if (!configfile)
		{
			gw_debug_print(stderr, "Error creating configfile\n");
			ret = FALSE;
		}
		else if (dotconf_command_loop(configfile) == 0)
		{
			gw_debug_print(stderr, "Error parsing config file %s\n",configfile->filename);
			ret = FALSE;
		}else
		{
			ret = TRUE;
		}
		dotconf_cleanup(configfile);
	}

	return ret;
}
/* Fill gwmanage_conf with parameters read from configuration file
 *
 * called every time there is a valid line in configuration file
 *
 *  */
DOTCONF_CB(default_callback)
{
	int i, arg_count;
	bstring command;
	bstring tmpcmd;

	command = bfromcstr(cmd->name);
	arg_count = cmd->arg_count;
	tmpcmd = bfromcstr("update_interval");
	if (bstricmp(command, tmpcmd) == 0)
	{
		gwmanage_conf.info_update_interval = cmd->data.value;
		gw_debug_print("the gw manage update interval is set to %d\n", gwmanage_conf.info_update_interval);
	}
	bdestroy(command);
	bdestroy(tmpcmd);

	return NULL;
}

/*
 * create a list to record the message already send to platform
 */
//static bool_t send_msglist_init(void)
//{
//	bool_t ret = TRUE;
//	send_list.num = 0;
//	INIT_LIST_HEAD(&(send_list.header));
//	send_list.mutex = gw_mutex_create();
//	if (send_list.mutex == NULL)
//	{
//		gw_log_print(GW_LOG_LEVEL_WARNING, "Gw manage module send list init error, can not create mutex!\n");
//		return FALSE;
//	}
//	return ret;
//}
//
//static send_msg_info_t *send_msglist_add(uint32_t serial, uint16_t svcid)
//{
//	send_msg_info_t *msg_info = (send_msg_info_t *)malloc(sizeof(send_msg_info_t));
//	if(msg_info != NULL)
//	{
//		msg_info->serial = serial;
//		msg_info->svcid = svcid;
//		gw_mutex_lock(send_list.mutex);
//		list_add_tail(&(msg_info->entry), &(send_list.header));
//		send_list.num++;
//		gw_mutex_unlock(send_list.mutex);
//	}
//	return msg_info;
//}
//
//static send_msg_info_t *send_msglist_del(uint32_t serial, uint16_t svcid)
//{
//	send_msg_info_t *msg_info = NULL;
//	struct list_head *pos, *tmp_pos;
//	bool_t tmatch = FALSE;
//	list_for_each_safe(pos, tmp_pos, &(send_list.header))
//	{
//		msg_info = list_entry(pos, send_msg_info_t, entry);
//		if ((msg_info->serial == serial) && (msg_info->svcid == svcid))
//		{
//			gw_mutex_lock(send_list.mutex);
//			list_del(&(msg_info->entry));
//			send_list.num--;
//			gw_mutex_unlock(send_list.mutex);
//			tmatch = TRUE;
//			break;
//		}
//	}
//	if(!tmatch)
//	{
//		gw_debug_print("mod gw management send msg list delete error!\n");
//		msg_info = NULL;
//	}
//	return msg_info;
//}
///*
// * destroy the list recording msgs send to platform
// */
//static bool_t send_msglist_destroy(void)
//{
//	bool_t ret;
//	struct list_head *pos, *tmp;
//	send_msg_info_t *msg;
//
//	/* check if the list still have msgs left */
//	if (send_list.num != 0)
//	{
//		gw_log_print(GW_LOG_LEVEL_WARNING,"mod gw management send list not empty, send list num is %d!\n", send_list.num);
//		list_for_each_safe(pos, tmp, &send_list.header)
//		{
//			msg = list_entry(pos, send_msg_info_t, entry);
//			list_del(&msg->entry);
//			free(msg);
//			msg = NULL;
//		}
//	}
//	ret = gw_mutex_destroy(send_list.mutex);
//	return ret;
//}

/* Processes platform initiated dcp data destined to gateway */
void *dcp_recv_handler(void *args)
{
	gw_msg_t *msg, *rsp_msg;
	int32_t *p;
	struct list_head *pos, *tmp_pos;
	send_msg_info_t *msg_info;
	gw_t *gw = (gw_t *)args;

	while ((msg = gw_msg_recv(dcp_msgq)) != NULL)
	{
		if (msg->type == GW_MSG_SYSTEM_QUIT)
		{
			gw_msg_free(msg);
			gw_debug_print("Mod_gwmanagement dcp recv thread receive a quit message!\n");
			break;
		}
		if (msg->dstsvc == GW_DATAINIT_SERVICE)
		{
			if (msg->type == UPSTREAM_DATA_ACK)
			{
//				msg_info = send_msglist_del(msg->serial, msg->dstsvc);
//				free(msg_info);
				msg_info = NULL;
				gw_debug_print("mod management receive serial %d resposce, results is %d\n", msg->serial, msg->retcode);
				gw_msg_free(msg);
			}
			else if (msg->type == DOWNSTREAM_DATA)
			{
				gw_debug_print("mod management receive a platform request message, free it!\n");
				rsp_msg = gw_msg_alloc();
				rsp_msg->version.major = msg->version.major;
				rsp_msg->version.minor = msg->version.minor;
				rsp_msg->serial = msg->serial;
				rsp_msg->type = DOWNSTREAM_DATA_ACK;
				rsp_msg->dstsvc = msg->srcsvc;
				rsp_msg->srcsvc = msg->dstsvc;
				memcpy(rsp_msg->devid, msg->devid, NUI_LEN);
				rsp_msg->datasize = 0;
				rsp_msg->retcode = 0;
				gw_msg_free(msg);
				gw_msg_send(gw, rsp_msg);
				printf("free success!\n");
			}
			else
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "unknown message type %u received by service %u, module %s\n",
						msg->type, dcp_svc.id, mod_gwmanage.name);
				gw_msg_free(msg);
			}
		}
		else
		{
			gw_log_print(GW_LOG_LEVEL_WARNING, "unknown dstsvc %u received by module %s\n",
					dcp_svc.id, mod_gwmanage.name);
			gw_msg_free(msg);
		}
	}
	gw_debug_print("Gwmanage module dcp recv thread quit!\n");
	gw_thread_exit(0);
}

/* Processes platform initiated dmp data destined to gateway */
void *dmp_recv_handler(void *args)
{
	gw_msg_t *msg;
	struct list_head *pos, *tmp_pos;
//	send_msg_info_t *msg_info;
	gw_t *gw = (gw_t *) args;

	while ((msg = gw_msg_recv(dmp_msgq)) != NULL)
	{
		if (msg->type == GW_MSG_SYSTEM_QUIT)
		{
			gw_msg_free(msg);
			gw_debug_print("Mod_gwmanagement dmp recv thread receive a quit message!\n");
			break;
		}
		if (msg->dstsvc == GW_MAN_SVC)
		{
			switch (msg->type)
			{
			case DEVICE_LOGIN_RESPONSE:
			{
				//add process here!
				gw_debug_print("Mod_gwmanagement receive a dmp login response");
//				msg_info = send_msglist_del(msg->serial, msg->dstsvc);
//				free(msg_info);
//				msg_info = NULL;
				if (msg->retcode == 0)
				{
					gw->data->gw_state = STA_REG_TO_PLAT;
					gw_debug_print("gw is registed!!!!\n");
				}
				break;
			}
			case DEVICE_LOGOUT_RESPONSE:
			{
				//add process here!
//				msg_info = send_msglist_del(msg->serial, msg->dstsvc);
//				if(msg_info == NULL)
//				{
//					gw_log_print(GW_LOG_LEVEL_WARNING, "Unproper log out response, gateway has not send log out request!\n");
//				}
//				else
//				{
//					free(msg_info);
//					msg_info = NULL;
//					gw_debug_print("mod management receive a device leave resposce, results is %d\n", msg->retcode);
//				}
				if (msg->retcode == 0)
				{
					gw->data->gw_state = STA_LOST_REG;
					gw_debug_print("gw is unregisted!!!!\n");
				}
				gw_debug_print("mod management receive a device leave resposce, results is %d\n", msg->retcode);
				break;
			}
			case PASSIVE_CONFIG_REQUEST:
			{
				gw_debug_print("mod management receive a config message, error code is %d!\n", msg->retcode);
				if((!msg->retcode) && (msg->datasize != 0))
				{
					uint8_t *pt = (uint8_t *)msg->data;
					gwmanage_conf.info_update_interval = *((uint32_t *)(pt+6))/1000;
					//memcpy(&(gwmanage_conf.info_update_interval), pt+6, 4);
					//gwmanage_conf.info_update_interval = gwmanage_conf.info_update_interval/1000;
					gw_debug_print("Platform config the gw interval is %d\n", gwmanage_conf.info_update_interval);
				}
				break;
			}
			default:
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "unknown message type %u received by service %u, module %s\n",
						msg->type, dmp_svc.id, mod_gwmanage.name);
				break;
			}
			}
		}
		else
		{
			gw_log_print(GW_LOG_LEVEL_WARNING, "unknown dstsvc %u received by module %s\n",
					dmp_svc.id, mod_gwmanage.name);
		}
		gw_msg_free(msg);
	}
	gw_debug_print("Gwmanage module dmp recv thread quit!\n");
	gw_thread_exit(0);
}

static int32_t dcp_cnt;
static int32_t dmp_cnt;

/* initiate dcp data for test */
static void dcp_timeout_cb(int fd, short event, void *arg)
{
	struct timeval tv;
	int32_t i;
	event_cb_arg_t *time_arg = (event_cb_arg_t *) arg;
	gw_msg_t *msg;
	send_msg_info_t *msg_info;

	msg = gw_msg_alloc();
	msg->serial = dcp_serial++;
	msg->type = UPSTREAM_DATA;
	memcpy(msg->devid, time_arg->gw->config->nui->data, NUI_LEN);
	msg->dstsvc = PLAT_DATA_SVC;
	msg->srcsvc = dcp_svc.id;

	msg->datasize = 50;
	msg->data = malloc(50);
	for (i = 0; i < 50; i++)
	{
		*((int8_t *) (msg->data + i)) = dcp_cnt + i;
	}
//	send_msglist_add(msg->serial, msg->srcsvc);
	gw_debug_print("Init a dcp data!\n");

	gw_msg_send(time_arg->gw, msg);

	evutil_timerclear(&tv);
	tv.tv_sec = 60;
	event_add(time_arg->evt, &tv);
}

/* initiate dmp data for test */
static void dmp_timeout_cb(int fd, short event, void *arg)
{
	struct timeval tv;
	event_cb_arg_t *time_arg = (event_cb_arg_t *) arg;
	gw_msg_t *msg;
	send_msg_info_t *msg_info;
	struct timezone tz;

	if (time_arg->gw->data->gw_state == STA_INIT)
	{
		msg = gw_msg_alloc();
		msg->serial = dmp_serial++;
		msg->type = DEVICE_LOGIN_REQUEST;
		memcpy(msg->devid, time_arg->gw->config->nui->data, NUI_LEN);
		msg->dstsvc = PLAT_DM_SVC;
		msg->srcsvc = dmp_svc.id;
		msg->datasize = 0;
//		send_msglist_add(msg->serial, msg->srcsvc);
		gw_msg_send(time_arg->gw, msg);
		gw_debug_print("Init a dmp register message!\n");

		evutil_timerclear(&tv);
		tv.tv_sec = 30;
		event_add(time_arg->evt, &tv);
	}
	else if (time_arg->gw->data->gw_state == STA_REG_TO_PLAT)
	{
		msg = gw_msg_alloc();
		msg->serial = dmp_serial++;
		msg->type = DEVICE_UPDATE_REQUEST;
		memcpy(msg->devid, time_arg->gw->config->nui->data, NUI_LEN);
		msg->dstsvc = PLAT_DM_SVC;
		msg->srcsvc = dmp_svc.id;
		msg->datasize = 0;
		//send_msglist_add(msg->serial, msg->srcsvc);
		gwmanage_stats.info_update_send_num ++;
		//gettimeofday(&(gwmanage_stats.last_update_time), &tz);
		gwmanage_stats.last_update_time = time(NULL);
		gw_msg_send(time_arg->gw, msg);
		gw_debug_print("Init a dmp info updata message!\n");

		evutil_timerclear(&tv);
		tv.tv_sec = gwmanage_conf.info_update_interval;
		event_add(time_arg->evt, &tv);
	}

}
/* Wait to read a quit msg for the send thread event loop */
static void fifo_read_cb(int fd, short event, void *arg)
{
	struct event *evfifo = (struct event *) arg;
	char buf[255];
	int len;
	len = read(fd, buf, sizeof(buf) - 1);

	if (len == -1)
	{
		gw_log_print(GW_LOG_LEVEL_INFO, "Mod gw_man fifo read error!\n");
	}
	else
	{
		gw_log_print(GW_LOG_LEVEL_INFO, "Mod gw_man send thread exit!\n");
	}
	event_base_loopbreak(evfifo->ev_base);
}
/* init dcp/dmp data for test */
void *send_handler(void *args)
{
	gw_msg_t *msg;
	gw_t *gw = (gw_t *) args;
	struct event timeoutdcp, timeoutdmp;
	struct timeval tv;
	event_cb_arg_t dcptime_arg, dmptime_arg;
	send_msg_info_t *msg_info;
	bool_t ret = TRUE;

	struct event evfifo;
	struct stat st;
	int32_t fifofd;

	if (lstat(gwman_fifo, &st) == 0)
	{
		if ((st.st_mode & S_IFMT) == S_IFREG)
		{
			gw_debug_print("lstat");
			exit(1);
		}
	}

	unlink(gwman_fifo);
	if (mkfifo(gwman_fifo, 0777) == -1)
	{
		gw_debug_print("mkfifo");
		exit(1);
	}
	fifofd = open(gwman_fifo, O_RDWR | O_NONBLOCK, 0);
	if (fifofd == -1)
	{
		gw_debug_print("open");
		exit(1);
	}

	if(ret)
	{
		msg = gw_msg_alloc();
		msg->serial = dmp_serial++;
		msg->type = DEVICE_LOGIN_REQUEST;
		memcpy(msg->devid, gw->config->nui->data, NUI_LEN);
		msg->dstsvc = PLAT_DM_SVC;
		msg->srcsvc = dmp_svc.id;
		msg->datasize = 0;
		//send_msglist_add(msg->serial, msg->srcsvc);
		gwmanage_stats.last_update_time = time(NULL);
		gw_msg_send(gw, msg);
		/* Initalize the event library */
		event_init();

		/* Initalize one event */
//		dcptime_arg.gw = (gw_t *) args;
//		dcptime_arg.evt = &timeoutdcp;
//		evtimer_set(&timeoutdcp, dcp_timeout_cb, &dcptime_arg);
//		evutil_timerclear(&tv);
//		tv.tv_sec = 1;
//		event_add(&timeoutdcp, &tv);

		dmptime_arg.gw = (gw_t *) args;
		dmptime_arg.evt = &timeoutdmp;
		evtimer_set(&timeoutdmp, dmp_timeout_cb, &dmptime_arg);
		evutil_timerclear(&tv);
		tv.tv_sec = 5;
		event_add(&timeoutdmp, &tv);

		event_set(&evfifo, fifofd, EV_READ, fifo_read_cb, (void *) &evfifo);
		event_add(&evfifo, NULL);

		event_dispatch();
	}
	close(fifofd);
	gw_debug_print("gw management done\n");
}

bool_t init(gw_t *gw, gw_dlib_t lib)
{
	bool_t ret;

	/* initialize module data */
	memset(&gwmanage_data, 0, sizeof(gwmanage_data));
	memset(&gwmanage_conf, 0, sizeof(gwmanage_conf));
	memset(&gwmanage_stats, 0, sizeof(gwmanage_stats));

	/* read config infromation */
	ret = load_config("/etc/gw/modules/gwmanage.conf");
	gwmanage_data.config = &gwmanage_conf;
	gwmanage_data.stats = &gwmanage_stats;

	srand((unsigned) time(NULL));
	dcp_serial = rand();
	srand((unsigned) time(NULL));
	dmp_serial = rand() * 2;
	/* initialize module structure */
	memset(&mod_gwmanage, 0, sizeof(mod_gwmanage));
	mod_gwmanage.name = "mod_gwmanage";
	mod_gwmanage.desc = "mod gw management, the gw management module";
	mod_gwmanage.vendor = "WSN";
	mod_gwmanage.modver.major = 1;
	mod_gwmanage.modver.minor = 0;
	mod_gwmanage.sysver.major = 1;
	mod_gwmanage.sysver.minor = 0;
	mod_gwmanage.init = init;
	mod_gwmanage.cleanup = cleanup;
	mod_gwmanage.serialize = serialize;
	mod_gwmanage.lib = lib;
	mod_gwmanage.data = &gwmanage_data;
	mod_gwmanage.lock = gw_mutex_create();
	INIT_LIST_HEAD(&mod_gwmanage.services);

	dcp_msgq = gw_msgq_create();
	dmp_msgq = gw_msgq_create();

	memset(&dcp_svc, 0, sizeof(dcp_svc));
	dcp_svc.id = GW_DATAINIT_SERVICE;
	dcp_svc.name = "gw_datainit_service";
	dcp_svc.mod = &mod_gwmanage;
	dcp_svc.mq = dcp_msgq;

	memset(&dmp_svc, 0, sizeof(dmp_svc));
	dmp_svc.id = GW_MAN_SVC;
	dmp_svc.name = "GW_MAN_SVC";
	dmp_svc.mod = &mod_gwmanage;
	dmp_svc.mq = dmp_msgq;

	if (mod_gwmanage.lock != NULL)
	{
		ret = mod_register(gw, &mod_gwmanage);
		if (ret)
		{
			ret = mod_register_service(gw, &mod_gwmanage, &dcp_svc);
			if (ret != TRUE)
			{
				mod_unregister(gw, &mod_gwmanage);
				gw_mutex_destroy(mod_gwmanage.lock);
				gw_msgq_destroy(dcp_msgq);
				gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_gwmanage.name);
				return ret;
			}
			ret = mod_register_service(gw, &mod_gwmanage, &dmp_svc);
			if (ret != TRUE)
			{
				mod_unregister(gw, &mod_gwmanage);
				gw_mutex_destroy(mod_gwmanage.lock);
				gw_msgq_destroy(dmp_msgq);
				gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_gwmanage.name);
				return ret;
			}
			gw_log_print(GW_LOG_LEVEL_INFO, "module %s initialized\n", mod_gwmanage.name);
		}
	}

	//send_msglist_init();

	send_thread = gw_thread_create(send_handler, (void *) gw);
	if (send_thread == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialized failed, thread create error!\n", mod_gwmanage.name);
		cleanup(gw);
		return FALSE;
	}

	dcp_recv_thread = gw_thread_create(dcp_recv_handler, (void *) gw);
	if (dcp_recv_thread == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialized failed, thread create error!\n", mod_gwmanage.name);
		return FALSE;
	}

	dmp_recv_thread = gw_thread_create(dmp_recv_handler, (void *) gw);
	if (dmp_recv_thread == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialized failed, thread create error!\n", mod_gwmanage.name);
		return FALSE;
	}
	return TRUE;
}


bool_t cleanup(gw_t *gw)
{
	//TODO: cleanup module stuff
	bool_t ret;
	gw_msg_t *msg;

	int32_t fifofd;
	int8_t cmd[16];

	json_object *my_object;
	my_object = serialize(gw);
	printf("my_object=\n");
	json_object_object_foreach(my_object, key, val) {
	printf("\t%s: %s\n", key, json_object_to_json_string(val));
	}
	printf("my_object.to_string()=%s\n", json_object_to_json_string(my_object));
	json_object_put(my_object);

	fifofd = open(gwman_fifo, O_RDWR | O_NONBLOCK, 0);
	if (fifofd == -1)
	{
		gw_debug_print("open!!!");
	}else
	{
		write(fifofd, cmd, sizeof(cmd));
		close(fifofd);
		gw_thread_wait(send_thread);
	}

	msg = gw_msg_alloc();
	msg->type = GW_MSG_SYSTEM_QUIT;
	msg->dstsvc = dcp_svc.id;
	msg->srcsvc = dcp_svc.id;
	gw_msg_send(gw, msg);
	gw_thread_wait(dcp_recv_thread);

	msg = gw_msg_alloc();
	msg->type = GW_MSG_SYSTEM_QUIT;
	msg->dstsvc = dmp_svc.id;
	msg->srcsvc = dmp_svc.id;
	gw_msg_send(gw, msg);
	gw_thread_wait(dmp_recv_thread);

	mod_unregister_service(gw, &mod_gwmanage, &dcp_svc);
	mod_unregister_service(gw, &mod_gwmanage, &dmp_svc);
	mod_unregister(gw, &mod_gwmanage);

	//send_msglist_destroy();
	gw_msgq_destroy(dcp_msgq);
	gw_msgq_destroy(dmp_msgq);
	gw_mutex_destroy(mod_gwmanage.lock);
	return TRUE;
}

json_object *serialize(gw_t *gw)
{
	json_object *output = NULL;
	time_t ntime;
	output = json_object_new_object();
	json_object_object_add(output, "GATEWAY_UPDATE_PACKET_NUM",
			json_object_new_int(gwmanage_stats.info_update_send_num));
	if(gw->data->gw_state == STA_REG_TO_PLAT)
	{
		json_object_object_add(output, "GATEWAY_LOG_STATUS",
				json_object_new_string("REGISTERED"));
		json_object_object_add(output, "LAST_UPDATE_TIME", json_object_new_string(
				ctime(&gwmanage_stats.last_update_time)));
		ntime = time(NULL);
		json_object_object_add(output, "TIME_LEFT_TO_UPDATE", json_object_new_int(
				gwmanage_conf.info_update_interval - difftime(ntime,
						gwmanage_stats.last_update_time)));
	}
	else
	{
		json_object_object_add(output, "GATEWAY_LOG_STATUS",
				json_object_new_string("UNREGISTERED"));
		json_object_object_add(output, "LAST_UPDATE_TIME", json_object_new_string(
				"0"));
		json_object_object_add(output, "TIME_LEFT_TO_UPDATE", json_object_new_int(
				gwmanage_conf.info_update_interval));
	}

	return output;
}


