/*
 * log.c
 *
 *  Created on: 2010-11-2
 *      Author: makui
 *   Copyright: 2010, WSN
 */

#include <log.h>
#include <stdio.h>
#include <time.h>
#include <bstrlib.h>

#define MAX_LOG_LEN		1024

static FILE *fp = NULL;
static gw_log_backend_t b = GW_LOG_BACKEND_NONE;
gw_log_level_t global_log_level = GW_LOG_LEVEL_ERROR;

bool_t gw_log_open(gw_log_backend_t backend, const char_t *file)
{
	bool_t ret = FALSE;

	if (b == GW_LOG_BACKEND_NONE)
	{
		switch (backend)
		{
		case GW_LOG_BACKEND_FILE:
			if ((fp = fopen(file, "a")) != NULL)
			{
				ret = TRUE;
				b = backend;
			}
			break;
		case GW_LOG_BACKEND_STDIO:
			b = backend;
			ret = TRUE;
			break;
		default:
			b = GW_LOG_BACKEND_NONE;
			break;
		}
	}

	return ret;
}

void gw_log_close(void)
{
	if (b != GW_LOG_BACKEND_NONE)
	{
		switch (b)
		{
		case GW_LOG_BACKEND_FILE:
			if (fp != NULL)
			{
				fclose(fp);
				fp = NULL;
			}
			break;
		case GW_LOG_BACKEND_STDIO:
		default:
			break;
		}

		b = GW_LOG_BACKEND_NONE;
	}
}

void __gw_log_print(gw_log_level_t level, const char_t *fn, uint32_t line, ...)
{
	va_list args;
	bstring str;
	const char_t *fmt;
	time_t tm;
	pid_t pid;

	if (b != GW_LOG_BACKEND_NONE)
	{
		time(&tm);
		str = bfromcstr("[");
		bcatcstr(str, ctime(&tm));
		btrunc(str, blength(str) - 1);
		bcatcstr(str, "]");

		switch (level)
		{
		case GW_LOG_LEVEL_DEBUG:
			pid = getpid();
			bformata(str, "[%s, %u, %u][DEBUG] ", fn, line, pid);
			break;
		case GW_LOG_LEVEL_INFO:
			bformata(str, "[%s, %u][INFO] ", fn, line);
			break;
		case GW_LOG_LEVEL_WARNING:
			bformata(str, "[%s, %u][WARNING] ", fn, line);
			break;
		case GW_LOG_LEVEL_ERROR:
			bformata(str, "[%s, %u][ERROR] ", fn, line);
			break;
		case GW_LOG_LEVEL_CRITICAL:
			bformata(str, "[%s, %u][CIRTICAL] ", fn, line);
			break;
		default:
			break;
		}

		va_start(args, line);
		fmt = va_arg(args, const char*);
		bvcformata(str, MAX_LOG_LEN, fmt, args);
		va_end(args);

		switch (b)
		{
		case GW_LOG_BACKEND_FILE:
			if (fp != NULL)
			{
				fprintf(fp, "%s", (char*)(str->data));
				fflush(fp);
			}
			break;
		case GW_LOG_BACKEND_STDIO:
			printf("%s", (char*)(str->data));
		default:
			break;
		}
	}

	bdestroy(str);
}
