#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <httpd/platform.h>
#include <httpd/microhttpd.h>

#define PORT            8888
#define POSTBUFFERSIZE  512
#define MAXNAMESIZE     20
#define MAXANSWERSIZE   512

#define GET             0
#define POST            1

#define FILENAME "/home/makui/projects/json2html/testjs1.html"
//#define FILENAME "/home/makui/projects/json2html/bloopletech-json2html-cc493b1/index.htm"

struct connection_info_struct
{
  int connectiontype;
  char *answerstring;
  struct MHD_PostProcessor *postprocessor;
};

static const char *askpage = "<html><body>\
                       What's your name, Sir?<br>\
                       <form action=\"/namepost\" method=\"post\">\
                       <input name=\"name\" type=\"text\"\
                       <input type=\"submit\" value=\" Send \"></form>\
                       </body></html>";

static const char *greatingpage =
  "<html><body><h1>Welcome, %s!</center></h1></body></html>";

static const char *errorpage =
  "<html><body>This doesn't seem to be right.</body></html>";

//const char *jsonrsponsepage =
//  "<html><body>Response to a json string post. </body></html>";
static const char *jsonrsponsepage =
  "{\"Key\":\"Value\",\"Json\":\"jsonString\",\"abc\": 12, \"foo\": \"bar\", \"bool0\": false, \"bool1\": true, \"arr\": [ 1, 2, null, 4 ] }";

static const char *unkwonpostpage =
  "<html><body>Response to a unknown post. </body></html>";

static int
send_page (struct MHD_Connection *connection, const char *page)
{
  int ret;
  struct MHD_Response *response;


  response =
    MHD_create_response_from_data (strlen (page), (void *) page, MHD_NO,
                                   MHD_NO);
  if (!response)
    return MHD_NO;

  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);

  return ret;
}

static int send_html(struct MHD_Connection *connection, char * file)
{
	int fd;
	int ret;
	struct stat sbuf;
	struct MHD_Response *response;
	if ((-1 == (fd = open(FILENAME, O_RDONLY)))	|| (0 != fstat (fd, &sbuf))	)
	{
		/* error accessing file */
		printf("\ncan't open file!\n");
		if (fd != -1)
			close(fd);
		const char
				*errorstr =
						"<html><body>An internal server error has occured!\
                              </body></html>";
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
	printf("file opened\n");
	response = MHD_create_response_from_fd(sbuf.st_size, fd);
	MHD_add_response_header(response, "Content-Type", "text/html");
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	return ret;

}

static int
iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size)
{
  struct connection_info_struct *con_info = coninfo_cls;

  if (0 == strcmp (key, "name"))
    {
      if ((size > 0) && (size <= MAXNAMESIZE))
        {
          char *answerstring;
          answerstring = malloc (MAXANSWERSIZE);
          if (!answerstring)
            return MHD_NO;

          snprintf (answerstring, MAXANSWERSIZE, greatingpage, data);
          con_info->answerstring = answerstring;
        }
      else
        con_info->answerstring = NULL;

      return MHD_NO;
    }

  if (0 == strcmp (key, "method"))
      {
	    con_info->answerstring = NULL;
        if ((size > 0) )
            con_info->answerstring = jsonrsponsepage;

        if(con_info->answerstring != NULL)
        	printf("Json string has recved!!!\n");

        return MHD_NO;
      }

  con_info->answerstring = unkwonpostpage;
  return MHD_YES;
}

static void
request_completed (void *cls, struct MHD_Connection *connection,
                   void **con_cls, enum MHD_RequestTerminationCode toe)
{
  struct connection_info_struct *con_info = *con_cls;

  if (NULL == con_info)
    return;

  if (con_info->connectiontype == POST)
    {
      MHD_destroy_post_processor (con_info->postprocessor);
//      if (con_info->answerstring)
 //       free (con_info->answerstring);
    }

  free (con_info);
  *con_cls = NULL;
}


static int
answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls)
{
  if (NULL == *con_cls)
    {
      struct connection_info_struct *con_info;

      con_info = malloc (sizeof (struct connection_info_struct));
      if (NULL == con_info)
        return MHD_NO;
      con_info->answerstring = NULL;

      if (0 == strcmp (method, "POST"))
        {
          con_info->postprocessor =
            MHD_create_post_processor (connection, POSTBUFFERSIZE,
                                       iterate_post, (void *) con_info);
          printf("MHD POST processor created!\n");

          if (NULL == con_info->postprocessor)
            {
              free (con_info);
              return MHD_NO;
            }

          con_info->connectiontype = POST;
        }
      else
        con_info->connectiontype = GET;

      *con_cls = (void *) con_info;
      printf("NULL GET\n");

      return MHD_YES;
    }

  if (0 == strcmp (method, "GET"))
    {
 //     return send_page (connection, askpage);
	  printf("GET HTML!\n");
	  return send_html(connection, FILENAME);
    }

  if (0 == strcmp (method, "POST"))
    {
      struct connection_info_struct *con_info = *con_cls;

      if (*upload_data_size != 0)
        {
          MHD_post_process (con_info->postprocessor, upload_data,
                            *upload_data_size);
          *upload_data_size = 0;

          return MHD_YES;
        }
      else if (NULL != con_info->answerstring)
        return send_page (connection, con_info->answerstring);
      else
    	  return send_page (connection, unkwonpostpage);
    }

  printf("RSPONS NULL POST\n");
  return send_page (connection, errorpage);
}

int
test_http_post (void)
{
  struct MHD_Daemon *daemon;

  daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG, PORT, NULL, NULL,
                             &answer_to_connection, NULL,
                             MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                             NULL, MHD_OPTION_END);
  if (NULL == daemon)
    return 1;

  getchar ();

  MHD_stop_daemon (daemon);

  return 0;
}
