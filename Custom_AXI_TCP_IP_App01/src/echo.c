/*
 * Copyright (C) 2009 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <string.h>
#include "xil_io.h"

#include "lwip/err.h"
#include "lwip/tcp.h"
#if defined (__arm__) || defined (__aarch64__)
#include "xil_printf.h"
#endif


#define HW_ACC_PatternIn				0x43C00000
#define HW_ACC_Pattern_Count_Total 		0x43C00004
#define HW_ACC_Threshold 				0x43C00008
#define HW_ACC_Intr_Rst					0x43C0000C
#define HW_ACC_BASE_ADDR				0x7AA00000



int transfer_data() {
	return 0;
}

void print_app_header()
{
#if (LWIP_IPV6==0)
	xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
#else
	xil_printf("\n\r\n\r-----lwIPv6 TCP echo server ------\n\r");
#endif
	xil_printf("TCP packets sent to port 6001 will be echoed back\n\r");
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb,
                               struct pbuf *p, err_t err)
{
	/* do not read the packet if we are not in ESTABLISHED state */
	if (!p) {
		tcp_close(tpcb);
		tcp_recv(tpcb, NULL);
		return ERR_OK;
	}
	static u8_t flag = 1;
	/* indicate that the packet has been received */
	tcp_recved(tpcb, p->len);
//**********************	User Application Definition 	*****************************************//
	xil_printf("Data Received from Client \r\n");
	u16_t RecvBuff[p->len];
	for (u16_t i = 0 ; i < p->len ; i++ ){
		RecvBuff[i] = ((char *)p->payload)[i] ;
		xil_printf(" %x ",RecvBuff[i]);

	}
	xil_printf(" \r\n ");

	if ( (RecvBuff[0] == 0x68) && (RecvBuff[1] == 0x70) ){
		Xil_Out32(HW_ACC_PatternIn,RecvBuff[2]);
		xil_printf(" Pattern in has been set to : 0x%x \n\r" , Xil_In32(0x7AA00000));
	}
	if ( (RecvBuff[0] == 0x68) && (RecvBuff[1] == 0x69) ){
		Xil_Out32(HW_ACC_Threshold,RecvBuff[2]);
		xil_printf(" Threshold is : 0x%x \n\r" , Xil_In32(HW_ACC_Threshold));
	}
	if ( (RecvBuff[0] == 0x68) && (RecvBuff[1] == 0x6A) ){
		xil_printf(" Total Pattern Count is : %d \n\r" ,Xil_In32(HW_ACC_Pattern_Count_Total));
	}
	if ( (RecvBuff[0] == 0x68) && (RecvBuff[1] == 0x6B) ){
		xil_printf(" Interrupt has been Reset \n\r" );
	}
	if ( (RecvBuff[0] == 0x68) && (RecvBuff[1] == 0x78) ){
		static u32_t BaseMEM_ADDR = HW_ACC_BASE_ADDR ;
		BaseMEM_ADDR = RecvBuff[2] + HW_ACC_BASE_ADDR ;
		Xil_Out32(BaseMEM_ADDR , (RecvBuff[6]<<24)|(RecvBuff[5]<<16)|(RecvBuff[4]<<8)|(RecvBuff[3]));
		xil_printf(" At 0x%x Addr , Repeated Data is %d \n\r" , BaseMEM_ADDR , Xil_In32(BaseMEM_ADDR));
		if (flag){
			BaseMEM_ADDR = HW_ACC_BASE_ADDR ;
			flag = 0;
		}
	}


//***************************************************************************************************//
	/* echo back the payload */
	/* in this case, we assume that the payload is < TCP_SND_BUF */
	if (tcp_sndbuf(tpcb) > p->len) {
		err = tcp_write(tpcb, p->payload, p->len, 1);
	} else
		xil_printf("no space in tcp_sndbuf\n\r");

	/* free the received pbuf */
	pbuf_free(p);

	return ERR_OK;
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	static int connection = 1;
	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	/* just use an integer number indicating the connection id as the
	   callback argument */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	/* increment for subsequent accepted connections */
	connection++;

	return ERR_OK;
}


int start_application()
{
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;

	/* create new TCP PCB structure */
	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb, NULL);

	/* listen for connections */
	pcb = tcp_listen(pcb);
	if (!pcb) {
		xil_printf("Out of memory while tcp_listen\n\r");
		return -3;
	}

	/* specify callback to use for incoming connections */
	tcp_accept(pcb, accept_callback);

	xil_printf("TCP echo server started @ port %d\n\r", port);

	return 0;
}
