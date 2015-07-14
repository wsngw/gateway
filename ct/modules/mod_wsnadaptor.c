/**
 * @brief wsnadaptor module implementation \n
 *  This module received data from wsn sink with uart, change its format and process the data
 *  it also received data from platform ,and forward it to sink in AT command format
 * @file mod_wsnadaptor.c
 * @author wanghl  <hlwang2001@163.com>
 *
 * @addtogroup Plugins_wsnadaptor Gateway wsn-adaptor module
 * @{
 */
#include <fcntl.h>
#include <stdio.h>
#include <gwtypes.h>
#include <gwmutex.h>
#include <gwthread.h>
#include <module.h>
#include <log.h>
#include <msg.h>
#include <dotconf/dotconf.h>
#include <gw.h>
#include <atcmddriver.h>
#include <json/json_object.h>
#include <infobase.h>
#include "mod_wsnadaptor.h"


static gw_mod_t mod_wsnadaptor;
static gw_mod_data_t wsnadpt_data;
static gw_svc_t wsnadpt_dm_svc, wsnadpt_data_svc;
static gw_msgq_t *wsnadpt_msgq;
static gw_thread_t wsnadpt_downlink_thread = NULL;
static gw_thread_t wsnadpt_uplink_thread = NULL;
wsnadpt_conf_t wsnadpt_conf;

// key data structure, global uart state data for every opened uart
struct uart_data uartdata[MAX_WSN] ;

bool_t init(gw_t *gw, gw_dlib_t lib);
bool_t cleanup(gw_t *gw);
json_object *serialize(gw_t *gw);

int32_t wsnadpt_config(int8_t *path_filename);
/*
static DOTCONF_CB(default_callback);

static const configoption_t options[] = {
	{"BaudRate", ARG_INT, default_callback, NULL, 0},
	{"", ARG_NAME, default_callback, NULL, 0},
	LAST_OPTION
};

static int32_t wsnadpt_config(int8_t *path_filename)
{
	configfile_t *configfile;

	if (path_filename == NULL) {
		printf("error: NULL input\n");
		return 1;
	}
	configfile = dotconf_create(path_filename, options, NULL, CASE_INSENSITIVE);
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

static int32_t wsnadpt_config_cleanup(void)
{
	int32_t ret;
//	ret = bdestroy(wsnadpt_conf.ip_address);
	return ret;
}

DOTCONF_CB(default_callback)
{
	int32_t i, arg_count;
	bstring command;
	bstring tmpcmd;
	char_t *tmpstr;
	int8_t *ret = NULL;

	gw_debug_print("default_callback handler called for \"%s\". Got %d args\n", cmd->name,
	       cmd->arg_count);
	for (i = 0; i < cmd->arg_count; i++)
		gw_debug_print("\tArg #%d: %s\n", i, cmd->data.list[i]);

	command = bfromcstr(cmd->name);
	arg_count = cmd->arg_count;
	tmpcmd = bfromcstr("device");
	if ( bstricmp(command, tmpcmd) == 0 ) {
		tmpstr = cmd->data.list[0];
		if ( strcmp(tmpstr, "ttyS0") == 0 )
			wsnadpt_conf.uartport = 1;
		else if ( strcmp(tmpstr, "ttyS1") == 0 )
			wsnadpt_conf.uartport = 2;
		else if ( strcmp(tmpstr, "ttyS2") == 0 )
			wsnadpt_conf.uartport = 3;
		else
			wsnadpt_conf.uartport = 1;

		gw_debug_print("the uart is %s\n", tmpstr);
//		ret = (int32_t *)wsnadpt_conf.uartport;
	}

	bassigncstr(tmpcmd, "BaudRate");
	if ( bstricmp(command, tmpcmd) == 0 )
	{
		wsnadpt_conf.baudrate = cmd->data.value;
		printf("the baudrate is set to %d\n", wsnadpt_conf.baudrate);
//		ret = (uint32_t *)&wsnadpt_conf.baudrate;
	}
	bdestroy(command);
	bdestroy(tmpcmd);
	return NULL;
}
*/


// wsn adaptor downlink data handler
void *wsnadpt_downlink_handler(void *args)
{
	gw_msg_t *msg;
	bool_t quit = FALSE;
	char_t *ackbuf;
	node_data_t *tmp_nddata;
	sn_data_t * tmp_sndata;
	struct uart_data *uartdata;
	printf("enter !!! downlink handler!!!!\n");
	while (!quit && ((msg = gw_msg_recv(wsnadpt_msgq)) != NULL))
	{
		uint8_t ackdatabuf[ACK_PKT_LEN]; // header(10) + NUI(8) + retcode(2)
		uint8_t *ackbuf = ackdatabuf;
		uint8_t *tmpbuf, *ptr;

		if(msg->type == GW_MSG_SYSTEM_QUIT)
		{
			quit = TRUE;
			gw_msg_free(msg);
			gw_debug_print("wsn adptor quit!\n");
			break;
		}
		printf("enter downlink handler, type is %d!!!!!!!!!!!!!!!!!!!!!!!\n",msg->type);
		// generate ack mesg
		ackbuf[0]=0xd5;
		ackbuf[1]=0xc8;
		ackbuf[2]=(uint8_t)(ACK_PKT_LEN-4); // length = ACK_PKT_LEN-2-2
		ackbuf[3]=0x00;
		ackbuf[4]=msg->serial; // serial
		ackbuf[5]=(msg->serial)>>8;
		ackbuf[6]=(msg->serial)>>16;
		ackbuf[7]=(msg->serial)>>24;
		printf("\n get mesg in wsnadaptor,msg-type = %x\n",msg->type);

		tmp_nddata = node_data_get(msg->devid, NUI_LEN);
		if (tmp_nddata == NULL)
		{
			printf("can't find nodedata  with this NUI\n");
			gw_msg_free(msg);
			continue;
		}
		tmp_sndata = tmp_nddata->pdata;
		uartdata = (struct uart_data *)(tmp_sndata->usrdata);

		switch (msg->type)
		{
		case DEVICE_LOGIN_RESPONSE:
			ackbuf[8]= (uint8_t)(AT_CMD_REG_ACK);
			ackbuf[9]=(uint8_t)(AT_CMD_REG_ACK >>8);
			memcpy(ackbuf+10, msg->devid, NID_SIZE ); //NUI
			ackbuf[18]= msg->retcode;
			ackbuf[19]= msg->retcode>>8;

			write_complete(uartdata->filefd, ackbuf, ACK_PKT_LEN);
			// save to file
			write_complete(uartdata->managefd, ackbuf, ACK_PKT_LEN );
			break;

		case DEVICE_LOGOUT_RESPONSE:
			ackbuf[8]= (uint8_t)(AT_CMD_UNREG_ACK);
			ackbuf[9]=(uint8_t)(AT_CMD_UNREG_ACK >>8);
			memcpy(ackbuf+10, msg->devid, NID_SIZE ); //NUI
			ackbuf[18]= msg->retcode;
			ackbuf[19]= msg->retcode>>8;

			write_complete(uartdata->filefd, ackbuf, ACK_PKT_LEN);
			// save to file
			write_complete(uartdata->managefd, ackbuf, ACK_PKT_LEN );
			break;

		case UPSTREAM_DATA_ACK:
			ackbuf[8]= (uint8_t)(AT_CMD_DATA_ACK);
			ackbuf[9]=(uint8_t)(AT_CMD_DATA_ACK >>8);
			memcpy(ackbuf+10, msg->devid, NID_SIZE ); //NUI
			ackbuf[18]= msg->retcode;
			ackbuf[19]= msg->retcode>>8;

			// don't give ack msg, for test use only
	//		write_complete(uartdata->filefd, ackbuf, ACK_PKT_LEN);
			// save to file
			write_complete(uartdata->managefd, ackbuf, ACK_PKT_LEN );
			break;

		case ACTIVE_CONFIG_RESPONSE:
			ackbuf[8]= (uint8_t)(AT_CMD_CONFIG_ACK);
			ackbuf[9]=(uint8_t)(AT_CMD_CONFIG_ACK >>8);
			memcpy(ackbuf+10, msg->devid, NID_SIZE ); //NUI
			ackbuf[18]= msg->retcode;
			ackbuf[19]= msg->retcode>>8;

			write_complete(uartdata->filefd, ackbuf, ACK_PKT_LEN);
			// save to file
			write_complete(uartdata->managefd, ackbuf, ACK_PKT_LEN );
			break;

		case PASSIVE_CONFIG_REQUEST:
			tmpbuf = malloc(0x17); //
			if (tmpbuf == NULL)
			{
				printf("not enough memory in mod_wsnadptor downlink_handler\n");
				return NULL;
			}
			ackbuf = tmpbuf;

			ackbuf[0]=0xd5;
			ackbuf[1]=0xc8;
			ackbuf[2]= 0x13;
			ackbuf[3]=0x00;
			ackbuf[4]=msg->serial; // serial
			ackbuf[5]=(msg->serial)>>8;
			ackbuf[6]=(msg->serial)>>16;
			ackbuf[7]=(msg->serial)>>24;
			ackbuf[8]= (uint8_t)(AT_CMD_CONFIG);
			ackbuf[9]=(uint8_t)(AT_CMD_CONFIG >>8);
			ptr = (uint8_t *)msg->data;
			ackbuf[10] = *(ptr+2); // type low byte
			printf("msg config data is %x,%x,%x,%x,%x,%x\n",*ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5));
			memcpy(ackbuf+11, msg->devid, NID_SIZE);
			memcpy(ackbuf+19, ptr+6, 4);  // :ms

			write_complete(uartdata->filefd, ackbuf, 0x17);  // only config heart beat
			// save to file
			write_complete(uartdata->managefd, ackbuf, 0x17 );
			free(ackbuf);
			tmpbuf = NULL;
			break;

		case DOWNSTREAM_DATA:
			tmpbuf = malloc(18 + msg->datasize); //
			if (tmpbuf == NULL)
			{
				printf("not enough memory in mod_wsnadptor downlink_handler\n");
				return NULL;
			}
			ackbuf = tmpbuf;

			ackbuf[0]=0xd5;
			ackbuf[1]=0xc8;
			ackbuf[2]= 0x13;
			ackbuf[3]=0x00;
			ackbuf[4]=msg->serial; // serial
			ackbuf[5]=(msg->serial)>>8;
			ackbuf[6]=(msg->serial)>>16;
			ackbuf[7]=(msg->serial)>>24;
			ackbuf[8]= (uint8_t)(AT_CMD_DATA);
			ackbuf[9]=(uint8_t)(AT_CMD_DATA >>8);
			memcpy(ackbuf+10, msg->devid, NID_SIZE);
			memcpy(ackbuf+18, msg->data, msg->datasize);  // copy downstream data

			write_complete(uartdata->filefd, ackbuf, 18 + msg->datasize);  //
			// save to file
			write_complete(uartdata->managefd, ackbuf, 18 + msg->datasize );
			printf("Downstream data send\n");
			free(ackbuf);
			tmpbuf = NULL;
			break;

		default:
			gw_log_print(GW_LOG_LEVEL_WARNING, "unknown message type %u received in module %s\n", \
					msg->type, mod_wsnadaptor.name);
			break;
		}
		gw_msg_free(msg);
		printf("south interface free a message !!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}

	gw_thread_exit(0);
	return NULL;
}


void *wsnadaptor_uplink_handler( void *arg)
{
    int32_t fd,fd_max, ret,i;
    fd_set  rdfds;
	int32_t len;
	char_t buf[128];
	gw_t *gw = arg;
	struct timeval tv;
	sn_data_t * tmp_sndata;

	printf("uplink handler start!\n");
    while (1)
     {
    	/*
    	printf("listen com port, fd = %d\n",uartdata[0].filefd);
    	len = read(uartdata[0].filefd, buf, 24); // set recv buffersize, block here
    	if(len > 0)
    		printf("Get data,len = %d\n",len);
    	continue;
    	*/
    	FD_ZERO(&rdfds);

    	fd_max = 0;
    	for(i=0; i< wsnadpt_conf.num; i++)
    	{
    		fd = uartdata[i].filefd;
    		fd_max = (fd > fd_max)?fd:fd_max;
    		FD_SET(uartdata[i].filefd, &rdfds);
//    		printf("FD_SET() add fd = %d\n", uartdata[i].filefd);
    	}

//    	tv.tv_sec = 5;   //need set it every time
//    	tv.tv_usec = 0;
    	printf("select start here, fd_max = %d\n", fd_max);
    	ret = select(fd_max+1,&rdfds, NULL, NULL, NULL); // tv ==NULL ,block here,never timeout
    	printf("select return %d\n",ret);
    	if(ret < 0)
    	{
    		printf("wsn adaptor select error\n");
    	}
    	else if (ret == 0)
    	{
    		continue; // timeout
    	}
    	else
    	{
    		for(i=0; i<wsnadpt_conf.num; i++)
    		{
    			 if(FD_ISSET(uartdata[i].filefd, &rdfds))
    			 {
    		         len = read(uartdata[i].filefd, buf, 128); // set recv buffersize, block here
    				 printf("read data from %d, datalen = %d\n",uartdata[i].filefd,len);

    		         if (len > 0)
    		         {
    		        	 process_buf_data(gw, &uartdata[i], buf, len);
    		         }
    			 }
    		} // end for
    	} //end if

     } //end while

    for(i=0; i<wsnadpt_conf.num; i++)
    	close(uartdata[i].filefd);
}

bool_t init(gw_t *gw, gw_dlib_t lib)
{
	bool_t ret = TRUE;
	int32_t i,fd,savefd;
	sn_data_t *tmp_sndata;
	char_t filename[64];

	/* initialize module data */
	memset(&wsnadpt_data, 0, sizeof(wsnadpt_data));

	/* read config infromation */
	wsnadpt_config("/etc/gw/modules/wsnadaptor.conf");

	printf("start open uart and init data\n");
	for(i=0; i< wsnadpt_conf.num; i++)
	{
		if((fd=open_port(fd,wsnadpt_conf.config[i].uartport))< 0){
	        perror("open_port error");
	        return 1;
	    }

	    if((ret=set_opt(fd,wsnadpt_conf.config[i].baudrate,8,'N',1)) < 0){
	        perror("set_opt error");
	        return 1;
	    }

		uart_data_init( &uartdata[i], fd, 256);  //
		uartdata[i].sn_id = i;   // uart <--> sn_id
		uartdata[i].DstSrvID = wsnadpt_conf.config[i].DstSrvID;

	    sprintf(filename,"/home/wsn/gw_data/manage_data_%d",i);
		if((savefd= open(filename, O_WRONLY|O_CREAT|O_TRUNC))< 0){
	        perror("open_port error");
	        return 1;
	    }
		uartdata[i].managefd = savefd;

	    sprintf(filename,"/home/wsn/gw_data/app_data_%d",i);
		if((savefd=open(filename,O_WRONLY|O_CREAT|O_TRUNC))< 0){
	        perror("open_port error");
	        return 1;
	    }
		uartdata[i].appdatafd = savefd;

	    printf("uart ttyS%d opened,fd=%d\n",wsnadpt_conf.config[i].uartport-1, fd);

	    tmp_sndata = sn_data_new();
	    if(!tmp_sndata)
	    {
	    	printf("\nsn_data_new() Failed! exit()\n");
	    	return 1;
	    }
	    printf("new sn_data OK\n");
		tmp_sndata->module = &mod_wsnadaptor;
		tmp_sndata->sn_id = i;
		tmp_sndata->usrdata =  &uartdata[i];
		printf("sn_data init success!\n");
		ret = sn_data_add(tmp_sndata);
	    printf("add sn_data OK\n");
		if(ret == FALSE)
	    {
	    	printf("\nsn_data_add() Failed! exit()\n");
	    	return 1;
	    }
	}

	printf("\nbegin init mod_wsnadaptor\n");

	/* initialize module structure */
	memset(&mod_wsnadaptor, 0, sizeof(mod_wsnadaptor));
	mod_wsnadaptor.name = "mod_wsnadaptor";
	mod_wsnadaptor.desc = "adaptor between sink and GW";
	mod_wsnadaptor.vendor = "WSN";
	mod_wsnadaptor.modver.major = 1;
	mod_wsnadaptor.modver.minor = 0;
	mod_wsnadaptor.sysver.major = 1;
	mod_wsnadaptor.sysver.minor = 0;
	mod_wsnadaptor.init = init;
	mod_wsnadaptor.cleanup = cleanup;
	mod_wsnadaptor.serialize = serialize;
	mod_wsnadaptor.lib = lib;
	mod_wsnadaptor.data = &wsnadpt_data;
	mod_wsnadaptor.lock = gw_mutex_create();
	INIT_LIST_HEAD(&mod_wsnadaptor.services);

	wsnadpt_msgq = gw_msgq_create();
	memset(&wsnadpt_dm_svc, 0, sizeof(wsnadpt_dm_svc));
	wsnadpt_dm_svc.id = SN_DM_SVC;
	wsnadpt_dm_svc.name = "wsnadpt_dm_service";
	wsnadpt_dm_svc.mod = &mod_wsnadaptor;
	wsnadpt_dm_svc.mq = wsnadpt_msgq;

	memset(&wsnadpt_data_svc, 0, sizeof(wsnadpt_data_svc));
	wsnadpt_data_svc.id = SN_DATA_SVC;
	wsnadpt_data_svc.name = "wsnadpt_data_service";
	wsnadpt_data_svc.mod = &mod_wsnadaptor;
	wsnadpt_data_svc.mq = wsnadpt_msgq;

	printf("start downlink_handler and uplink_handler\n");

	wsnadpt_downlink_thread = gw_thread_create(wsnadpt_downlink_handler, NULL);  // wsn adaptor downlink handler

	wsnadpt_uplink_thread = gw_thread_create(wsnadaptor_uplink_handler, gw); // wsn adaptor uplink handler

	printf("begin register mod_wsnadaptor and register svc\n");
	//TODO: initialize another stuff
	if (mod_wsnadaptor.lock != NULL)
	{
		ret = mod_register(gw, &mod_wsnadaptor);
		if (ret)
		{
			ret = mod_register_service(gw, &mod_wsnadaptor, &wsnadpt_dm_svc);
			if (ret != TRUE)
			{
				mod_unregister(gw, &mod_wsnadaptor);
				gw_mutex_destroy(mod_wsnadaptor.lock);
				gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_wsnadaptor.name);
				return ret;
			}

			ret = mod_register_service(gw, &mod_wsnadaptor, &wsnadpt_data_svc);
			if (ret != TRUE)
			{
				mod_unregister(gw, &mod_wsnadaptor);
				gw_mutex_destroy(mod_wsnadaptor.lock);
				gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_wsnadaptor.name);
				return ret;
			}

			gw_log_print(GW_LOG_LEVEL_INFO, "module %s initialized\n", mod_wsnadaptor.name);
		}
	}
	printf("mod_wsnadaptor initialized\n");
	return ret;
}

bool_t cleanup(gw_t *gw)
{
	//TODO: cleanup module stuff
	gw_msg_t *msg;
	int32_t i;

	// Stop wsnadaptor_uplink_handler thread
	msg = gw_msg_alloc();
	msg->type = GW_MSG_SYSTEM_QUIT;
	msg->dstsvc = wsnadpt_dm_svc.id;
	msg->srcsvc = wsnadpt_dm_svc.id;
	//gw_msg_send(gw, msg);
	gw_mutex_lock(wsnadpt_msgq->lock);
	list_add(&(msg->header), &(wsnadpt_msgq->header));
	wsnadpt_msgq->num++;
	gw_mutex_unlock(wsnadpt_msgq->lock);
	gw_sem_post(wsnadpt_msgq->sem);

	gw_thread_wait(wsnadpt_downlink_thread);

    for(i=0; i<wsnadpt_conf.num; i++)
    {
    	close(uartdata[i].filefd);
    	close(uartdata[i].appdatafd);
    	close(uartdata[i].managefd);
    }

	mod_unregister_service(gw, &mod_wsnadaptor, &wsnadpt_dm_svc);
	//mod_unregister(gw, &mod_wsnadaptor);
	mod_unregister_service(gw, &mod_wsnadaptor, &wsnadpt_data_svc);
	mod_unregister(gw, &mod_wsnadaptor);

	gw_msgq_destroy(wsnadpt_msgq);
	gw_mutex_destroy(mod_wsnadaptor.lock);

	//free sn_data and node_data
	//sn_data_free();

//	wsnadpt_config_cleanup();

	return TRUE;
}



json_object *serialize(gw_t *gw)
{

//	return TRUE;
}
/**
 * @}
 */


