#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <httpd/platform.h>
#include <httpd/microhttpd.h>
#include <time.h>

#define PORT 8888
#if 0
#define FILENAME "/home/makui/projects/Sunset.jpg"
#define MIMETYPE "image/jpg"
#endif

#if 0
#define FILENAME "/home/makui/projects/test.json"
#define MIMETYPE "application/json"
#endif

#define FILENAME "/home/makui/projects/json2html/testjs1.html"
// #define MIMETYPE "text/html"


static int answer_to_connection(void *cls, struct MHD_Connection *connection,
		const char *url, const char *method, const char *version,
		const char *upload_data, size_t *upload_data_size, void **con_cls)
{
	struct MHD_Response *response, *rsp_post;
	int fd;
	int ret;
	struct stat sbuf;

	if (0 == strcmp(method, "GET")) {
		if ((-1 == (fd = open(FILENAME, O_RDONLY)))
		|| (0 != fstat (fd, &sbuf))
		)
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
	//	MHD_add_response_header(response, "Content-Type", MIMETYPE);
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);

		return ret;
	}

	if (0 == strcmp(method, "POST")) {
		const char
				*rspstr =
						"<html><body>An POST mesg has received!\
	                          </body></html>";
		printf("recved POST mesg from client\n");
		rsp_post = MHD_create_response_from_data(strlen(rspstr),
				(void *) rspstr, MHD_NO, MHD_NO);
		if (rsp_post== NULL)
		{
			ret = MHD_queue_response(connection,
					MHD_HTTP_INTERNAL_SERVER_ERROR, rsp_post);
			MHD_destroy_response(response);
			printf("can't create MHD response!\n");

			return MHD_YES;
		}
		ret = MHD_add_response_header(rsp_post, "Content-Type", "text/html");
		if (ret == MHD_NO)
			printf("error add header\n");
		ret = MHD_queue_response(connection, MHD_HTTP_OK, rsp_post);
		if(ret == MHD_NO)
			printf("rsponse queue error\n");
		else
			printf("rsponse queue ok\n");
		MHD_destroy_response(rsp_post);
	}
}

int responsehdr_test(void)
{
	struct MHD_Daemon *daemon;

	printf("MHD deamon starting.....\n");
	daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG, PORT, NULL, NULL,
			&answer_to_connection, NULL, MHD_OPTION_END);
	if (NULL == daemon)
	{
		printf("MHD deamon failed.....\n");
		return 1;
	}
	printf("MHD deamon started.....\n");
	getchar();

	getchar();

	MHD_stop_daemon(daemon);

	return 0;
}
