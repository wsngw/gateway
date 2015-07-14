/*
 * types.h
 *
 *  Created on: 2010-11-1
 *      Author: makui
 */

#ifndef __GWTYPES_H
#define __GWTYPES_H

#include <sys/types.h>

/* DATA TYPES (Compiler Specific) */
typedef char					char_t;  	/* char type */
typedef unsigned char			bool_t;  	/* Boolean type */
typedef unsigned char			uint8_t; 	/* Unsigned  8 bit quantity  */
typedef signed char				int8_t;  	/* Signed    8 bit quantity  */
typedef unsigned short			uint16_t;	/* Unsigned 16 bit quantity  */
typedef signed short			int16_t; 	/* Signed   16 bit quantity  */
typedef unsigned int			uint32_t;	/* Unsigned 32 bit quantity  */
typedef signed int				int32_t; 	/* Signed   32 bit quantity  */
typedef unsigned long long		uint64_t;	/* Unsigned 64 bit quantity  */
typedef signed long long		int64_t; 	/* Signed   64 bit quantity  */
typedef float					fp32_t;
typedef double					fp64_t;

#ifndef NULL
#define NULL			        ((void*)0)
#endif

#ifndef TRUE
#define TRUE			        ((uint8_t)1)
#endif

#ifndef FALSE
#define FALSE   		        ((uint8_t)0)
#endif

typedef void*					gw_handle_t;

#endif /* __GWTYPES_H */
