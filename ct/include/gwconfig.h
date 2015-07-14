/*
 * gwconfig.h
 *
 *  Created on: 2010-11-2
 *      Author: makui
 */

#ifndef GWCONFIG_H_
#define GWCONFIG_H_

#include <gwtypes.h>


extern int32_t  gw_config_parse(char_t *filename, int32_t (*mod_config_parse)(char_t *, char_t *));


#endif /* GWCONFIG_H_ */
