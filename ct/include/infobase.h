/*
 * public_info.h
 *
 *  Created on: 2010-11-2
 *      Author: makui
 */

#ifndef __INFOBASE_H
#define __INFOBASE_H

#include <gwtypes.h>
#include <list.h>
#include <hash.h>
#include <bstrlib.h>
#include <array.h>
#include <gwmutex.h>
#include <json/json.h>

#define 	NUI_LEN			8

/*******************  DATA TYPES DEFINE *************************/

typedef enum
{

	PROTO_UNKNOWN = 0,
	PROTO_IPV4,
	PROTO_IPV6,
	PROTO_SCT,
	PROTO_ZIGBEE,
	PROTO_ISA100,
	PROTO_WIAPA,
	PROTO_WHART,
	PROTO_6LowPAN,
	/*----add more here---*/

	PROTO_MAX
} net_proto_t;

typedef enum
{

	PHY_UNKNOWN = 0,
	PHY_ETHERNET,
	PHY_WIFI,
	PHY_GSM,
	PHY_GPRS,
	PHY_FIBER,
	PHY_3G,
	PHY_FIELDBUS,
	/*----add more here----*/

	PHY_MAX

} phy_type_t;

typedef enum
{

	UNKNOWN = 0,
	RFD,
	ROUTER,
	ADAPTOR,
	GATWAY,
	COORDINATOR,
	NKW_MANAGER,
	SECUR_MANAGER,
	/*---add more below----*/

	ND_MAX

} nd_class_t;

typedef struct __tag_location
{

	fp32_t longtitude;
	fp32_t latitude;
	fp32_t altitude;
	bool_t in_or_outdoor;
} location_t;

typedef enum
{

	QOS_NONE = 0, QOS_PRIORITY, QOS_CONGESTION,
	/**----add more here---**/

	QOS_MAX

} qos_ctrl_t;

typedef enum
{

	POWER_UNKOWN = 0, POWER_MAIN, POWER_BATTERY, POWER_SELF_GEN,

	POWER_MAX
} power_suply_t;

typedef struct __tag_power
{

	power_suply_t type;
	fp32_t energy_cur; // mAh
	fp32_t energy_max; // mAh
	fp32_t energy_percent;

} power_t;

typedef enum
{

	STA_UNKNOWN = 0,
	STA_INIT,
	STA_REG_TO_SN,
	STA_REG_TO_PLAT,
	STA_LOST_REG,
	STA_PN_DISCON,
	STA_SN_DISCON,
	STA_ERROR,
	STA_RESET,

	STA_MAX

} status_t;

typedef struct __tag_group
{

	uint32_t gid; // group id
	uint32_t group_size;
	uint16_t id_list[100];
} group_node_t;

typedef struct __tag_sn_data
{

	struct list_head sn_list;
	struct list_head node_list;
	net_proto_t protocal;
	uint16_t pan_id;
	uint32_t sn_id; //
	uint32_t node_num;
	uint32_t reg_node_num;
	uint32_t profile_id;
	uint64_t up_data;
	uint64_t down_data;
	void *module;
	void *usrdata;
	bstring comment;

} sn_data_t;

typedef struct __tag_node_data
{

	struct list_head entry;
	sn_data_t *pdata;
	char_t sw_version[2];
	char_t hw_version[2];
	uint32_t manufacturer_id;
	char_t nui[NUI_LEN]; // key for node hash
	uint16_t id;
	uint32_t update_interval;
	time_t last_update_time;
	nd_class_t node_type;
	location_t location;
	power_t power_info;
	bstring comment;
	status_t state;
	uint64_t up_data;
	uint64_t down_data;
	UT_hash_handle hh;

/*------- more info here ------*/

} node_data_t;

typedef struct __tag_gw_data
{
	uint8_t sw_version[2];
	uint8_t hw_version[2];
	uint32_t manufacturer_id;
	char_t nui[NUI_LEN];
	uint32_t model;
	location_t gw_location;
	char_t *time; // use Linux time type
	qos_ctrl_t gw_qos;
	status_t gw_state;
	bstring comment;

/*------- more info here ------*/

} gw_data_t;

typedef struct __tag_sn_head
{
	struct list_head sn_list;
	uint32_t sn_num;
	uint32_t sn_id_gen;
	node_data_t *hash_head;
	gw_mutex_t mutex;
} sn_head_t;

/********** GW config data  **********/

typedef struct __tag_gw_config
{
	bstring nui;
	UT_array *module;
	bool_t daemon; //frontdesk or daemon
	bstring logpath;
	bstring pidpath;
	bstring cfgpath;
} gw_config_t;

/********** API definition  **********/

extern void gw_infobase_init(void);
extern void gw_infobase_destroy(void);
extern gw_data_t *gw_data_get(void);
extern char_t *gw_data_serialize(gw_data_t *gw_data, uint32_t flag);

void dev_infobase_init(void);
bool_t dev_infobase_destroy(void);
json_object *dev_infobase_serialize(uint32_t flag);

extern sn_data_t *sn_data_get(uint32_t sn_id);
extern sn_data_t *sn_data_get_first(void);
extern sn_head_t *sn_data_head(void);
extern sn_data_t *sn_get_next_safe(sn_data_t *curr, sn_data_t **next);
extern sn_data_t *sn_data_new(void);
extern bool_t sn_data_add(sn_data_t * sn_data);
extern bool_t sn_data_del(sn_data_t * sn_data);
extern bool_t sn_data_free(sn_data_t * sn_data);
//extern char_t *sn_data_serialize(sn_data_t *sn_data, uint32_t flag);
extern json_object *sn_data_serialize(sn_data_t *sn_data, uint32_t flag);

extern node_data_t *node_data_new(void);
extern node_data_t *node_data_get(int8_t *nui, int32_t len);
extern bool_t node_data_free(node_data_t *node_data);
extern bool_t node_data_add(sn_data_t *sn_data, node_data_t *node_data);
extern bool_t node_data_del(node_data_t *node_data);
//extern char_t *node_data_serialize(node_data_t *node_data, uint32_t flag);
extern json_object *node_data_serialize(node_data_t *node_data, uint32_t flag);

extern gw_config_t *gw_config_get(void);

#endif /* __INFOBASE_H */
