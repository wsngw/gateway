/*
 * atcmddriver.h
 *
 *  Created on: 2010-12-3
 *      Author: makui
 */

#ifndef ATCMDDRIVER_H_
#define ATCMDDRIVER_H_

#define MAX_PKT_SIZE 		240
#define MAX_UART_BUFSIZE 	512

#define HEADERSIZE			10
#define ACK_PKT_LEN			20
#define NID_SIZE			8


#define MAJOR_VER			0x1
#define MINOR_VER			0x0

enum uartcmd {
	AT_CMD_RDY = 0,
	AT_CMD_REG,
	AT_CMD_UNREG,
	AT_CMD_CONFIG,
	AT_CMD_QUERY,
	AT_CMD_KEEPALIVE,
	AT_CMD_DATA,
	AT_CMD_MAX,

	AT_CMD_RDY_ACK = 0x08000,
	AT_CMD_REG_ACK,
	AT_CMD_UNREG_ACK,
	AT_CMD_CONFIG_ACK,
	AT_CMD_QUERY_ACK,
	AT_CMD_KEEPALIVE_ACK,
	AT_CMD_DATA_ACK,
	AT_CMD_ACK_MAX

};


enum uartcapstate {
	UART_CAP_INIT = 1,
	UART_CAP_HEAD1,
	UART_CAP_HEAD2,
	UART_CAP_LEN1,
	UART_CAP_LEN2,
	UART_CAP_CMD1,
	UART_CAP_CMD2,
	UART_CAP_OK,
	UART_CAP_MAX
};

struct uart_data
{
	int32_t 	filefd;
	int32_t managefd;   // file to save data
	int32_t appdatafd;
	uint32_t sn_id; // match sn_id
	uint16_t DstSrvID;
	uint32_t recv_pkt_num;
	uint32_t recv_data_pkt_num;
	enum uartcapstate state;
	uint32_t bufsize;
	uint32_t pkt_left;
	char_t * write_pos;
	uint32_t pktlen;
	uint32_t cmd_id;
	char_t *pktbuf;
	uint32_t packet_serial;
};

int32_t set_opt(int32_t fd,int32_t nSpeed, int32_t nBits, char_t nEvent, int32_t nStop);
int32_t open_port(int32_t fd,int32_t comport);
int32_t uart_data_init(struct uart_data *pinfo, int32_t filefd, uint32_t bufsize );
int32_t write_complete(int32_t fd, char_t *buf, uint32_t len);
int32_t process_uart_uplink_packet(gw_t *gw, struct uart_data *pinfo);
void process_buf_data( gw_t *gw, struct uart_data *pinfo, char_t *data, uint32_t length);

#endif /* ATCMDDRIVER_H_ */
