/**
 * @brief locate application module
 *
 * @file
 * @author lilei (2010-12-31)
 *
 */
#include <module.h>
#include <dotconf/dotconf.h>
#include <arpa/inet.h>
#include <libevent/event.h>
#include <json/json.h>

/* module skeleton config information */
typedef struct __tag_applocate_config
{
	bstring plat_ip;
	int32_t serv_port;
	int32_t client_port;
}applocate_conf_t;

typedef struct __tag_applocate_stats
{
	uint64_t upstream_data_num;
	uint64_t downstream_data_num;
}applocate_stats_t;

static int8_t serv_ip[] = "10.13.16.252";
#define	SERV_PORT		5432
#define	CLIENT_PORT		2345
#define SEND_BUFFER_LEN 1024
#define RECV_BUFFER_LEN 1024
#define	SA	struct sockaddr

static gw_mod_t mod_applocate;
static gw_mod_data_t applocate_data;
static gw_svc_t applocate_svc;
static gw_msgq_t *applocate_msgq;
static gw_thread_t msg_handle_thread = NULL;
static applocate_conf_t applocate_conf;
static applocate_stats_t applocate_stats;

bool_t init(gw_t *gw, gw_dlib_t lib);
bool_t cleanup(gw_t *gw);
json_object *serialize(gw_t *gw);
static DOTCONF_CB(default_callback);

static const configoption_t options[] = {
	{"server_port", ARG_INT, default_callback, NULL, 0},
	{"client_port", ARG_INT, default_callback, NULL, 0},
	{"", ARG_NAME, default_callback, NULL, 0},
	LAST_OPTION
};

static int32_t load_config(int8_t *path_filename)
{
	configfile_t *configfile;

	if (path_filename == NULL) {
		printf("error: NULL input\n");
		return 1;
	}
	configfile = dotconf_create(path_filename,
				    options, NULL, CASE_INSENSITIVE);
	if (!configfile) {
		fprintf(stderr, "Error creating configfile\n");
		return 1;
	}

	if (dotconf_command_loop(configfile) == 0) {
		fprintf(stderr, "Error parsing config file %s\n",configfile->filename);
		return 1;
	}

	dotconf_cleanup(configfile);

	return 0;
}

static int32_t config_cleanup(void)
{
	int32_t ret;
	ret = bdestroy(applocate_conf.plat_ip);
	return ret;
}

DOTCONF_CB(default_callback)
{
	int32_t i, arg_count;
	bstring command;
	bstring tmpcmd;
	int8_t *ret = NULL;

	gw_debug_print("default_callback handler called for \"%s\". Got %d args\n", cmd->name,
	       cmd->arg_count);
	for (i = 0; i < cmd->arg_count; i++)
		gw_debug_print("\tArg #%d: %s\n", i, cmd->data.list[i]);

	command = bfromcstr(cmd->name);
	arg_count = cmd->arg_count;
	tmpcmd = bfromcstr("server_ip");
	if ( bstricmp(command, tmpcmd) == 0 ) {
		//skel_conf.ip_address = bfromcstr(cmd->data.list[0]);
		applocate_conf.plat_ip = bfromcstr(cmd->data.list[0]);
		gw_debug_print("the ip address is %s\n", applocate_conf.plat_ip->data);
	}

	bassigncstr(tmpcmd, "server_port");
	if ( bstricmp(command, tmpcmd) == 0 )
	{
		//skel_conf.port = cmd->data.value;
		applocate_conf.serv_port = cmd->data.value;
		gw_debug_print("the server port is set to %d\n", applocate_conf.serv_port);
	}

	bassigncstr(tmpcmd, "client_port");
	if ( bstricmp(command, tmpcmd) == 0 )
	{
		//skel_conf.port = cmd->data.value;
		applocate_conf.client_port = cmd->data.value;
		gw_debug_print("the client port is set to %d\n", applocate_conf.client_port);
	}
	bdestroy(command);
	bdestroy(tmpcmd);
}

static void applocate_config_cleanup(void)
{
	if(applocate_conf.plat_ip != NULL)
	{
		bdestroy(applocate_conf.plat_ip);
	}
	memset(&applocate_conf, 0, sizeof(applocate_conf));
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
	printf("mod app generate an ack to south interface!\n");
	return rsp_msg;
}

void *applocate_msg_handler(void *args)
{
	gw_msg_t *msg, *rsp_msg;
	bool_t quit = FALSE;
	gw_t *gw = (gw_t *)args;
	int8_t send_buff[SEND_BUFFER_LEN];
	struct sockaddr_in serv_addr;
	int32_t send_num;
	int32_t send_sock;
	bool_t ret;

	/***** initialize send socket *****/
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(applocate_conf.serv_port);
	inet_pton(AF_INET, applocate_conf.plat_ip->data, &serv_addr.sin_addr);
	send_sock = socket(AF_INET, SOCK_DGRAM, 0);

	while (!quit && ((msg = gw_msg_recv(applocate_msgq)) != NULL))
	{
		ret = TRUE;
		switch (msg->type)
		{
		case GW_MSG_SYSTEM_QUIT:
		{
			quit = TRUE;
			gw_debug_print("applocate module thread quit!\n");
			break;
		}
		case UPSTREAM_DATA:
		{
			if(msg->srcsvc == SN_DATA_SVC)
			{
				gw_debug_print("send a message to the LOCATE APPLICATION SERVER!\n");
				applocate_stats.upstream_data_num ++;
				memcpy(send_buff, msg->data, msg->datasize);
				sendto(send_sock, send_buff, msg->datasize, 0, (SA *) &serv_addr,
							sizeof(serv_addr));
				printf("mod app start to generate ack message!\n");
				rsp_msg = gen_ack_msg(msg, EC_SUCCESS);
				printf("dst svc id is %d!!!!!!!!!!!!!!!!!!\n",rsp_msg->dstsvc);
				ret = gw_msg_send(gw, rsp_msg);
				if(ret == FALSE)
				{
					gw_debug_print("sent msg to sn error!!\n");
					gw_msg_free(rsp_msg);
				}
			}
			else
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "Locate application receive an unknow msg!\n");
			}
			break;
		}
		case UPSTREAM_DATA_ACK:
		{
			gw_debug_print("Locate application receive a upstream data ack, error code is %d!\n", msg->retcode);
			break;
		}
		default:
			gw_log_print(GW_LOG_LEVEL_WARNING, "unknown message type %u received by service %u, module %s\n", \
					msg->type, applocate_svc.id, mod_applocate.name);
			break;
		}
		gw_msg_free(msg);
	}
	close(send_sock);
	return NULL;
}

void *applocate_net_handler(void *args)
{
	gw_msg_t *msg;
	bool_t quit = FALSE;
	int32_t recv_sock;
	struct sockaddr_in host_addr;
	int8_t recv_buff[RECV_BUFFER_LEN];
	struct sockaddr_in tmp_cli_addr;
	int32_t tmp_cli_len = sizeof(tmp_cli_addr);
	int32_t recv_num;
	int32_t send_serial;
	gw_t *gw = (gw_t *)args;

	gw_thread_detach(gw_thread_self());

	srand((int32_t)&send_serial);
	send_serial = rand();
	recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&host_addr, sizeof(host_addr));
	host_addr.sin_family = AF_INET;
	host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	host_addr.sin_port = htons(applocate_conf.client_port);
	bind(recv_sock, (SA *) &host_addr, sizeof(host_addr));

	while(1)
	{
		recv_num = recvfrom(recv_sock, recv_buff, RECV_BUFFER_LEN, 0,
				(SA *) &tmp_cli_addr, &tmp_cli_len);
		gw_debug_print("Locate application module receive a packet from locate server, pack len %d\n", recv_num);
		if (recv_num > 0)
		{
			applocate_stats.downstream_data_num ++;
			msg = gw_msg_alloc();
			msg->version.major = 1;
			msg->version.minor = 0;
			msg->type = UPSTREAM_DATA;
			msg->srcsvc = GW_LOCATE_APP;
			msg->dstsvc = PLAT_DATA_SVC;
			msg->serial = send_serial ++;
			memcpy(msg->devid, gw->config->nui->data, NUI_LEN);
			msg->datasize = recv_num;
			msg->data = malloc(msg->datasize);
			memcpy(msg->data, recv_buff, msg->datasize);
			gw_msg_send(gw, msg);
		}
		else
		{
			gw_log_print(GW_LOG_LEVEL_ERROR, "Locate application module network error!\n");
		}
	}

	return NULL;
}
/**
 * @brief Initialize this module.
 *
 * Call this function when loading this module.
 *
 * @param gw	pointer to gateway context
 * @param lib	pointer to dynamic library generated by this module
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t init(gw_t *gw, gw_dlib_t lib)
{
	bool_t ret = TRUE;
	gw_thread_t net_handle_thread = NULL;

	/* initialize module data */
	memset(&applocate_conf, 0, sizeof(applocate_conf));
	applocate_conf.plat_ip = bfromcstr(serv_ip);
	applocate_conf.serv_port = SERV_PORT;
	applocate_conf.client_port = CLIENT_PORT;
	memset(&applocate_stats, 0, sizeof(applocate_stats));
	memset(&applocate_data, 0, sizeof(applocate_data));
	applocate_data.config = (void *)&applocate_conf;
	applocate_data.stats = (void *)&applocate_stats;

	/* read config infromation */
	//load_config("/etc/gw/modules/applocate.conf");

	/* initialize module structure */
	memset(&mod_applocate, 0, sizeof(mod_applocate));
	mod_applocate.name = "mod_applocate";
	mod_applocate.desc = "mod locate application";
	mod_applocate.vendor = "WSN";
	mod_applocate.modver.major = 1;
	mod_applocate.modver.minor = 0;
	mod_applocate.sysver.major = 1;
	mod_applocate.sysver.minor = 0;
	mod_applocate.init = init;
	mod_applocate.cleanup = cleanup;
	mod_applocate.serialize = serialize;
	mod_applocate.lib = lib;
	mod_applocate.data = &applocate_data;
	mod_applocate.lock = gw_mutex_create();
	INIT_LIST_HEAD(&mod_applocate.services);

	applocate_msgq = gw_msgq_create();
	memset(&applocate_svc, 0, sizeof(applocate_svc));
	applocate_svc.id = GW_LOCATE_APP;
	applocate_svc.name = "applocate_service";
	applocate_svc.mod = &mod_applocate;
	applocate_svc.mq = applocate_msgq;

	//TODO: initialize another stuff
	if (mod_applocate.lock != NULL)
	{
		ret = mod_register(gw, &mod_applocate);
		if (ret)
		{
			ret = mod_register_service(gw, &mod_applocate, &applocate_svc);
			if (ret != TRUE)
			{
				mod_unregister(gw, &mod_applocate);
				gw_mutex_destroy(mod_applocate.lock);
				gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_applocate.name);
			}
			else
			{
				gw_log_print(GW_LOG_LEVEL_INFO, "module %s initialized\n", mod_applocate.name);
			}
		}
	}

	msg_handle_thread = gw_thread_create(applocate_msg_handler, (void *)gw);
	net_handle_thread = gw_thread_create(applocate_net_handler, (void *)gw);
	if(net_handle_thread == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Thread create error!\n");
		ret = FALSE;
	}
	else
	{
		gw_thread_close(net_handle_thread);
	}
	return ret;
}

/**
 * @brief Do the cleanup for this module.
 *
 * Call this function when unloading this module.
 *
 * @param gw pointer to gateway context
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t cleanup(gw_t *gw)
{
	//TODO: cleanup module stuff
	gw_msg_t *msg;

	json_object *my_object;
	my_object = serialize(gw);
	printf("my_object=\n");
	json_object_object_foreach(my_object, key, val) {
	printf("\t%s: %s\n", key, json_object_to_json_string(val));
	}
	printf("my_object.to_string()=%s\n", json_object_to_json_string(my_object));
	json_object_put(my_object);

	msg = gw_msg_alloc();
	msg->type = GW_MSG_SYSTEM_QUIT;
	msg->dstsvc = applocate_svc.id;
	msg->srcsvc = applocate_svc.id;
	gw_msg_send(gw, msg);

	gw_thread_wait(msg_handle_thread);

	mod_unregister_service(gw, &mod_applocate, &applocate_svc);
	mod_unregister(gw, &mod_applocate);

	gw_msgq_destroy(applocate_msgq);
	gw_mutex_destroy(mod_applocate.lock);

	applocate_config_cleanup();

	return TRUE;
}

/**
 * @brief Output status information of this module.
 *
 * @param gw	pointer to gateway context
 * @param buf	pointer to output buffer
 *
 * @return TRUE on success, FALSE on failure
 */
json_object *serialize(gw_t *gw)
{
	json_object *my_object = NULL;
	int8_t buff[100];
	int64_t tnum;
	my_object = json_object_new_object();
	memset(buff, 0, 100);
	sprintf(buff, "%lld", applocate_stats.downstream_data_num);
	json_object_object_add(my_object, "DOWNSTREAM_PACKET_NUM",
			json_object_new_string(buff));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", applocate_stats.upstream_data_num);
	json_object_object_add(my_object, "UPSTREAM_PACKET_NUM",
			json_object_new_string(buff));
	return my_object;
}

