/*
 * log.h
 *
 *  Created on: 2010-11-2
 *      Author: makui
 *   Copyright: 2010, WSN
 */

#ifndef __LOG_H
#define __LOG_H

#include <gw.h>

typedef enum __tag_gw_log_level
{
	GW_LOG_LEVEL_DEBUG,
	GW_LOG_LEVEL_INFO,
	GW_LOG_LEVEL_WARNING,
	GW_LOG_LEVEL_ERROR,
	GW_LOG_LEVEL_CRITICAL,
} gw_log_level_t;

typedef enum __tag_gw_log_type
{
	GW_LOG_BACKEND_NONE = 0,
	GW_LOG_BACKEND_STDIO,
	GW_LOG_BACKEND_FILE
} gw_log_backend_t;

extern gw_log_level_t global_log_level;

#define gw_log_set_level(level) 				\
	do											\
	{											\
		global_log_level = level;				\
	} while (__LINE__ == -1)

#define gw_log_print(level, ...)																\
	do																							\
	{																							\
		if (level >= global_log_level)															\
		{																						\
			__gw_log_print((gw_log_level_t)level, __FILE__, (uint32_t)__LINE__, __VA_ARGS__);	\
		}																						\
	} while (__LINE__ == -1)

#ifdef GW_DEBUG
#define gw_debug_print(...)																\
	do																							\
	{																							\
		__gw_log_print(GW_LOG_LEVEL_DEBUG, __FILE__, (uint32_t)__LINE__, __VA_ARGS__);	\
	} while (__LINE__ == -1)
#else
#define gw_debug_print(...)
#endif

bool_t gw_log_open(gw_log_backend_t backend, const char_t *file);
void gw_log_close(void);
void __gw_log_print(gw_log_level_t level, const char_t *fn, uint32_t line, ...);

#endif /* __LOG_H */
