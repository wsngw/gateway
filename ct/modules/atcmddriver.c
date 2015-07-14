/**
 * @brief wsnadaptor's AT command handle routines \n
 *  this file received uart data from sink, parse it to AT command, and process AT command
 *
 * @file atcommanddriver.c
 * @author wanghl  <hlwang2001@163.com>
 *
 * @addtogroup Plugins_wsnadaptor Gateway wsn-adaptor module
 * @{
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <gw.h>
#include <msg.h>
#include <atcmddriver.h>
#include <msgdef.h>

/**
 * @brief set uart's options
 *
 * This function set uart's baudrate, data bits, stop bits, checksum options
 *
 * @param fd opened uart file
 * @param nSpeed  uart's baudrate,eg. 4800, 9600,..., 115200
 * @param nBits data bits
 * @param nEvent odd, even or no check, value is 'O' or 'E' or 'N'
 * @param nStop stop bit, value can be 0,1,2
 * @return 0 on success, -1 on failure
 */
int32_t set_opt(int32_t fd,int32_t nSpeed, int32_t nBits, char_t nEvent, int32_t nStop)
{
    struct termios newtio,oldtio;
    if ( tcgetattr( fd,&oldtio) != 0) { 
        perror("SetupSerial 1");
        return -1;
    }
    bzero( &newtio, sizeof( newtio ) );
    newtio.c_cflag |= CLOCAL | CREAD; 
    newtio.c_cflag &= ~CSIZE; 
    switch( nBits )
    {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }
    switch( nEvent )
    {
    case 'O':
    case 'o':
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= (INPCK | ISTRIP);
        break;
    case 'e':
    case 'E':
        newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        break;
    case 'n':
    case 'N':
        newtio.c_cflag &= ~PARENB;
        break;
    }
switch( nSpeed )
    {
    case 2400:
        cfsetispeed(&newtio, B2400);
        cfsetospeed(&newtio, B2400);
        break;
    case 4800:
        cfsetispeed(&newtio, B4800);
        cfsetospeed(&newtio, B4800);
        break;
    case 9600:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    case 19200:
        cfsetispeed(&newtio, B19200);
        cfsetospeed(&newtio, B19200);
        break;
    case 38400:
        cfsetispeed(&newtio, B38400);
        cfsetospeed(&newtio, B38400);
        break;
    case 57600:
        cfsetispeed(&newtio, B57600);
        cfsetospeed(&newtio, B57600);
        break;
    case 115200:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    default:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    }
    if( nStop == 1 )
        newtio.c_cflag &= ~CSTOPB;
    else if ( nStop == 2 )
    newtio.c_cflag |= CSTOPB;
    newtio.c_cc[VTIME] = 1; // critical parameters, 0.1 Second
    newtio.c_cc[VMIN] = 12; // critical parameters
    tcflush(fd,TCIFLUSH);
    if((tcsetattr(fd,TCSANOW,&newtio))!=0)
    {
        perror("com set error");
        return -1;
    }
    printf("set done!\n");
    return 0;
}

/**
 * @brief open uart port
 *
 * This function open specified uart port
 *
 * @param fd opened uart file fd
 * @param comport  uart's number,value can be 1,2,3, and means ttyS0,ttyS1,ttyS2
 *
 * @return 0 on success, -1 on failure
 */
int32_t open_port(int32_t fd,int32_t comport)
{
    char_t *dev[]={"/dev/ttyS0","/dev/ttyS1","/dev/ttyS2"};
    long vdisable;
    if (comport==1)
    {    fd = open( "/dev/ttyS0", O_RDWR|O_NOCTTY ); // O_NDELAY
        if (-1 == fd){
            perror("Can't Open Serial Port");
            return(-1);
        }
        else 
            printf("open ttyS0 .....\n");
    }
    else if(comport==2)
    {    fd = open( "/dev/ttyS1", O_RDWR|O_NOCTTY|O_NDELAY);
        if (-1 == fd){
            perror("Can't Open Serial Port");
            return(-1);
        }
        else 
            printf("open ttyS1 .....\n");
    }
    else if (comport==3)
    {
        fd = open( "/dev/ttyS2", O_RDWR|O_NOCTTY|O_NDELAY);
        if (-1 == fd){
            perror("Can't Open Serial Port");
            return(-1);
        }
        else 
            printf("open ttyS2 .....\n");
    }
    if(fcntl(fd, F_SETFL, 0) < 0)
        printf("fcntl failed!\n");
    else
        printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));
    if(isatty(STDIN_FILENO)==0)
        printf("standard input is not a terminal device\n");
    else
        printf("isatty success!\n");
    printf("fd-open=%d\n",fd);
    return fd;
}


/**
 * @brief open uart port
 *
 * This function init uart_data data structure, which is used for other functions
 *
 * @param pinfo uart_data type pointer
 * @param filefd opened uart file fd
 * @param bufsize size of buffer for this driver,
 * it must be larger than a max AT command size
 *
 * @return 0 on success, -1 on failure
 */
int32_t uart_data_init(struct uart_data *pinfo, int32_t filefd, uint32_t bufsize )
{
	if(filefd <= 0)
		return -1;
	if(bufsize > MAX_UART_BUFSIZE)
		return -1;

	pinfo->bufsize = bufsize;
	pinfo->pktbuf = malloc(bufsize);
	if(pinfo->pktbuf == NULL)
		return -1;

	pinfo->filefd = filefd;
	pinfo->write_pos = pinfo->pktbuf;
	pinfo->pktlen = 0;
	pinfo->state = UART_CAP_INIT;
	pinfo->recv_data_pkt_num = 0;
	pinfo->recv_pkt_num = 0;
};

/**
 * @brief send data in buffer to uart, and return until all finished
 *
 * @param fd opened uart file fd
 * @param buf  pointer to data buffer
 * @param len  length of data to send
 *
 * @return 0 on success
 */
int32_t write_complete(int32_t fd, char_t *buf, uint32_t len)
{
	uint32_t num = len;
	uint32_t nwrite;
	while( num > 0 )
	{
		nwrite = write(fd, buf, num);
		num = num - nwrite;
	}
	return 0;
}

/**
 * @brief proccess AT command
 *
 * This function parse and proccess all received AT command
 *
 * @param gw pointer of gw_t data
 * @param pinfo pointer of struct uart_data
 *
 * @return 0 on success, -1 on failure
 */
int32_t process_uart_uplink_packet(gw_t *gw, struct uart_data *pinfo)
{
	char_t *ptr, *ackbuf;
	uint32_t cmd = 0xFF;
	gw_msg_t *mesg = NULL;
	int32_t i;
//	char_t head[4]={0xD5,0xC8,0x00,0x00};

	cmd = pinfo->pktbuf[4] + (uint32_t)(pinfo->pktbuf[5]<<8);
	cmd &= 0x0000FFFF;

	printf("\nCapture Packet: lenght=%d\t CMD_ID = %x \n", pinfo->pktlen, cmd);
	for(i=0; i < pinfo->pktlen ; i++)
	{
		printf("%x ",*((uint8_t *)pinfo->pktbuf+i));
	}

	if(cmd == AT_CMD_RDY)
	{
		ackbuf = malloc(HEADERSIZE+2);
		if(ackbuf == NULL)
		{
			printf("\nmalloc() error in process_uart_uplink_packet\n");
			return -1;
		}

		ackbuf[0]=0xd5;
		ackbuf[1]=0xc8;
		ackbuf[2]=0x08;
		ackbuf[3]=0x00;
		ptr = pinfo->pktbuf;
		ackbuf[4]= *ptr;	// serial num
		ackbuf[5]= *(ptr+1);
		ackbuf[6]= *(ptr+2);
		ackbuf[7]= *(ptr+3);
		ackbuf[8]= (uint8_t)(AT_CMD_RDY_ACK);
		ackbuf[9]= (uint8_t)((AT_CMD_RDY_ACK >>8));
		ackbuf[10]=0x00;
		ackbuf[11]=0x00;
		write_complete(pinfo->filefd, ackbuf, HEADERSIZE+2);

		free(ackbuf);
		printf("AT_CMD_RDY recved\n");
		return 0;
	}
	else
	{
		mesg = gw_msg_alloc();
		printf("south interface msg alloc!!!!!!!!!!!!!!!!\n");
		if(mesg == NULL)
		{
			printf("\nmalloc() error in process_uart_uplink_packet\n");
			return -1;
		}
		mesg->version.major = MAJOR_VER;
		mesg->version.major = MINOR_VER;
		ptr = pinfo->pktbuf;
	//	mesg->serial = *ptr + (*(ptr+1))<<8 + (*(ptr+2))<<16 + (*(ptr+3))<<24;
		mesg->serial = *((uint32_t *)ptr); // little endian
	}
	// process other CMD
	switch(cmd)
	{
	case AT_CMD_KEEPALIVE:
	case AT_CMD_REG:
	case AT_CMD_UNREG:
		// register/unregister/keepalive end device
		mesg->datasize = 0;
		mesg->data = NULL;
		mesg->dstsvc = DEV_MAN_SVC;
		mesg->srcsvc = SN_DM_SVC;
		if (cmd == AT_CMD_KEEPALIVE)
			mesg->type = DEVICE_UPDATE_REQUEST;
		else if (cmd == AT_CMD_REG)
		{
			mesg->type = DEVICE_LOGIN_REQUEST;
			mesg->data = malloc(4);
			if(mesg->data == NULL)
			{
				printf("\nmalloc() error in process_uart_uplink_packet\n");
				return -1;
			}
			*((uint32_t *)mesg->data) = pinfo->sn_id;
			mesg->datasize = 4;
			printf("mesg sn_id = %d\n",*((uint32_t *)mesg->data));
		}
		else
			mesg->type = DEVICE_LOGOUT_REQUEST;
		memcpy(mesg->devid, pinfo->pktbuf+6 , NID_SIZE);
		gw_msg_send(gw, mesg);
		break;

	case AT_CMD_DATA:
		// statistic
		pinfo->recv_data_pkt_num ++;
		// save to file
		printf("CMD_DATA data pkt, len = %d\n", pinfo->pktlen-14);
		write_complete(pinfo->appdatafd, pinfo->pktbuf+14, pinfo->pktlen-14);

		// sink send data to platform
		if(pinfo->DstSrvID != 0 )
			mesg->dstsvc = pinfo->DstSrvID;
		else
			mesg->dstsvc = PLAT_DATA_SVC;
		mesg->srcsvc = SN_DATA_SVC;
		mesg->type = UPSTREAM_DATA;
		memcpy(mesg->devid, pinfo->pktbuf+6 , NID_SIZE);
		mesg->datasize = (pinfo->pktlen- 14);  // del:serial, CMD_ID, NID, add: APP_ID,SP_ID
		ptr = malloc(mesg->datasize);
		if(ptr == NULL)
		{
			gw_msg_free(mesg);
			return -1;
		}

		memcpy(ptr, pinfo->pktbuf + 14, mesg->datasize );
		mesg->data = ptr;
		if(mesg->dstsvc != PLAT_DATA_SVC)
			printf("send mesg to app,Data is %x, %x\n",*ptr&0x0ff,*(ptr+1)&0x0ff);
		gw_msg_send(gw, mesg);
		break;

	case AT_CMD_DATA_ACK:
	case AT_CMD_CONFIG_ACK:
		// process DATA ACK from sink
		if (cmd == AT_CMD_DATA_ACK)
		{
			mesg->srcsvc = SN_DATA_SVC;
			mesg->dstsvc = PLAT_DATA_SVC;
			mesg->type = DOWNSTREAM_DATA_ACK;
		}
		else
		{
			mesg->srcsvc = SN_DM_SVC;
			mesg->dstsvc = DEV_MAN_SVC;
			mesg->type = PASSIVE_CONFIG_RESPONSE;
		}

		memcpy(mesg->devid, pinfo->pktbuf + 6, 8 );
		mesg->retcode = pinfo->pktbuf[14] + (pinfo->pktbuf[15])<<8;
		mesg->datasize = 0;  //
		mesg->data = NULL;
		gw_msg_send(gw, mesg);
		break;

	default:
		// return error: invalid cmd type
		printf("\n unknow AT CMD!\n");
		return -1;
		break;
	}

}

/**
 * @brief proccess AT command
 *
 * This function search valid AT command in the buffer, and store valid data in struct uart_data's buffer
 *
 * @param gw pointer of gw_t data
 * @param pinfo pointer of struct uart_data
 * @param data pointer to the buffer
 * @param length length of data in the buffer
 *
 * @return no return value
 */
void process_buf_data( gw_t *gw, struct uart_data *pinfo, char_t *data, uint32_t length)
{
	uint32_t len_tmp, more_data_flag = 0;
	uint32_t len = length;
	uint8_t *ptr, *ptr_max, *ptr_tmp, ch;
	char_t head[4]={0xD5,0xC8,0x00,0x00};

	printf("start process buf data,len = %d and uart state is %d\n",length, pinfo->state);
cap_len2:
	if (more_data_flag == 0)
		ptr = data;

	if (pinfo->state == UART_CAP_LEN2 )
	{
		if( len > pinfo->pkt_left )
		{
			len_tmp = len - pinfo->pkt_left;
			ptr_tmp = ptr + pinfo->pkt_left;
			more_data_flag = 1;
			len = pinfo->pkt_left;
		}
		else
			more_data_flag = 0;

		memcpy(pinfo->write_pos, ptr, len);
		pinfo->write_pos += len;
		pinfo->pkt_left -= len;
		if(pinfo->pkt_left == 0)
		{
			pinfo->state = UART_CAP_OK;
			process_uart_uplink_packet(gw, pinfo);
			// statistic
			pinfo->recv_pkt_num ++;
			// save to file
			head[2]=pinfo->pktlen;
			head[3]=(pinfo->pktlen>>8);
			write_complete(pinfo->managefd,head, 4);
			write_complete(pinfo->managefd,pinfo->pktbuf, pinfo->pktlen);

			pinfo->state = UART_CAP_INIT;
		}
		if (more_data_flag == 0)
			return;
	}

	if(more_data_flag == 1)
	{
		ptr = ptr_tmp;
		ptr_max = ptr_tmp + len_tmp -1;
		more_data_flag = 0;
	}
	else
	{
		ptr = data;
		ptr_max = data + len -1;
	}


	for(ptr; ptr <= ptr_max; ptr++)
	{
		ch = *ptr;
		switch(pinfo->state)
		{
			case UART_CAP_INIT:
				if (ch == 0xd5)
					pinfo->state = UART_CAP_HEAD1;
				break;

			case UART_CAP_HEAD1:
				if (ch == 0xc8)
					pinfo->state = UART_CAP_HEAD2;
				else if(ch == 0xd5)
					pinfo->state = UART_CAP_HEAD1;
				else
					pinfo->state = UART_CAP_INIT;
				break;

			case UART_CAP_HEAD2:
				pinfo->pktlen = (uint32_t)(ch);
				pinfo->state = UART_CAP_LEN1;
				break;

			case UART_CAP_LEN1:
				pinfo->pktlen += (uint32_t)( ch<<8 );
				if( (pinfo->pktlen > MAX_PKT_SIZE) ||(pinfo->pktlen < 6) ) // because min pkt size is 8
				{
					pinfo->state = UART_CAP_INIT;
					printf("invalid length\n");
				}
				else
					pinfo->state = UART_CAP_LEN2;
				break;

			case UART_CAP_LEN2:
				// start copy data to pktbuf
				break;

			case UART_CAP_CMD1:
				break;

			case UART_CAP_CMD2:
				break;

			case UART_CAP_OK:
				break;
		}

		if(pinfo->state == UART_CAP_LEN2)
			break;  // go out for loop
	}

	if( (pinfo->state == UART_CAP_LEN2) && (ptr < ptr_max) )
	{
		//capture uart packet header
		pinfo->pkt_left = pinfo->pktlen;
		len = ptr_max - ptr ;
		ptr = ptr+1;
		pinfo->write_pos = pinfo->pktbuf;
		more_data_flag = 1;
		goto cap_len2;
	}
}


/*
int32_t uart_test(void)
{
    int32_t fd;
    int32_t nread,nwrite,i;
    static char_t buff[512]="\0";
    if((fd=open_port(fd,1))< 0)
    {
        perror("open_port error");
        return;
    }
    if((i=set_opt(fd,9600,8,'N',1)) < 0)
    {
        perror("set_opt error");
        return;
    }
    printf("uart opened,fd=%d\n",fd);

    while (1)
    {
        nread=read(fd,buff,500);
        buff[nread] = '\0';
        if (nread > 0)
        {
        	printf("nread=%d,%s\n",nread,buff);
        	nwrite = write(fd,buff,nread);
        }
    }
    close(fd);
    return;
}
*/

/*
int32_t wsn_adaptor_uplink_loop( gw_t *gw, struct uart_data *pinfo)  // re-entrant
{
	int32_t len;
	char_t buf[24];

    while (1)
     {
         len = read(pinfo->filefd, buf, 24); // set resv buffersize, block here

         if (len > 0)
         {
        	 printf("data len =%d \n",len);
        	 process_buf_data(gw,pinfo, buf, len);
         }
     }
     close(pinfo->filefd);
}


void uart_adaptor_test( void)
{
    int32_t fd, ret;
	int32_t len;
	struct uart_data uart0_data ;
	gw_t gw;

    if((fd=open_port(fd,1))< 0){
        perror("open_port error");
        return;
    }
    if((ret=set_opt(fd,9600,8,'N',1)) < 0){
        perror("set_opt error");
        return;
    }
    printf("uart opened,fd=%d\n",fd);

	uart_data_init( &uart0_data, fd, 256);

	wsn_adaptor_uplink_loop(&gw, &uart0_data); // loop here, never return
}
*/


/**
 * @}
 */
