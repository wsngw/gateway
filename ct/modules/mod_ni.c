/*
 * mod_ni.c
 *
 *  Created on: 2010-11-15
 *      Author: lilei
 *   Copyright: 2010, WSN
 */
//This is the DCP/DMP module

#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <module.h>
#include <dotconf/dotconf.h>
#include <libevent/event.h>
#include <json/json.h>
#include <time.h>
#include <sys/time.h>

#define	SA	struct sockaddr

#define SEND_BUFFER_LEN 4096
#define RECV_BUFFER_LEN 4096


//define dcp type
#define DCP_HEAD_LEN 		24
#define DCP_VERSION 		0x01
#define GW_REPORT 			0x01
#define GW_CONFIRM 			0x02
#define PT_REQUEST 			0x03
#define PT_RESPONCE 		0x04
//define dmp type
#define DEV_JOIN_REQ 		0x01
#define DEV_LEAVE_REQ 		0x02
#define DEV_INFO_UPDATE_REQ 0x03
#define DEV_DMP_REPLY 		0x04
#define DMP_HEAD_LEN 		28
#define DMP_VERSION 		0x01

#define APP_ID				52
#define SP_ID				44

#define MAX(a,b) ((a)>(b))?(a):(b)
#define MIN(a,b) ((a)<(b))?(a):(b)
/***** record all the msgs already send, released when receive a response form platform *******/
typedef struct __tag_ni_msglist
{
	struct list_head header;
	gw_mutex_t mutex;
	int32_t num;
} ni_msglist_t;

/***** msg already send *******/
typedef struct __tag_ni_msg
{
	struct list_head entry;
	gw_msg_t *msg;
	struct event *timeout;
	gw_t *gw;
	int32_t cnt;
	int32_t interval;
	int32_t elapse;
} ni_msg_t;

/***** north interface config information *******/
typedef struct __tag_ni_config
{
	bstring pf_ip;

	/***** DMP info *****/
	int32_t dmp_serv_port;
	int32_t dmp_cli_port;
	int32_t join_init_time;
	int32_t join_max_time;
	int32_t join_max_cnt;
	int32_t leave_init_time;
	int32_t leave_max_time;
	int32_t leave_max_cnt;
	int32_t infoupdate_init_time;
	int32_t infoupdate_max_time;
	int32_t infoupdate_max_cnt;

	/***** DCP info *****/
	int32_t dcp_serv_port;
	int32_t dcp_cli_port;
	int32_t report_init_time;
	int32_t report_max_time;
	int32_t report_max_cnt;
} ni_conf_t;

/***** north interface config information *******/
typedef struct __tag_ni_stats
{
	/***** dcp status ****/
	int64_t dcp_send_num;
	int64_t dcp_success_num;
	int64_t dcp_fail_num;
	int32_t dcp_wait_num;
	int64_t dcp_recv_num;
	/***** dmp status *****/
	int64_t dmp_send_num;
	int64_t dmp_success_num;
	int64_t dmp_fail_num;
	int32_t dmp_wait_num;
} ni_stats_t;

/* retransmite parameters */
typedef struct __tag_trans_time_param
{
	int32_t init_trans_time;
	int32_t max_trans_time;
	int32_t max_trans_cnt;
}time_param_t;

/* arg parameter for libevent */
typedef struct __tag_event_cb_arg{
	ni_msg_t *ni_msg;
	gw_t *gw;
}event_cb_arg_t;

static ni_msglist_t msg_list; //north interface msg list
static gw_mod_t mod_ni; //north interface module basic information
static gw_svc_t dcp_svc; //north interface module dcp service
static gw_svc_t dmp_svc; //north interface module dmp service
static gw_mod_data_t ni_data; //north interface module private data
static gw_msgq_t *ni_msgq; //north interface module msg queue
static ni_conf_t ni_conf;
static ni_stats_t ni_stats;
static const int8_t *ni_fifo = "/tmp/ni_event.fifo";
static gw_thread_t send_thread, recv_thread;
static int32_t send_dcp_sock;
static int32_t send_dmp_sock;
static struct event_base *ev_base;
static gw_mutex_t ev_lock;

bool_t init(gw_t *gw, gw_dlib_t lib);
bool_t cleanup(gw_t *gw);
json_object *serialize(gw_t *gw);

static DOTCONF_CB(default_callback);

static const configoption_t options[] =
{
{ "", ARG_NAME, default_callback, NULL, 0 },
LAST_OPTION };

static int load_config(char *path_filename)
{
	configfile_t *configfile;

	if (path_filename == NULL)
	{
		gw_debug_print("error: NULL input\n");
		return 1;
	}
	configfile = dotconf_create(path_filename, options, NULL, CASE_INSENSITIVE);
	if (!configfile)
	{
		gw_debug_print(stderr, "Error creating configfile\n");
		return 1;
	}

	if (dotconf_command_loop(configfile) == 0)
	{
		gw_debug_print(stderr, "Error parsing config file %s\n",configfile->filename);
		return 1;
	}

	dotconf_cleanup(configfile);

	return 0;
}

DOTCONF_CB(default_callback)
{
	int i, arg_count;
	bstring command;
	bstring tmpcmd;

	command = bfromcstr(cmd->name);
	arg_count = cmd->arg_count;
	tmpcmd = bfromcstr("platform_ip_address");
	if (bstricmp(command, tmpcmd) == 0)
	{
		ni_conf.pf_ip = bfromcstr(cmd->data.list[0]);
		gw_debug_print("the platform ip address is %s\n", ni_conf.pf_ip->data);
	}
	else if ((!bassigncstr(tmpcmd, "dmp_server_port")) && (!bstricmp(command,
			tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.dmp_serv_port);
		gw_debug_print("the dmp server port is set to %d\n", ni_conf.dmp_serv_port);
	}
	else if ((!bassigncstr(tmpcmd, "dmp_client_port")) && (!bstricmp(command,
			tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.dmp_cli_port);
		gw_debug_print("the dmp client port is set to %d\n", ni_conf.dmp_cli_port);
	}
	else if ((!bassigncstr(tmpcmd, "join_initial_retrans_time")) && (!bstricmp(
			command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.join_init_time);
		gw_debug_print("the dmp join initial retransmission time is %d\n", ni_conf.join_init_time);
	}
	else if ((!bassigncstr(tmpcmd, "join_max_retrans_time")) && (!bstricmp(
			command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.join_max_time);
		gw_debug_print("the dmp join max retransmission time is %d\n", ni_conf.join_max_time);
	}
	else if ((!bassigncstr(tmpcmd, "join_max_retrans_count")) && (!bstricmp(
			command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.join_max_cnt);
		gw_debug_print("the dmp join max retransmission count is %d\n", ni_conf.join_max_cnt);
	}
	else if ((!bassigncstr(tmpcmd, "leave_initial_retrans_time"))
			&& (!bstricmp(command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.leave_init_time);
		gw_debug_print("the dmp leave request retransmission time is %d\n", ni_conf.leave_init_time);
	}
	else if ((!bassigncstr(tmpcmd, "leave_max_retrans_time")) && (!bstricmp(
			command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.leave_max_time);
		gw_debug_print("the dmp leave request max retransmission time is %d\n", ni_conf.leave_max_time);
	}
	else if ((!bassigncstr(tmpcmd, "leave_max_retrans_count")) && (!bstricmp(
			command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.leave_max_cnt);
		gw_debug_print("the dmp leave request max retransmission count is %d\n", ni_conf.leave_max_cnt);
	}
	else if ((!bassigncstr(tmpcmd, "info_update_initial_retrans_time"))
			&& (!bstricmp(command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.infoupdate_init_time);
		gw_debug_print("the dmp info update retransmission time is %d\n", ni_conf.infoupdate_init_time);
	}
	else if ((!bassigncstr(tmpcmd, "info_update_max_retrans_time"))
			&& (!bstricmp(command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.infoupdate_max_time);
		gw_debug_print("the dmp max info update retransmission time is %d\n", ni_conf.infoupdate_max_time);
	}
	else if ((!bassigncstr(tmpcmd, "info_update_max_retrans_count"))
			&& (!bstricmp(command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.infoupdate_max_cnt);
		gw_debug_print("the dmp max info update retransmission count is %d\n", ni_conf.infoupdate_max_cnt);
	}
	else if ((!bassigncstr(tmpcmd, "dcp_server_port")) && (!bstricmp(command,
			tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.dcp_serv_port);
		gw_debug_print("the dcp server port is set to %d\n", ni_conf.dcp_serv_port);
	}
	else if ((!bassigncstr(tmpcmd, "dcp_client_port")) && (!bstricmp(command,
			tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.dcp_cli_port);
		gw_debug_print("the dcp client port is set to %d\n", ni_conf.dcp_cli_port);
	}
	else if ((!bassigncstr(tmpcmd, "report_initial_retrans_time"))
			&& (!bstricmp(command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.report_init_time);
		gw_debug_print("the dcp report initial retransmission time is %d\n", ni_conf.report_init_time);
	}
	else if ((!bassigncstr(tmpcmd, "report_max_retrans_time")) && (!bstricmp(
			command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.report_max_time);
		gw_debug_print("the dcp report max retransmission time is %d\n", ni_conf.report_max_time);
	}
	else if ((!bassigncstr(tmpcmd, "report_max_retrans_count")) && (!bstricmp(
			command, tmpcmd)))
	{
		sscanf(cmd->data.list[0], "%d", &ni_conf.report_max_cnt);
		gw_debug_print("the dcp report max retransmission count is %d\n", ni_conf.report_max_cnt);
	}

	bdestroy(command);
	bdestroy(tmpcmd);

	return NULL;
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

static bool_t get_time_var(time_param_t *ret, uint16_t type)
{
	switch (type)
	{
	case DEVICE_LOGIN_REQUEST:
	{
		ret->init_trans_time = ni_conf.join_init_time;
		ret->max_trans_time = ni_conf.join_max_time;
		ret->max_trans_cnt = ni_conf.join_max_cnt;
		break;
	}
	case DEVICE_LOGOUT_REQUEST:
	{
		ret->init_trans_time = ni_conf.leave_init_time;
		ret->max_trans_time = ni_conf.leave_max_time;
		ret->max_trans_cnt = ni_conf.leave_max_cnt;
		break;
	}
	case DEVICE_UPDATE_REQUEST:
	{
		ret->init_trans_time = ni_conf.infoupdate_init_time;
		ret->max_trans_time = ni_conf.infoupdate_max_time;
		ret->max_trans_cnt = ni_conf.infoupdate_max_cnt;
		break;
	}
	case UPSTREAM_DATA:
	{
		ret->init_trans_time = ni_conf.report_init_time;
		ret->max_trans_time = ni_conf.report_max_time;
		ret->max_trans_cnt = ni_conf.report_max_cnt;
		break;
	}
	default:
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "unknown msg type %d!\n", type);
		break;
	}
	}
}

/* create a list to record the message already sent to platform */
static bool_t ni_msglist_init(void)
{
	bool_t ret = TRUE;
	msg_list.num = 0;
	INIT_LIST_HEAD(&(msg_list.header));
	msg_list.mutex = gw_mutex_create();
	if (msg_list.mutex == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "North interface msg list init error, can not create mutex!\n");
		ret = FALSE;
	}
	return ret;
}

static bool_t ni_msglist_add(gw_msg_t *msg, gw_t *gw)
{
	bool_t ret;
	ni_msg_t *ni_msg;
	time_param_t tpm;
	struct timeval tv;
	get_time_var(&tpm, msg->type);
	ni_msg = (ni_msg_t *) malloc(sizeof(ni_msg_t));
	if(ni_msg != NULL)
	{
		ni_msg->cnt = 0;
		ni_msg->msg = msg;
		ni_msg->timeout = (struct event*)malloc(sizeof(struct event));
		ni_msg->gw  =  gw;
		srand(time(NULL));
		ni_msg->interval = tpm.init_trans_time;
		ni_msg->elapse = 0;
		gw_mutex_lock(msg_list.mutex);
		list_add_tail(&(ni_msg->entry), &(msg_list.header));
		msg_list.num++;
		gw_mutex_unlock(msg_list.mutex);

		
		event_assign(ni_msg->timeout, ev_base, -1, 0, timeout_cb, (void *)ni_msg);
		tv.tv_sec = ni_msg->interval;
		tv.tv_usec = 0;
		gw_mutex_lock(ev_lock);
		event_add(ni_msg->timeout, &tv);
		gw_mutex_unlock(ev_lock);
		ret = TRUE;
	}
	else
	{
		gw_debug_print("Ni module add msg list error!\n");
		ret = FALSE;
	}
	return ret;
}

static ni_msg_t *ni_msglist_get(gw_msg_t *msg)
{
	bool_t ret;
	ni_msg_t *nimsg = NULL;
	struct list_head *pos, *tmp_pos;
	bool_t match = FALSE;
	list_for_each_safe(pos, tmp_pos, &msg_list.header)
	{
		nimsg = list_entry(pos, ni_msg_t, entry);
		if (((nimsg->msg->dstsvc) == msg->srcsvc)
				&& (nimsg->msg->serial == msg->serial) && (!memcmp(
						nimsg->msg->devid, msg->devid, NUI_LEN)))
		{
			match = TRUE;
			break;
		}
	}

	if (match == FALSE)
	{
		gw_debug_print("Receive a mismatch confirm msg!\n");
		nimsg = NULL;
	}
	return nimsg;
}

static void ni_msglist_del(ni_msg_t *nimsg)
{
	gw_mutex_lock(ev_lock);
	event_del(nimsg->timeout);
	gw_mutex_unlock(ev_lock);
	free(nimsg->timeout);
	gw_mutex_lock(msg_list.mutex);
	list_del(&nimsg->entry);
	msg_list.num--;
	gw_mutex_unlock(msg_list.mutex);
}

/* destroy the list recording messages already sent to platform */
static bool_t ni_msglist_destroy(void)
{
	bool_t ret;
	struct list_head *pos, *tmp;
	ni_msg_t *nimsg;
	gw_debug_print("ni module destroy send list!\n");
	/***** check if the list still have msgs left *****/
	if (msg_list.num != 0)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING,"mod NI send list not empty!\n");
		list_for_each_safe(pos, tmp, &msg_list.header)
		{
			nimsg = list_entry(pos, ni_msg_t, entry);
			list_del(&nimsg->entry);
			msg_list.num--;
			gw_msg_free(nimsg->msg);
			free(nimsg);
			nimsg = NULL;
		}
	}
	ret = gw_mutex_destroy(msg_list.mutex);
	return ret;
}

/* calculate a crc code for a buffer */
static uint16_t crc_cal(int8_t *send_buff, uint32_t len)
{
	uint32_t data_len;
	uint32_t sum = 0;
	int32_t i;
	uint8_t *read_pt, *wt_pt;
	uint16_t checksum;
	data_len = len - 2;

	for (i = 0; i < (data_len - 1); i += 2)
	{
		read_pt = send_buff + i;
		sum += (uint32_t) (((*read_pt) << 8) & 0xff00) + (uint32_t) ((*(read_pt
				+ 1)) & 0x00ff);
	}

	if (data_len % 2)
	{
		//packet length is odd
		sum += (uint32_t) (*(send_buff + data_len - 1));
	}
	while (((sum >> 16) & 0xffff) != 0)
		sum = ((sum >> 16) & 0xffff) + (sum & 0xffff);
	checksum = ~(sum & 0xffff);

	return checksum;
}

/**
 * @brief fill send buffer with a dcp msg.
 *
 * @param msg pointer to the msg to be send
 * @param buff pointer to the send buffer
 * @param size send buffer length
 *
 * @return number of bytes write to send buffer, -1 on failure
 */
int32_t dcp_write_to_buffer(gw_msg_t *msg, int8_t *buff, int32_t size)
{
	int8_t *wt_pt = buff;
	int32_t i;
	uint16_t checksum;
	uint16_t pack_len;
	uint8_t pack_type;
	uint16_t pack_app_id, pack_sp_id;
	int32_t ret;
	uint8_t stat_code;

	switch (msg->type)
	{
	case UPSTREAM_DATA:
	{
		pack_len = DCP_HEAD_LEN + msg->datasize;
		pack_type = GW_REPORT;
		pack_app_id = (uint16_t)APP_ID;
		pack_sp_id = (uint16_t)SP_ID;
		stat_code = msg->retcode;
		if (size < pack_len)
		{
			gw_log_print(GW_LOG_LEVEL_ERROR, "dcp msg write to send buffer error, buffer overflow!\n");
			ret = -1;
		}
		else
		{
			wt_pt = buff + DCP_HEAD_LEN;
			memcpy(wt_pt, msg->data, msg->datasize);
			ret = pack_len;
		}
		break;
	}
	case DOWNSTREAM_DATA_ACK:
	{
		pack_len = DCP_HEAD_LEN;
		pack_type = PT_RESPONCE;
		pack_app_id = (uint16_t)APP_ID;
		pack_sp_id = (uint16_t)SP_ID;
		stat_code = msg->retcode;
		ret = pack_len;
		break;
	}
	default:
	{
		gw_debug_print("Ni module dcp write to buffer error, unknow msg type!\n");
		ret = -1;
		break;
	}
	}
	if (ret != -1)
	{
		wt_pt = buff;
		*(wt_pt++) = DCP_VERSION;
		*(wt_pt++) = (pack_len >> 8) & 0xff;
		*(wt_pt++) = pack_len & 0xff;
		*(wt_pt++) = pack_type;
		*(wt_pt++) = (msg->serial >> 24) & 0xff;
		*(wt_pt++) = (msg->serial >> 16) & 0xff;
		*(wt_pt++) = (msg->serial >> 8) & 0xff;
		*(wt_pt++) = (msg->serial) & 0xff;
		for (i = 0; i < 8; i++)
		{
			*(wt_pt++) = msg->devid[i];
		}
		*(wt_pt++) = (pack_app_id >> 8) & 0xff;
		*(wt_pt++) = (pack_app_id) & 0xff;
		*(wt_pt++) = (pack_sp_id >> 8) & 0xff;
		*(wt_pt++) = (pack_sp_id) & 0xff;
		*(wt_pt++) = stat_code;
		*(wt_pt++) = 0;
		checksum = crc_cal(buff, DCP_HEAD_LEN);
		wt_pt = buff + DCP_HEAD_LEN - 2;
		*(wt_pt++) = (checksum >> 8) & 0xff;
		*(wt_pt++) = (checksum) & 0xff;
	}
	return ret;
}

/**
 * @brief fill send buffer with a dmp msg.
 *
 * @param msg pointer to the msg to be send
 * @param buff pointer to the send buffer
 * @param size send buffer length
 *
 * @return number of bytes write to send buffer, -1 on failure
 */
int32_t dmp_write_to_buffer(gw_msg_t *msg, int8_t *buff, int32_t size)
{
	int8_t *wt_pt = buff;
	int32_t i;
	uint16_t checksum;
	uint16_t pack_len;
	uint8_t pack_type;
	int32_t ret;

	switch(msg->type)
	{
	case DEVICE_LOGIN_REQUEST:
	{
		pack_len = DMP_HEAD_LEN+14;
		pack_type = DEV_JOIN_REQ;
		ret = pack_len;
		wt_pt = buff + DMP_HEAD_LEN;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x01;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x02;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x02;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x04;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x38;
		*(wt_pt++) = 0x40;
		break;
	}
	case DEVICE_LOGOUT_REQUEST:
	{
		pack_len = DMP_HEAD_LEN + 6;
		pack_type = DEV_LEAVE_REQ;
		ret = pack_len;
		wt_pt = buff + DMP_HEAD_LEN;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x01;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x02;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x00;
		break;
	}
	case DEVICE_UPDATE_REQUEST:
	{
		pack_type = DEV_INFO_UPDATE_REQ;
		pack_len = DMP_HEAD_LEN + 14;
		ret = pack_len;
//		if(msg->datasize != 0)
//		{
//			pack_len = DMP_HEAD_LEN + msg->datasize - 2;
//			wt_pt = buff + DMP_HEAD_LEN;
//			memcpy(wt_pt, (msg->data+2), (msg->datasize-2));
//		}
		wt_pt = buff + DMP_HEAD_LEN;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x01;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x02;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x02;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x04;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x00;
		*(wt_pt++) = 0x38;
		*(wt_pt++) = 0x40;
		break;
	}
	default:
	{
		gw_debug_print("Ni module dcp write to buffer error, unknow msg type!\n");
		ret = -1;
		break;
	}
	}
	if(ret > 0)
	{
		wt_pt = buff;
		*(wt_pt++) = DMP_VERSION;
		*(wt_pt++) = (pack_len >> 8) & 0xff;
		*(wt_pt++) = pack_len & 0xff;
		*(wt_pt++) = (pack_type & 0xff);
		*(wt_pt++) = (msg->serial >> 24) & 0xff;
		*(wt_pt++) = (msg->serial >> 16) & 0xff;
		*(wt_pt++) = (msg->serial >> 8) & 0xff;
		*(wt_pt++) = (msg->serial) & 0xff;
		for (i = 0; i < 8; i++)
		{
			*(wt_pt++) = msg->devid[i];
		}
		memset(wt_pt, 0, 8);
		wt_pt += 8;
		*(wt_pt++) = 0;
		*(wt_pt++) = 0;
		checksum = crc_cal(buff, DMP_HEAD_LEN);
		wt_pt = buff + DMP_HEAD_LEN - 2;
		*(wt_pt++) = (checksum >> 8) & 0xff;
		*(wt_pt++) = (checksum) & 0xff;
	}
	return ret;
}

/* fill a msg with a received dcp packet */
static int32_t dcp_read_from_buffer(gw_msg_t *msg, int8_t *buff, int32_t size)
{
	int8_t *read_pt = buff;
	int32_t i;
	uint16_t checksum;
	int16_t pack_len;
	uint8_t pack_type;
	uint16_t app_id;
	uint16_t sp_id;
	checksum = (((*(read_pt + DCP_HEAD_LEN - 2)) << 8) & 0xff00) + ((*(read_pt
			+ DCP_HEAD_LEN - 1)) & 0x00ff);
	if (crc_cal(buff, DCP_HEAD_LEN) != checksum)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "crc check error! \n");
		return -1;
	}
	if (*(read_pt++) != DCP_VERSION)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "dcp version check error! received version is %d\n",*(read_pt-1));
		return -1;
	}
	pack_len = (((*read_pt) << 8) & 0xff00) + ((*(read_pt + 1)) & 0x00ff);
	read_pt += 2;
	pack_type = (*(read_pt++) & 0xff);
	msg->serial = (((*read_pt) << 24) & 0xff000000) + (((*(read_pt + 1)) << 16)
			& 0x00ff0000) + (((*(read_pt + 2)) << 8) & 0xff00) + (((*(read_pt
			+ 3))) & 0xff);
	read_pt += 4;
	msg->srcsvc = PLAT_DATA_SVC;
	for (i = 0; i < 8; i++)
	{
		msg->devid[i] = *(read_pt++);
	}
	app_id = (((*read_pt) << 8) & 0xff00) + ((*(read_pt + 1)) & 0x00ff);
	read_pt += 2;
	sp_id = (((*read_pt) << 8) & 0xff00) + ((*(read_pt + 1)) & 0x00ff);
	read_pt += 2;

	if (pack_type == GW_CONFIRM)
	{
		msg->type = UPSTREAM_DATA_ACK;
		read_pt = buff + 20;
		msg->retcode = *read_pt;
	}
	else if (pack_type == PT_REQUEST)
	{
		msg->type = DOWNSTREAM_DATA;
		msg->datasize = pack_len-DCP_HEAD_LEN;
		msg->data = malloc(msg->datasize);
		read_pt = buff + DCP_HEAD_LEN;
		memcpy(msg->data, (buff+DCP_HEAD_LEN), msg->datasize);
	}
	else
	{
		gw_debug_print("Ni module dcp read from buffer error, unknow msg type!\n");
	}
	if(app_id != APP_ID)
	{
		return 1;
	}
	else if(sp_id != SP_ID)
	{
		return 2;
	}else
	{
		return 0;
	}
//	return 0;
}

/* fill a msg with a received dmp packet */
static bool_t dmp_read_from_buffer(gw_msg_t *msg, int8_t *buff, int32_t size)
{
	int8_t *read_pt = buff;
	int32_t i;
	uint16_t checksum;
	int32_t pack_len;
	uint8_t pack_type;
	uint16_t oid;
	uint16_t olen;
	uint16_t tlv_num;
	uint8_t dmp_buff[500];
	checksum = (((*(read_pt + DMP_HEAD_LEN - 2)) << 8) & 0xff00) + ((*(read_pt
			+ DMP_HEAD_LEN - 1)) & 0x00ff);
	if (crc_cal(buff, DMP_HEAD_LEN) != checksum)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "crc check error, recv %x, cal %x!\n",checksum, crc_cal(buff, DMP_HEAD_LEN));
		return FALSE;
	}
	if (*(read_pt++) != DMP_VERSION)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "dmp version check error!\n");
		return FALSE;
	}
	pack_len = (((*read_pt) << 8) & 0xff00) + ((*(read_pt + 1)) & 0x00ff);
	if(pack_len < (DMP_HEAD_LEN + 6))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "dmp len check error!\n");
		return FALSE;
	}
	read_pt += 2;
	pack_type = (*(read_pt++) & 0xff);
	msg->serial = (((*read_pt) << 24) & 0xff000000) + (((*(read_pt + 1)) << 16)
			& 0x00ff0000) + (((*(read_pt + 2)) << 8) & 0xff00) + (((*(read_pt
			+ 3))) & 0xff);
	read_pt += 4;
	for (i = 0; i < 8; i++)
	{
		msg->devid[i] = *(read_pt++);
	}
	msg->srcsvc = PLAT_DM_SVC;
	if(pack_type == DEV_DMP_REPLY)
	{
		read_pt = buff + DMP_HEAD_LEN + 4;
		msg->retcode = (((*read_pt) << 8) & 0xff00)	+ ((*(read_pt + 1)) & 0x00ff);
		read_pt += 2;
		tlv_num = 0;
		msg->datasize = 0;
		uint8_t *wt_pt = dmp_buff+2;
		while((read_pt - buff) < pack_len)
		{
			tlv_num ++;
			oid = (((*read_pt) << 8) & 0xff00)	+ ((*(read_pt + 1)) & 0x00ff);
			read_pt += 2;
			if(oid == 0x0002)
			{
				read_pt += 2;
				uint32_t uptime =  (((*read_pt) << 24) & 0xff000000) + (((*(read_pt + 1)) << 16)
						& 0x00ff0000) + (((*(read_pt + 2)) << 8) & 0xff00) + (((*(read_pt
						+ 3))) & 0xff);
				read_pt += 4;
				uptime = uptime*1000/2;
				*(wt_pt++) = 0x01;
				*(wt_pt++) = 0x00;
				*(wt_pt++) = 0x04;
				*(wt_pt++) = 0x00;
				memcpy(wt_pt, &uptime, 4);
				wt_pt += 4;
				msg->datasize += 8;
			}else
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "unknow msg type!!!!!!!!\n");
			}
		}
		if(msg->datasize > 0)
		{
			msg->datasize += 2;
			msg->data = malloc(msg->datasize);
			memcpy(msg->data, &tlv_num, 2);
			memcpy((msg->data + 2), dmp_buff+2, (msg->datasize-2));
		}
	}else
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "dmp read error, unknown type!\n");
		return FALSE;
	}
	return TRUE;
}

/**
 * @brief Receive msg from msg queue, repack according to DCP/DMP protocol and send to network.
 *
 * Msg are not freed in this thread.
 *
 * @param args pointer to gateway context
 *
 * @return NULL
 */
void *ni_send_handler(void *args)
{
	gw_msg_t *msg, *rsp_msg;
	ni_msg_t *dcpmsg, *dmpmsg;
	uint8_t send_buff[SEND_BUFFER_LEN];
	struct sockaddr_in dcp_addr, dmp_addr;
	gw_t *gw = (gw_t *) args;
	int32_t send_num;
	ni_msg_t *nimsg;
	struct timeval tm;
	int32_t i;

	/***** initialize dcp and dmp send socket *****/
	bzero(&dcp_addr, sizeof(dcp_addr));
	dcp_addr.sin_family = AF_INET;
	dcp_addr.sin_port = htons(ni_conf.dcp_serv_port);
	inet_pton(AF_INET, ni_conf.pf_ip->data, &dcp_addr.sin_addr);

	bzero(&dmp_addr, sizeof(dmp_addr));
	dmp_addr.sin_family = AF_INET;
	dmp_addr.sin_port = htons(ni_conf.dmp_serv_port);
	inet_pton(AF_INET, ni_conf.pf_ip->data, &dmp_addr.sin_addr);

	while ((msg = gw_msg_recv(ni_msgq)) != NULL)
	{
		if (msg->type == GW_MSG_SYSTEM_QUIT)
		{
			gw_msg_free(msg);
			gw_debug_print("NI recv thread receive a quit message!\n");
			break; //quit
		}
		for (i = 0; i < 8; i++)
			{
				printf("%02x ",msg->devid[i]);
			}
		printf("\n");
		if (msg->dstsvc == PLAT_DATA_SVC)
		{
			/* dcp data can only be sent when gw is registed */
			if (gw->data->gw_state != STA_REG_TO_PLAT)
			{
				gw_debug_print("GW not registered yet, can not send dcp data!\n");
				rsp_msg = gen_ack_msg(msg, EC_DEV_UNREG);
				gw_msg_free(msg);
				gw_msg_send(gw, rsp_msg);
			}
			else if (msg->type == UPSTREAM_DATA)
			{
				gw_debug_print("mod ni has a gw report msg serial %d, src svc is %d!\n",msg->serial, msg->srcsvc);
				ni_msglist_add(msg, gw);
				memset(send_buff, 0, SEND_BUFFER_LEN);
				send_num = dcp_write_to_buffer(msg, send_buff, SEND_BUFFER_LEN);
				sendto(send_dcp_sock, send_buff, send_num, 0, (SA *) &dcp_addr,
						sizeof(dcp_addr));
				gettimeofday(&tm, NULL);
				printf("send time sec %u, usec %u\n",(uint32_t)tm.tv_sec,(uint32_t)tm.tv_usec/1000);
				ni_stats.dcp_send_num ++;
				ni_stats.dcp_wait_num ++;
			}
			else if(msg->type == DOWNSTREAM_DATA_ACK)
			{
				gw_debug_print("mod ni send a down stream ack data!\n");
				memset(send_buff, 0, SEND_BUFFER_LEN);
				send_num = dcp_write_to_buffer(msg, send_buff, SEND_BUFFER_LEN);
				sendto(send_dcp_sock, send_buff, send_num, 0, (SA *) &dcp_addr,
						sizeof(dcp_addr));
				gw_msg_free(msg);
			}
			else
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "unknown message type %u received by service %u, module %s\n",
						msg->type, dcp_svc.id, mod_ni.name);
				gw_msg_free(msg);
			}
		}
		else if (msg->dstsvc == PLAT_DM_SVC)
		{
			/* dmp info update msg and leave msg can only be sent when gw is registed */
			if ((gw->data->gw_state != STA_REG_TO_PLAT) && (msg->type
					!= DEVICE_LOGIN_REQUEST))
			{
				gw_debug_print("GW not registered yet, can only send DEV_JOIN_REQ dmp data!\n");
				rsp_msg = gen_ack_msg(msg, EC_DEV_UNREG);
				gw_msg_free(msg);
				gw_msg_send(gw, rsp_msg);
			}
			else if ((msg->type == DEVICE_LOGIN_REQUEST) || (msg->type == DEVICE_LOGOUT_REQUEST)
					|| (msg->type == DEVICE_UPDATE_REQUEST))
			{
				gw_debug_print("mod ni has a dmp msg to send, type %d, src svc is %d!\n",msg->type, msg->srcsvc);
				ni_msglist_add(msg, gw);
				memset(send_buff, 0, SEND_BUFFER_LEN);
				send_num = dmp_write_to_buffer(msg, send_buff, SEND_BUFFER_LEN);
				sendto(send_dmp_sock, send_buff, send_num, 0, (SA *) &dmp_addr,
						sizeof(dmp_addr));
				ni_stats.dmp_send_num ++;
				ni_stats.dmp_wait_num ++;
			}
			else
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "unknown message type %u received by service %u, module %s\n",
						msg->type, dmp_svc.id, mod_ni.name);
				gw_msg_free(msg);
			}
		}
		else
		{
			gw_log_print(GW_LOG_LEVEL_WARNING, "unknown dstsvc %u received by module %s\n",
					msg->type, mod_ni.name);
			gw_msg_free(msg);
		}
	}
	gw_debug_print("NI module recv thread quit!\n");
	gw_thread_exit(0);
}

/* called every 100ms, increase send message time, check if a message needs a retranmission */
static void timeout_cb(int fd, short event, void *arg)
{
	struct timeval tv, tm;
	ni_msg_t *ni_msg = (ni_msg_t *)arg;
	gw_t *gw = ni_msg->gw;
//	event_cb_arg_t *timer_cb_arg = (event_cb_arg_t *) arg;
//	struct event *timeout = timer_cb_arg->evt;
//	gw_t *gw = timer_cb_arg->gw;
	

//	struct list_head *pos, *tmp_pos;
//	ni_msg_t *msg_info;
//	int32_t init_retrans_time;
//	int32_t max_retrans_time;
//	int32_t max_retrans_cnt;
	int8_t send_buff[SEND_BUFFER_LEN];
	struct sockaddr_in dcp_addr, dmp_addr;
	gw_msg_t *rsp_msg;
	int32_t send_num;
	time_param_t tpm;
	bool_t ret;
//	gettimeofday(&tm, NULL);
//	printf("send time sec %u, usec %u\n",(uint32_t)tm.tv_sec,(uint32_t)tm.tv_usec/1000);
	/***** initialize dcp and dmp send socket *****/
	bzero(&dcp_addr, sizeof(dcp_addr));
	dcp_addr.sin_family = AF_INET;
	dcp_addr.sin_port = htons(ni_conf.dcp_serv_port);
	inet_pton(AF_INET, ni_conf.pf_ip->data, &dcp_addr.sin_addr);

	bzero(&dmp_addr, sizeof(dmp_addr));
	dmp_addr.sin_family = AF_INET;
	dmp_addr.sin_port = htons(ni_conf.dmp_serv_port);
	inet_pton(AF_INET, ni_conf.pf_ip->data, &dmp_addr.sin_addr);

	get_time_var(&tpm, ni_msg->msg->type);
	if(ni_msg->cnt > tpm.max_trans_cnt)
	{
		gw_debug_print("Msg retransmite two much time!\n");
		ni_msglist_del(ni_msg);
		if(ni_msg->msg->type == UPSTREAM_DATA)
		{
			ni_stats.dcp_fail_num ++;
			ni_stats.dcp_wait_num --;
		}
		else
		{
			ni_stats.dmp_fail_num ++;
			ni_stats.dmp_wait_num --;
		}
		rsp_msg = gen_ack_msg(ni_msg->msg, EC_NETWOEK_ERROR);
		gw_msg_free(ni_msg->msg);
		free(msg_info);
		msg_info = NULL;
		ret = gw_msg_send(gw, rsp_msg);
		if(ret == FALSE)
		{
			gw_debug_print("Msg send error!\n");
			gw_msg_free(rsp_msg);
		}
	}esle
	{
		ni_msg->interval = MIN(2*ni_msg->interval, tpm.max_trans_time);
		if (ni_msg->msg->dstsvc == PLAT_DATA_SVC)
		{
			memset(send_buff, 0, SEND_BUFFER_LEN);
			send_num = dcp_write_to_buffer(ni_msg->msg, send_buff,
					SEND_BUFFER_LEN);
			sendto(send_dcp_sock, send_buff, send_num, 0,
					(SA *) &dcp_addr, sizeof(dcp_addr));
			gettimeofday(&tm, NULL);
			printf("send time sec %u, usec %u\n",(uint32_t)tm.tv_sec,(uint32_t)tm.tv_usec/1000);
			gw_debug_print("resend dcp data serial %d time %d\n",ni_msg->msg->serial,ni_msg->interval);
		}
		else if (ni_msg->msg->dstsvc == PLAT_DM_SVC)
		{
			memset(send_buff, 0, SEND_BUFFER_LEN);
			send_num = dmp_write_to_buffer(ni_msg->msg, send_buff,
					SEND_BUFFER_LEN);
			sendto(send_dmp_sock, send_buff, send_num, 0,
					(SA *) &dmp_addr, sizeof(dmp_addr));
			gettimeofday(&tm, NULL);
			printf("send time sec %u, usec %u\n",(uint32_t)tm.tv_sec,(uint32_t)tm.tv_usec/1000);
			gw_debug_print("resend dmp data serial %d\n",ni_msg->msg->serial);
		}
		tv.tv_sec = ni_msg->interval;
		tv.tv_usec = 0;
		event_add(ni_msg->timeout, &tv);
	}
		
/*	
	gw_mutex_lock(msg_list.mutex);
	list_for_each_safe(pos, tmp_pos, &(msg_list.header))
	{
		msg_info = list_entry(pos, ni_msg_t, entry);
		if ((++(msg_info->elapse)) >= msg_info->interval*10)
		{
			get_time_var(&tpm, msg_info->msg->type);
			max_retrans_time = tpm.max_trans_time;
			max_retrans_cnt = tpm.max_trans_cnt;
			printf("max retrans time is %u max_retrans_cnt %u\n", max_retrans_time, max_retrans_cnt);
			if ((++(msg_info->cnt)) > max_retrans_cnt)
			{
				gw_debug_print("msg %d transmite too much times!\n", msg_info->msg->serial);
				msg_list.num--;
				list_del(&(msg_info->entry));

				if(msg_info->msg->type == UPSTREAM_DATA)
				{
					ni_stats.dcp_fail_num ++;
					ni_stats.dcp_wait_num --;
				}
				else
				{
					ni_stats.dmp_fail_num ++;
					ni_stats.dmp_wait_num --;
				}
				if(msg_info->msg->type != DEVICE_UPDATE_REQUEST)
				{
					rsp_msg = gen_ack_msg(msg_info->msg, EC_NETWOEK_ERROR);
					gw_msg_free(msg_info->msg);
					free(msg_info);
					msg_info = NULL;
					gw_msg_send(gw, rsp_msg);
				}
				else
				{
					gw_msg_free(msg_info->msg);
					free(msg_info);
					msg_info = NULL;
				}
			}
			else
			{
				msg_info->elapse = 0;
				msg_info->interval = MIN(2*msg_info->interval, max_retrans_time);
				printf("max retrans time is %d\n", max_retrans_time);
				if (msg_info->msg->dstsvc == PLAT_DATA_SVC)
				{
					memset(send_buff, 0, SEND_BUFFER_LEN);
					send_num = dcp_write_to_buffer(msg_info->msg, send_buff,
							SEND_BUFFER_LEN);
					sendto(send_dcp_sock, send_buff, send_num, 0,
							(SA *) &dcp_addr, sizeof(dcp_addr));
					gettimeofday(&tm, NULL);
					printf("send time sec %u, usec %u\n",(uint32_t)tm.tv_sec,(uint32_t)tm.tv_usec/1000);
					gw_debug_print("resend dcp data serial %d time %d\n",msg_info->msg->serial,msg_info->interval);
				}
				else if (msg_info->msg->dstsvc == PLAT_DM_SVC)
				{
					memset(send_buff, 0, SEND_BUFFER_LEN);
					send_num = dmp_write_to_buffer(msg_info->msg, send_buff,
							SEND_BUFFER_LEN);
					sendto(send_dmp_sock, send_buff, send_num, 0,
							(SA *) &dmp_addr, sizeof(dmp_addr));
					gw_debug_print("resend dmp data serial %d\n",msg_info->msg->serial);
				}
				gw_debug_print("Ni module %d service has a data type %d retransmite %d time, seq is %d\n",
						msg_info->msg->dstsvc, msg_info->msg->type, msg_info->cnt, msg_info->msg->serial);
			}
		}
	}
	gw_mutex_unlock(msg_list.mutex);
	tv.tv_sec = 0;
	tv.tv_usec = 97 * 1000;
	event_add(timeout, &tv);
	*/
}

static void dcprecv_cb(int fd, short event, void *arg)
{
	int32_t recv_num;
	bool_t ret;
	gw_t *gw = (gw_t *) arg;
	gw_msg_t *msg, *rsp_msg;
	ni_msg_t *dcpmsg;
	uint8_t recv_buff[RECV_BUFFER_LEN];
	struct sockaddr_in tmp_cli_addr;
	int32_t tmp_cli_len = sizeof(tmp_cli_addr);
	struct sockaddr_in dcp_addr;
	int32_t send_num;
	int32_t i;

	recv_num = recvfrom(fd, recv_buff, RECV_BUFFER_LEN, 0,
			(SA *) &tmp_cli_addr, &tmp_cli_len);

	if (recv_num >= DCP_HEAD_LEN)
	{
		gw_debug_print("receive a dcp data from platform! receive data is:\n");
		for (i = 0; i < recv_num; i++)
		{
			printf(" %x ", recv_buff[i]);
		}
		printf("\n");
		msg = gw_msg_alloc();
		memset(msg, 0, sizeof(gw_msg_t));
		ret = dcp_read_from_buffer(msg, recv_buff, recv_num);
		if (ret < 0)
		{
			gw_msg_free(msg);
			gw_log_print(GW_LOG_LEVEL_WARNING, "Dcp packet parse error!\n");
			return;
		}
		switch (msg->type)
		{
		case UPSTREAM_DATA_ACK:
		{
			gw_debug_print("NI receive a report confirm message, seq is %d!\n",msg->serial);
			dcpmsg = ni_msglist_get(msg);
			if (dcpmsg != NULL)
			{
				if(ret == 1)
				{
					gw_msg_free(msg);
					gw_debug_print("NI receive packet app id error!\n");
					ni_msglist_del(dcpmsg);
					dcpmsg->msg->retcode = 1;
					gw_msg_send(gw, dcpmsg->msg);
					free(dcpmsg);
					dcpmsg = NULL;
				}
				else if(ret == 2)
				{
					gw_msg_free(msg);
					gw_debug_print("NI receive packet sp id error!\n");
					ni_msglist_del(dcpmsg);
					dcpmsg->msg->retcode = 2;
					gw_msg_send(gw, dcpmsg->msg);
					free(dcpmsg);
					dcpmsg = NULL;
				}
				else
				{
					ni_msglist_del(dcpmsg);
					ni_stats.dcp_success_num ++;
					ni_stats.dcp_wait_num --;
					msg->dstsvc = dcpmsg->msg->srcsvc;
					gw_msg_free(dcpmsg->msg);
					free(dcpmsg);
					dcpmsg = NULL;
					gw_msg_send(gw, msg);
				}
			}
			else
			{
				gw_debug_print("Receive a mismatch dcp confirm msg!\n");
				gw_msg_free(msg);
			}
			break;
		}
		case DOWNSTREAM_DATA:
		{
			gw_debug_print("NI receive a platform request message!\n");
			ni_stats.dcp_recv_num ++;
			if (!memcmp(msg->devid, gw->config->nui->data, NUI_LEN))
			{
				msg->dstsvc = GW_DATAINIT_SERVICE;
				gw_msg_send(gw, msg);
			}
			else
			{
				node_data_t *tmp_node_data = node_data_get(msg->devid, NUI_LEN);
				if ((tmp_node_data == NULL) || (tmp_node_data->state != STA_REG_TO_PLAT))
				{
					gw_debug_print("Unknown msg dst received or node not registered!\n");
					gw_msg_free(msg);
				}
				else
				{
					msg->dstsvc = SN_DATA_SVC; //regard data svc is mg svc+1
					printf("Gw receive a data for south network!\n");
					gw_msg_send(gw, msg);
				}
			}
			break;
		}
		default:
		{
			gw_log_print(GW_LOG_LEVEL_WARNING, "Dcp unit receive a unknown message!\n");
			gw_msg_free(msg);
			break;
		}
		}
	}
	else
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Dcp receive packet len error!\n");
	}
}

static void dmprecv_cb(int fd, short event, void *arg)
{
	int32_t recv_num, i, j;
	bool_t ret;
	gw_t *gw = (gw_t *) arg;
	gw_msg_t *msg;
	ni_msg_t *dmpmsg;
	int8_t recv_buff[RECV_BUFFER_LEN];
	struct sockaddr_in tmp_cli_addr;
	int32_t tmp_cli_len = sizeof(tmp_cli_addr);

	recv_num = recvfrom(fd, recv_buff, RECV_BUFFER_LEN, 0,
			(SA *) &tmp_cli_addr, &tmp_cli_len);
	if (recv_num >= DMP_HEAD_LEN)
	{
		printf("dmp receive data is: \n");
		for(j=0;j<recv_num;j++)
		{
			printf("%02x ", (uint8_t)recv_buff[j]);
		}
		printf("\n");
		msg = gw_msg_alloc();
		memset(msg, 0, sizeof(gw_msg_t));
		ret = dmp_read_from_buffer(msg, recv_buff, recv_num);
		if (ret != TRUE)
		{
			gw_msg_free(msg);
			gw_log_print(GW_LOG_LEVEL_WARNING, "Dmp packet parse error!\n");
			return;
		}
		dmpmsg = ni_msglist_get(msg);
		if (dmpmsg != NULL)
		{
			ni_msglist_del(dmpmsg);
			ni_stats.dmp_success_num ++;
			ni_stats.dmp_wait_num --;
			msg->dstsvc = dmpmsg->msg->srcsvc;
			switch(dmpmsg->msg->type)
			{
			case DEVICE_LOGIN_REQUEST:
			{
				gw_debug_print("receive a login response data\n");
				if(msg->datasize == 0)
				{
					msg->type = DEVICE_LOGIN_RESPONSE;
					gw_msg_send(gw, msg);
				}
				else
				{
					gw_msg_t *tmsg;
					tmsg = gen_ack_msg(dmpmsg->msg, msg->retcode);
					gw_msg_send(gw, tmsg);
					usleep(100*1000);
					msg->type = PASSIVE_CONFIG_REQUEST;
					msg->retcode = 0;
					msg->dstsvc = dmpmsg->msg->srcsvc;
					gw_msg_send(gw, msg);
				}
				break;
			}
			case DEVICE_LOGOUT_REQUEST:
			{
				gw_debug_print("receive a logout response data\n");
				msg->type = DEVICE_LOGOUT_RESPONSE;
				gw_msg_send(gw, msg);
				break;
			}
			case DEVICE_UPDATE_REQUEST:
			{
				msg->type = PASSIVE_CONFIG_REQUEST;
				msg->retcode = 0;
				msg->dstsvc = dmpmsg->msg->srcsvc;
				gw_debug_print("config data is :\n");
				for(i=0; i<msg->datasize; i++)
				{
					printf("%x ", *((uint8_t *)msg->data +i));
				}
				printf("\n");
				gw_msg_send(gw, msg);
				break;
			}
			default:
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "Unknown msg type %d\n", dmpmsg->msg->type);
				gw_msg_free(msg);
			}
			}
			gw_msg_free(dmpmsg->msg);
			free(dmpmsg);
			dmpmsg = NULL;
		}
		else
		{
			gw_debug_print("Receive a mismatch dmp confirm msg!\n");
			gw_msg_free(msg);
		}
	}
	else
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Dmp receive packet len error!\n");
	}
	gw_debug_print("NI receive thread has dmp data received!\n");
}

static void fifo_read_cb(int fd, short event, void *arg)
{
	struct event *evfifo = (struct event *) arg;
	char buf[255];
	int len;
	len = read(fd, buf, sizeof(buf) - 1);

	if (len == -1)
	{
		perror("read");
		return;
	}
	else
	{
		gw_log_print(GW_LOG_LEVEL_INFO, "Ni thread exit!\n");
	}
	close(fd);
	event_base_loopbreak(evfifo->ev_base);
}

/**
 * @brief Start a libevent for receiving network data.
 *
 * @param args pointer to gateway context
 *
 * @return NULL
 */
void *ni_recv_handler(void *args)
{
	gw_msg_t *msg;
	gw_t *gw = (gw_t *) args;
	int32_t recv_dcp_sock, recv_dmp_sock;
	struct sockaddr_in client_dcp_addr, client_dmp_addr;
	struct event timeout, dcprecv, dmprecv;
	struct timeval tv;
	struct event_base *base;
	event_cb_arg_t timer_cb_arg;
	struct event evfifo;
	struct stat st;
	int fifofd;

	if (lstat(ni_fifo, &st) == 0)
	{
		if ((st.st_mode & S_IFMT) == S_IFREG)
		{
			gw_debug_print("lstat");
			exit(1);
		}
	}

	unlink(ni_fifo);
	if (mkfifo(ni_fifo, 0777) == -1)
	{
		gw_debug_print("mkfifo");
		exit(1);
	}
	fifofd = open(ni_fifo, O_RDWR | O_NONBLOCK, 0);
	if (fifofd == -1)
	{
		gw_debug_print("open");
		exit(1);
	}

	/***** initialize dcp and dmp recv socket *****/
	recv_dcp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&client_dcp_addr, sizeof(client_dcp_addr));
	client_dcp_addr.sin_family = AF_INET;
	client_dcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	client_dcp_addr.sin_port = htons(ni_conf.dcp_cli_port);
	bind(recv_dcp_sock, (SA *) &client_dcp_addr, sizeof(client_dcp_addr));

	recv_dmp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&client_dmp_addr, sizeof(client_dmp_addr));
	client_dmp_addr.sin_family = AF_INET;
	client_dmp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	client_dmp_addr.sin_port = htons(ni_conf.dmp_cli_port);
	bind(recv_dmp_sock, (SA *) &client_dmp_addr, sizeof(client_dmp_addr));
	/* Initalize the event library */
//	event_init();

	/* Initalize one event */
//	timer_cb_arg.evt = &timeout;
//	timer_cb_arg.gw = gw;
//	evtimer_set(&timeout, timeout_cb, &timer_cb_arg);

//	evutil_timerclear(&tv);
//	tv.tv_sec = 0;
//	tv.tv_usec = 100 * 1000;
//	event_add(&timeout, &tv);

	ev_lock = gw_mutex_create();
	ev_base = event_base_new();
	event_assign(&dcprecv, ev_base, recv_dcp_sock, (EV_READ | EV_PERSIST), dcprecv_cb,
			(void *) gw);

//	event_set(&dcprecv, recv_dcp_sock, (EV_READ | EV_PERSIST), dcprecv_cb,
//			(void *) gw);
	event_add(&dcprecv, NULL);

	event_assign(&dmprecv, ev_base, recv_dmp_sock, (EV_READ | EV_PERSIST), dmprecv_cb,
			(void *) gw);
//	event_set(&dmprecv, recv_dmp_sock, (EV_READ | EV_PERSIST), dmprecv_cb,
//			(void *) gw);
	event_add(&dmprecv, NULL);

	event_assign(&evfifo, ev_base, fifofd, EV_READ,  fifo_read_cb, (void *) &evfifo);
//	event_set(&evfifo, fifofd, EV_READ, fifo_read_cb, (void *) &evfifo);
	event_add(&evfifo, NULL);

	event_dispatch();

	gw_mutex_destroy(ev_lock);
	close(fifofd);
	close(recv_dcp_sock);
	close(recv_dmp_sock);
}

bool_t init(gw_t *gw, gw_dlib_t lib)
{
	bool_t ret = TRUE;

	/* initialize module data */
	memset(&ni_data, 0, sizeof(ni_data));
	memset(&ni_conf, 0, sizeof(ni_conf));
	memset(&ni_stats, 0, sizeof(ni_stats));

	/* read config infromation */
	load_config("/etc/gw/modules/ni.conf");
	ni_data.config = &ni_conf;
	ni_data.stats = &ni_stats;

	/* initialize module structure */
	memset(&mod_ni, 0, sizeof(mod_ni));
	mod_ni.name = "mod_ni";
	mod_ni.desc = "mod ni, the gw north interface to the platform";
	mod_ni.vendor = "WSN";
	mod_ni.modver.major = 1;
	mod_ni.modver.minor = 0;
	mod_ni.sysver.major = 1;
	mod_ni.sysver.minor = 0;
	mod_ni.init = init;
	mod_ni.cleanup = cleanup;
	mod_ni.serialize = serialize;
	mod_ni.lib = lib;
	mod_ni.data = &ni_data;
	mod_ni.lock = gw_mutex_create();
	INIT_LIST_HEAD(&mod_ni.services);

	ni_msgq = gw_msgq_create();

	memset(&dcp_svc, 0, sizeof(dcp_svc));
	dcp_svc.id = PLAT_DATA_SVC;
	dcp_svc.name = "PLAT_DATA_SVC";
	dcp_svc.mod = &mod_ni;
	dcp_svc.mq = ni_msgq;

	memset(&dmp_svc, 0, sizeof(dmp_svc));
	dmp_svc.id = PLAT_DM_SVC;
	dmp_svc.name = "PLAT_DM_SVC";
	dmp_svc.mod = &mod_ni;
	dmp_svc.mq = ni_msgq;

	if (mod_ni.lock != NULL)
	{
		ret = mod_register(gw, &mod_ni);
		if (ret)
		{
			ret = mod_register_service(gw, &mod_ni, &dcp_svc);
			if (ret != TRUE)
			{
				mod_unregister(gw, &mod_ni);
				gw_mutex_destroy(mod_ni.lock);
				gw_msgq_destroy(ni_msgq);
				gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_ni.name);


			}
			else
			{
				ret = mod_register_service(gw, &mod_ni, &dmp_svc);
				if (ret != TRUE)
				{
					mod_unregister_service(gw, &mod_ni, &dcp_svc);
					mod_unregister(gw, &mod_ni);
					gw_mutex_destroy(mod_ni.lock);
					gw_msgq_destroy(ni_msgq);
					gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_ni.name);
					return ret;
				}
			}

			gw_log_print(GW_LOG_LEVEL_INFO, "module %s initialized\n", mod_ni.name);
		}
	}

	if(ret)
	{
		send_dcp_sock = socket(AF_INET, SOCK_DGRAM, 0);
		send_dmp_sock = socket(AF_INET, SOCK_DGRAM, 0);
		ni_msglist_init();
		send_thread = gw_thread_create(ni_send_handler, (void *) gw);
		if (send_thread == NULL)
		{
			gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialized failed, thread create error!\n", mod_ni.name);
			ret = FALSE;
		}

		recv_thread = gw_thread_create(ni_recv_handler, (void *) gw);
		if (recv_thread == NULL)
		{
			gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialized failed, thread create error!\n", mod_ni.name);
			ret = FALSE;
		}
	}

	return ret;
}

bool_t cleanup(gw_t *gw)
{
	//TODO: cleanup module stuff
	gw_msg_t *msg;
	gw_debug_print("mod ni start to clean up!\n");
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

	fifofd = open(ni_fifo, O_RDWR | O_NONBLOCK, 0);
	if (fifofd == -1)
	{
		perror("open!!!");
	}else
	{
		write(fifofd, cmd, sizeof(cmd));
		close(fifofd);
		gw_thread_wait(recv_thread);
	}


	msg = gw_msg_alloc();
	msg->type = GW_MSG_SYSTEM_QUIT;
	msg->dstsvc = dcp_svc.id;
	msg->srcsvc = dcp_svc.id;
	gw_msg_send(gw, msg);
	gw_thread_wait(send_thread);

	mod_unregister_service(gw, &mod_ni, &dcp_svc);
	mod_unregister_service(gw, &mod_ni, &dmp_svc);
	mod_unregister(gw, &mod_ni);

	gw_msgq_destroy(ni_msgq);
	gw_mutex_destroy(mod_ni.lock);

	ni_msglist_destroy();
	close(send_dcp_sock);
	close(send_dmp_sock);
	bdestroy(ni_conf.pf_ip);
	gw_debug_print("mog ni end the clean up!\n");
	return TRUE;
}

json_object *serialize(gw_t *gw)
{

	json_object *new_obj;
	int8_t buff[100];
	int64_t tnum;
	new_obj = json_object_new_object();
	/*  ni module status */
	memset(buff, 0, 100);
	sprintf(buff, "%lld", (ni_stats.dcp_send_num + ni_stats.dmp_send_num));
	json_object_object_add(new_obj, "TOTAL_SEND_PACKET_NUM",
			json_object_new_string(buff));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", (ni_stats.dcp_recv_num));
	json_object_object_add(new_obj, "TOTAL_RECV_PACKET_NUM",
			json_object_new_string(buff));
	json_object_object_add(new_obj, "TOTAL_WAIT_PACKET_NUM",
			json_object_new_int(ni_stats.dcp_wait_num + ni_stats.dmp_wait_num));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", (ni_stats.dcp_success_num + ni_stats.dmp_success_num));
	json_object_object_add(new_obj, "SEND_SUCCESS_PACKET_NUM",
			json_object_new_string(buff));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", (ni_stats.dcp_fail_num + ni_stats.dmp_fail_num));
	json_object_object_add(new_obj, "SEND_FAIL_PACKET_NUM",
			json_object_new_string(buff));
	tnum = (ni_stats.dcp_success_num + ni_stats.dmp_success_num)
				+ (ni_stats.dcp_fail_num + ni_stats.dmp_fail_num);
	if(tnum == 0)
	{
		json_object_object_add(new_obj, "TOTAL_SEND_SUCCESS_RATIO",
				json_object_new_int(0));
	}
	else
	{
		memset(buff, 0, 100);
		sprintf(buff, "%d", (uint32_t)((ni_stats.dcp_success_num + ni_stats.dmp_success_num)
				* 100 / tnum));
		strcat(buff, "%");
		json_object_object_add(new_obj, "TOTAL_SEND_SUCCESS_RATIO",
					json_object_new_string(buff));
	}
	/* dcp status */
	memset(buff, 0, 100);
	sprintf(buff, "%lld", ni_stats.dcp_send_num);
	json_object_object_add(new_obj, "SEND_DATA_PACKET_NUM",
			json_object_new_string(buff));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", (ni_stats.dcp_recv_num));
	json_object_object_add(new_obj, "RECV_DATA_PACKET_NUM",
			json_object_new_string(buff));
	json_object_object_add(new_obj, "WAIT_DATA_PACKET_NUM",
			json_object_new_int(ni_stats.dcp_wait_num));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", ni_stats.dcp_success_num);
	json_object_object_add(new_obj, "SEND_DATA_PACKET_SUCCESS_NUM",
			json_object_new_string(buff));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", ni_stats.dcp_fail_num);
	json_object_object_add(new_obj, "SEND_DATA_PACKET_FAIL_NUM",
			json_object_new_string(buff));
	tnum = ni_stats.dcp_success_num + ni_stats.dcp_fail_num;
	if(tnum == 0)
	{
		json_object_object_add(new_obj, "SEND_DATA_PACKET_SUCCESS_RATIO",
				json_object_new_int(0));
	}
	else
	{
		memset(buff, 0, 100);
		sprintf(buff, "%lld", ni_stats.dcp_success_num * 100
					/ tnum);
		strcat(buff, "%");
		json_object_object_add(new_obj, "SEND_DATA_PACKET_SUCCESS_RATIO",
				json_object_new_string(buff));
	}
	/* dmp status */
	memset(buff, 0, 100);
	sprintf(buff, "%lld", ni_stats.dmp_send_num);
	json_object_object_add(new_obj, "SEND_MANAGE_PACKET_NUM",
			json_object_new_string(buff));
	json_object_object_add(new_obj, "WAIT_MANAGE_PACKET_NUM",
			json_object_new_int(ni_stats.dmp_wait_num));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", ni_stats.dmp_success_num);
	json_object_object_add(new_obj, "SEND_MANAGE_PACKET_SUCCESS_NUM",
			json_object_new_string(buff));
	memset(buff, 0, 100);
	sprintf(buff, "%lld", ni_stats.dmp_fail_num);
	json_object_object_add(new_obj, "SEND_MANAGE_PACKET_FAIL_NUM",
			json_object_new_string(buff));
	tnum = ni_stats.dmp_success_num + ni_stats.dmp_fail_num;
	if(tnum == 0)
	{
		json_object_object_add(new_obj, "SEND_MANAGE_PACKET_SUCCESS_RATIO",
				json_object_new_int(0));
	}
	else
	{
		memset(buff, 0, 100);
		sprintf(buff, "%lld", ni_stats.dmp_success_num * 100
				/ tnum);
		strcat(buff, "%");
		json_object_object_add(new_obj, "SEND_MANAGE_PACKET_SUCCESS_RATIO",
					json_object_new_string(buff));
	}
	return new_obj;

}

