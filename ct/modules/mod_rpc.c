/**
 * @brief module rpc routines
 *  This module encode gateway state information in Json string form, and send to client web-browser to display
 * @file mod_rpc.c
 * @author wanghl  <hlwang2001@163.com>
 *
 * @addtogroup Plugins_RPC Gateway Json-RPC module
 * @{
 */
#include <stdio.h>
#include <gwtypes.h>
#include <gwmutex.h>
#include <gwthread.h>
#include <module.h>
#include <log.h>
#include <msg.h>
#include <dotconf/dotconf.h>
#include <httpd/platform.h>
#include <httpd/microhttpd.h>
#include <json/json.h>
#include <rpc/jsonrpc.h>

#define RPC_SERVICE_ID	10

/* module rpc config information */
typedef struct __tag_rpc_config
{
	int32_t		http_port;
	int32_t     http_postbuffersize;
	int32_t		http_maxanswersize;
	int32_t 	http_maxnamesize;
//	bstring ip_address;
	bstring httpd_start_mode;
	bstring homepage_filename;

}rpc_conf_t;

static gw_mod_t mod_rpc;
static gw_mod_data_t rpc_data;
static gw_svc_t rpc_svc;
static gw_msgq_t *rpc_msgq;
static gw_thread_t rpc_thread = NULL;
static rpc_conf_t rpc_conf;
static struct MHD_Daemon *httpdaemon;

/*------------ httpd related info ---------*/
#define GET		0
#define POST	1

#define MUST_FREE  1
#define NO_FREE  0
struct connection_info_struct
{
  int32_t connectiontype;
  int32_t must_free_flag;
  char_t *answerstring;
  struct MHD_PostProcessor *postprocessor;
};

const char_t *askpage = "<html><body>\
                       What's your name, Sir?<br>\
                       <form action=\"/namepost\" method=\"post\">\
                       <input name=\"name\" type=\"text\"\
                       <input type=\"submit\" value=\" Send \"></form>\
                       </body></html>";

const char_t *greatingpage =
  "<html><body><h1>Welcome, %s!</center></h1></body></html>";

const char_t *errorpage =
  "<html><body>Unknow method! \nThis doesn't seem to be right.</body></html>";

const char_t *jsonrsponsepage =
  "{\"Key\":\"Value\",\"jsonrpc\":\"result data\",\"post count\": \"you are the %d visitor\", \"acknowledge\": \"Thanks for visiting\"}";

const char_t *unkwonpostpage =
  "<html><body> You have post unkown mesg type. </body></html>";

bool_t init(gw_t *gw, gw_dlib_t lib);
bool_t cleanup(gw_t *gw);
bool_t serialize(gw_t *gw, bstring *buf);
static DOTCONF_CB(default_callback);

static const configoption_t options[] = {
	{"http_port", ARG_INT, default_callback, NULL, 0},
	{"http_postbuffersize", ARG_INT, default_callback, NULL, 0},
	{"http_maxanswersize", ARG_INT, default_callback, NULL, 0},
	{"http_maxnamesize", ARG_INT, default_callback, NULL, 0},
	{"", ARG_NAME, default_callback, NULL, 0},
	LAST_OPTION
};

static int32_t rpc_config(char_t *path_filename)
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

DOTCONF_CB(default_callback)
{
	int32_t i, arg_count;
	bstring command;
	bstring tmpcmd;

	gw_debug_print("default_callback handler called for \"%s\". Got %d args\n", cmd->name,
	       cmd->arg_count);
	for (i = 0; i < cmd->arg_count; i++)
		gw_debug_print("\tArg #%d: %s\n", i, cmd->data.list[i]);

	command = bfromcstr(cmd->name);
	arg_count = cmd->arg_count;
	/*
	tmpcmd = bfromcstr("ip_address");
	if ( bstricmp(command, tmpcmd) == 0 ) {
		rpc_conf.ip_address = bfromcstr(cmd->data.list[0]);
		gw_debug_print("the ip address is %s\n", rpc_conf.ip_address->data);
		goto exit;
	}
	*/

	tmpcmd = bfromcstr("http_postbuffersize");
	if ( bstricmp(command, tmpcmd) == 0 )
	{
		rpc_conf.http_postbuffersize = cmd->data.value;
		printf("the http_postbuffersize is set to %d\n", rpc_conf.http_postbuffersize);
		goto tail;
	}

	bassigncstr(tmpcmd, "http_port");
	if ( bstricmp(command, tmpcmd) == 0 )
	{
		rpc_conf.http_port = cmd->data.value;
		printf("the http_port is set to %d\n", rpc_conf.http_port);
		goto tail;
	}

	bassigncstr(tmpcmd, "http_maxanswersize");
	if ( bstricmp(command, tmpcmd) == 0 )
	{
		rpc_conf.http_maxanswersize = cmd->data.value;
		printf("the http_maxanswersize is set to %d\n", rpc_conf.http_maxanswersize);
		goto tail;
	}

	bassigncstr(tmpcmd, "http_maxnamesize");
	if ( bstricmp(command, tmpcmd) == 0 )
	{
		rpc_conf.http_maxnamesize = cmd->data.value;
		printf("the http_maxnamesize is set to %d\n", rpc_conf.http_maxnamesize);
		goto tail;
	}

	bassigncstr(tmpcmd, "homepage_filename");
	if ( bstricmp(command, tmpcmd) == 0 )
	{
		rpc_conf.homepage_filename = bfromcstr(cmd->data.list[0]);
		printf("the homepage_filename is set to %s\n", rpc_conf.homepage_filename->data);
		goto tail;
	}

tail:
	bdestroy(command);
	bdestroy(tmpcmd);

	return NULL;
}

/*
void *rpc_svc_handler(void *args)
{
	gw_msg_t *msg;
	bool_t quit = FALSE;

	while (!quit && ((msg = gw_msg_recv(rpc_msgq)) != NULL))
	{
		switch (msg->type)
		{
		case GW_MSG_SYSTEM_QUIT:
			quit = TRUE;
			break;
		default:
			gw_log_print(GW_LOG_LEVEL_WARNING, "unknown message type %u received by service %u, module %s\n", \
					msg->type, rpc_svc.id, mod_rpc.name);
			break;
		}
		gw_msg_free(msg);
	}

	gw_thread_exit(0);

	return NULL;
}
*/

static int32_t
send_http_page (struct MHD_Connection *connection, const char_t *page, int32_t free_flag)
{
  int32_t ret;
  struct MHD_Response *response;
  if (free_flag == MUST_FREE)
	  response =
	    MHD_create_response_from_data (strlen (page), (void *) page, MUST_FREE, MHD_NO);
  else
	  response =
	    MHD_create_response_from_data (strlen (page), (void *) page, MHD_NO, MHD_NO);

  if (!response)
    return MHD_NO;

  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);

  return ret;
}

static int32_t send_http_homepage(struct MHD_Connection *connection, char_t * file)
{
	int32_t fd;
	int32_t ret;
	struct stat sbuf;
	struct MHD_Response *response;
	if ((-1 == (fd = open(rpc_conf.homepage_filename->data, O_RDONLY)))	|| (0 != fstat (fd, &sbuf))	)
	{
		/* error accessing file */
		gw_debug_print("\ncan't open homepage file!\n");
		if (fd != -1)
			close(fd);
		const char_t *errorstr =
						"<html><body>An internal server error has occured! </body></html>";
		response = MHD_create_response_from_data(strlen(errorstr),
				(void *) errorstr, MHD_NO, MHD_NO);
		if (response)
		{
			ret = MHD_queue_response(connection,
					MHD_HTTP_INTERNAL_SERVER_ERROR, response);
			MHD_destroy_response(response);

			return MHD_YES;
		}
		else
			return MHD_NO;
	}

	response = MHD_create_response_from_fd(sbuf.st_size, fd);
	MHD_add_response_header(response, "Content-Type", "text/html");
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	return ret;
}

static int32_t
post_msg_postprocess (void *coninfo_cls, enum MHD_ValueKind kind, const char_t *key,
              const char_t *filename, const char_t *content_type,
              const char_t *transfer_encoding, const char_t *data, uint64_t off,
              size_t size)
{
  struct connection_info_struct *con_info = coninfo_cls;

  gw_debug_print("post mesg from client recved\n");
  if (0 == strcmp (key, "name"))
    {
      if ((size > 0) && (size <= rpc_conf.http_maxnamesize))
        {
          char_t *answerstring;
          answerstring = malloc (rpc_conf.http_maxanswersize);
          if (!answerstring)
            return MHD_NO;

          snprintf (answerstring, rpc_conf.http_maxanswersize, greatingpage, data);
          con_info->answerstring = answerstring;
          con_info->must_free_flag = MUST_FREE;
        }
      else {
        con_info->answerstring = NULL;
        con_info->must_free_flag = NO_FREE;
      }

      if(con_info->answerstring != NULL)
      	gw_debug_print("name posted  recved!\n");

      return MHD_YES;
    }

  if (0 == strcmp (key, "rpc_request"))
      {
	    con_info->answerstring = NULL;
	    if ((size > 0) && (size <= rpc_conf.http_maxnamesize)) {
	    	 char_t * rpc_response;
	    	 /*
 	    	    char_t* response_text;
	    	    struct json_object *request;
	    	    struct json_object *response;

	    	    request = json_tokener_parse(request_text);
	    	    response = json_object_new_object();

	    	    jsonrpc_service(request, response);

	    	    char_t* text = json_object_to_json_string(response);
	    	    response_text = Milk(strlen(text) + 1);
	    	    strcpy(response_text, text);

	    	    json_object_put(request);
	    	    json_object_put(response);

	    	    return response_text;
	    	    */
	    	 gw_debug_print("jspnrpc request recved\n");
	         rpc_response = jsonrpc_process(data);
             con_info->answerstring = rpc_response;
	        // con_info->answerstring =jsonrsponsepage;
             con_info->must_free_flag = MUST_FREE;
	    }

        if(con_info->answerstring != NULL)
        	gw_debug_print("Client posted Json string recved!\n");

        return MHD_YES;
      }
  // unknow key
  gw_debugprint("unkown post mesg recved\n");
  con_info->answerstring = unkwonpostpage;
  return MHD_YES;
}

static void
request_completed_callback (void *cls, struct MHD_Connection *connection,
                   void **con_cls, enum MHD_RequestTerminationCode toe)
{
  struct connection_info_struct *con_info = *con_cls;

  if (NULL == con_info)
    return;

  // if this connection is http POST type
  if (con_info->connectiontype == POST)
      MHD_destroy_post_processor (con_info->postprocessor);

  free (con_info);
  *con_cls = NULL;
}

static int32_t
answer_to_connection_callback (void *cls, struct MHD_Connection *connection,
                      const char_t *url, const char_t *method,
                      const char_t *version, const char_t *upload_data,
                      size_t *upload_data_size, void **con_cls)
{
  if (NULL == *con_cls) // indicate a new connection
    {
      struct connection_info_struct *con_info;

      con_info = malloc (sizeof (struct connection_info_struct));
      if (NULL == con_info)
        return MHD_NO;
      con_info->answerstring = NULL;
      con_info->must_free_flag = NO_FREE;

      // test for http connect!
      if (0 == strcmp (method, "POST"))
        {
          con_info->postprocessor =
            MHD_create_post_processor (connection, rpc_conf.http_postbuffersize,
            		post_msg_postprocess, (void *) con_info);

          if (NULL == con_info->postprocessor)
          {
              free (con_info);
              return MHD_NO;
            }
          else
        	  con_info->connectiontype = POST;
          gw_debug_print("Set up a new connection for mesg Post!\n");
        }
      else {
    	  con_info->connectiontype = GET;
    	  gw_debug_print("Set up a new connection for mesg GET!\n");
      }

      *con_cls = (void *) con_info;

      return MHD_YES;
    }

  /*---not a new connection ---- */
  if (0 == strcmp (method, "GET")) {
//	  gw_debug_print("Http GET recved!\n");
	  return send_http_homepage(connection, rpc_conf.homepage_filename->data);
    }

  if (0 == strcmp (method, "POST"))
    {
      struct connection_info_struct *con_info = *con_cls;
      gw_debug_print("recv a post mesg\n");
      if (*upload_data_size != 0)
        {
          MHD_post_process (con_info->postprocessor, upload_data, *upload_data_size);
          *upload_data_size = 0;
          return MHD_YES;
         }
      else if (NULL != con_info->answerstring)
        return send_http_page (connection, con_info->answerstring,con_info->must_free_flag);
      else
    	  return send_http_page (connection, unkwonpostpage,NO_FREE);
    }
   // unkown http method
   return send_http_page (connection, errorpage,NO_FREE);
}

/*****************************************************************************/


void gw_serialize( struct json_object *request, struct json_object *response )
{
	static int32_t post_count = 1;
	struct json_object *tmp_response;
	static char_t answerjsonstring[1024];

	snprintf (answerjsonstring, 1024, jsonrsponsepage, post_count++);
	printf("answerstring = %s\n",answerjsonstring);
	tmp_response = json_tokener_parse(answerjsonstring);
	json_object_object_add(response,"result",tmp_response);
	printf("jsonstring = %s\n",json_object_to_json_string(response));

	//response = jsonrsponsepage;
	gw_debug_print("gw_serialize() called\n");


}

/*****************************************************************************/

void jsonrpc_init(void)
{
	jsonrpc_add_method("gw.rpc.getall", gw_serialize);
}

bool_t init(gw_t *gw, gw_dlib_t lib)
{
	bool_t ret = TRUE;

	gw_debug_print("mod_rpc init() called\n");

	/* initialize module data */
	memset(&rpc_data, 0, sizeof(rpc_data));

	/* read config infromation */
	rpc_config("/home/makui/projects/gw/modules/rpc.conf");

	/*--- initialize jsonrpc ----*/
	jsonrpc_init();

	/* initialize module structure */
	memset(&mod_rpc, 0, sizeof(mod_rpc));
	mod_rpc.name = "mod_rpc";
	mod_rpc.desc = "RPC over http";
	mod_rpc.vendor = "WSN";
	mod_rpc.modver.major = 1;
	mod_rpc.modver.minor = 0;
	mod_rpc.sysver.major = 1;
	mod_rpc.sysver.minor = 0;
	mod_rpc.init = init;
	mod_rpc.cleanup = cleanup;
	mod_rpc.serialize = serialize;
	mod_rpc.lib = lib;
	mod_rpc.data = &rpc_data;
	mod_rpc.lock = gw_mutex_create();
	INIT_LIST_HEAD(&mod_rpc.services);

	rpc_msgq = gw_msgq_create();
	memset(&rpc_svc, 0, sizeof(rpc_svc));
	rpc_svc.id = RPC_SERVICE_ID;
	rpc_svc.name = "rpc_service";
	rpc_svc.mod = &mod_rpc;
	rpc_svc.mq = rpc_msgq;

//	rpc_thread = gw_thread_create(rpc_svc_handler, NULL);
	httpdaemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG, rpc_conf.http_port, NULL, NULL,
	                             &answer_to_connection_callback, NULL,
	                             MHD_OPTION_NOTIFY_COMPLETED, request_completed_callback,
	                             NULL, MHD_OPTION_END);
	  if (NULL == httpdaemon) {
		  gw_debug_print("\n start RPC httpd deamon failed\n");
		  return FALSE;
	  }
	  else
		  gw_debug_print("\n RPC httpd deamon started\n");



	//TODO: initialize another stuff
	if (mod_rpc.lock != NULL)
	{
		ret = mod_register(gw, &mod_rpc);
/*
		if (ret)
		{
			ret = mod_register_service(gw, &mod_rpc, &rpc_svc);
			if (ret != TRUE)
			{
				mod_unregister(gw, &mod_rpc);
				gw_mutex_destroy(mod_rpc.lock);
				gw_log_print(GW_LOG_LEVEL_ERROR, "module %s initialization failed\n", mod_rpc.name);
			}
			else
			{
				gw_log_print(GW_LOG_LEVEL_INFO, "module %s initialized\n", mod_rpc.name);
			}
		}
*/
	}

	return ret;
}

bool_t cleanup(gw_t *gw)
{
	/*
	//TODO: cleanup module stuff
	gw_msg_t *msg;

	msg = gw_msg_alloc();
	msg->type = GW_MSG_SYSTEM_QUIT;
	msg->dstsvc = rpc_svc.id;
	msg->srcsvc = rpc_svc.id;
	gw_msg_send(gw, msg);

	gw_thread_wait(rpc_thread);
	*/
	gw_debug_print("mod_rpc cleanup start\n");
	MHD_stop_daemon (httpdaemon);

//	mod_unregister_service(gw, &mod_rpc, &rpc_svc);
	mod_unregister(gw, &mod_rpc);

	gw_msgq_destroy(rpc_msgq);
	gw_mutex_destroy(mod_rpc.lock);

	return TRUE;
}

bool_t serialize(gw_t *gw, bstring *buf)
{
	return TRUE;
}
/**
 * @}
 */
