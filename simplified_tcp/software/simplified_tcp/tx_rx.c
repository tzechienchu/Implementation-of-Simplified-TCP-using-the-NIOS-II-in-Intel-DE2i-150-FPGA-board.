/*
 * tx_rx.c

 *
 *  Created on: Dec 8, 2016
 *      Author: Darshan,Avinash,Aravind
 */
 /*** NOTE : please interchange the MAC addresses and PORT numbers on the other board before running the C code in eclipse ***************/

#include "system.h"
#include <unistd.h>
#include <stdio.h>
#include <altera_avalon_sgdma.h>
#include <altera_avalon_sgdma_descriptor.h> //include the sgdma descriptor
#include <altera_avalon_sgdma_regs.h>//include the sgdma registers
#include <altera_avalon_pio_regs.h>//include the PIO registers
#include "sys/alt_stdio.h"
#include "sys/alt_irq.h"
#include <string.h>
#include <stdint.h>
#include<sys/alt_cache.h>
#include<altera_avalon_timer.h>
#include<altera_avalon_timer_regs.h>

/********* Initialize variables *****************/
//uint8_t n = 0;
uint8_t loss_count = 0;
uint8_t loss_flag = 0;
int pckt_rxed = 0;
int succ_rxed = 0;
uint8_t flag_syn = 0;
uint8_t retr_count = 0;
int in=0;
uint8_t data_flag;

/********** Function Prototypes ******************/
void statistics_counter();
void rx_ethernet_isr (void *context);
void create_pkt();
/*********** STRUCTS *******************************/
struct tcp_conn {
	uint8_t link;
	unsigned char dest_mac[6];
	unsigned char source_mac[6];
	unsigned short source_port ;
	unsigned short dest_port;
	unsigned short ack_num;
	unsigned short seq_num;
};

/*********** Create a receive frame ****************/
unsigned char rx_frame[64] = { 0 };
unsigned char tx_frame[64] = { 0 };

/************* Create sgdma transmit and receive devices **************/

alt_sgdma_dev * sgdma_tx_dev;
alt_sgdma_dev * sgdma_rx_dev;

/**************  Allocate descriptors in the descriptor_memory (onchip memory) *************/
alt_sgdma_descriptor tx_descriptor		__attribute__ (( section ( ".descriptor_memory" )));
alt_sgdma_descriptor tx_descriptor_end	__attribute__ (( section ( ".descriptor_memory" )));

alt_sgdma_descriptor rx_descriptor  	__attribute__ (( section ( ".descriptor_memory" )));
alt_sgdma_descriptor rx_descriptor_end  __attribute__ (( section ( ".descriptor_memory" )));

/************* Tcp connection structures *********************/

struct tcp_conn TCP[2];

/******* MAIN BLOCK STARTS HERE ********************************/

int main(void){
	loss_flag=0;
	loss_count=0;

/**** Initializing timer status and control values *************/
	IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 0x0000);
	IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0x0000);

/****** copying TCP header values to structure ****************************/
	memmove(&TCP[0].dest_mac,"\x00\x1C\x23\x17\x4A\xCA",6);
	memmove(&TCP[0].source_mac,"\x00\x1C\x23\x17\x4A\xCB",6);
	memmove(&TCP[0].source_port, "\x00\x10", 2);
	memmove(&TCP[0].dest_port, "\x00\x20", 2);
	memmove(&TCP[0].seq_num, "\x00\x00", 2);
	memmove(&TCP[0].ack_num, "\x00\x00", 2);
	data_flag = 0;

/*********** Open the sgdma transmit device ********************/
	sgdma_tx_dev = alt_avalon_sgdma_open ("/dev/sgdma_tx");
	if (sgdma_tx_dev == NULL) {
		alt_printf ("Error: could not open scatter-gather dma transmit device\n");
		//return -1;
	} else alt_printf ("Opened scatter-gather dma transmit device\n");

/***********  Open the sgdma receive device *************************/

	sgdma_rx_dev = alt_avalon_sgdma_open ("/dev/sgdma_rx");
	if (sgdma_rx_dev == NULL) {
		alt_printf ("Error: could not open scatter-gather dma receive device\n");
		//return -1;
	} else alt_printf ("Opened scatter-gather dma receive device\n");

/**************** Set interrupts for the sgdma receive device , Create sgdma receive descriptor  & Set up non-blocking transfer of sgdma receive descriptor **********/
	alt_avalon_sgdma_register_callback( sgdma_rx_dev, (alt_avalon_sgdma_callback) rx_ethernet_isr, 0x00000014, NULL );
	alt_avalon_sgdma_construct_stream_to_mem_desc( &rx_descriptor, &rx_descriptor_end, (alt_u32 *)rx_frame, 0, 0 );
	alt_avalon_sgdma_do_async_transfer( sgdma_rx_dev, &rx_descriptor );

/********************* Triple-sp **********************************************/
	volatile int *tse = (int *)TSE_BASE;
	// Specify the addresses of the PHY devices to be accessed through MDIO interface
	*(tse + 0x0F) = 0x10;

/************ Disable read and write transfers and wait**************************/
	*(tse + 0x02 ) = *(tse + 0x02) | 0x00800220;
	while ( *(tse + 0x02 ) != ( *(tse + 0x02 ) | 0x00800220 ));

/****************MAC FIFO Configuration*****************************************/
	*(tse + 0x09) = TSE_TRANSMIT_FIFO_DEPTH-16;
	*(tse + 0x0E) = 3 ;
	*(tse + 0x0D) = 8;
	*(tse + 0x07) =TSE_RECEIVE_FIFO_DEPTH-16;
	*(tse + 0x0C) = 8 ;
	*(tse + 0x0B) = 8 ;
	*(tse + 0x0A) = 0;
	*(tse + 0x08) = 0;

/***************** Initialize the MAC address************************************/
	*(tse + 0x03 ) = 0x17231C00  ; //mac_0
	*(tse + 0x04) =  0x0000CB4A;  //mac_1

/****************** MAC function configuration**********************************/
	*(tse + 0x05) = 1518 ;
	*(tse + 0x17) = 12;
	*(tse + 0x06 ) = 0xFFFF;
	*(tse + 0x02 ) = 0x00800220; //command config


/*************** Software reset the PHY chip and wait***************************/
	*(tse + 0x02  ) =  0x00802220;
	while ( *(tse + 0x02 ) != ( 0x00800220 ) ) alt_printf("h") ;

/*** Enable read and write transfers, gigabit Ethernet operation and promiscuous mode*/

	*(tse + 0x02 ) = *(tse + 0x02 ) | 0x0080023B;
	while ( *(tse + 0x02 ) != ( *(tse + 0x02) | 0x0080023B ) );


	uint8_t data = 0x00;
	uint8_t flag_a = 0;
	int delay = 0;
	int n; //variable to read timer status

/********** Infinite Loop ******************************/
	while (1) {
		in =  IORD_ALTERA_AVALON_PIO_DATA(SWITCH_BASE); //read the input from the switch

		/**** Check if 3 way handshake already established *********************/
		if((TCP[0].link == 1) && (flag_syn == 0) && (in & 0x01)==1){
			flag_syn = 1;
			alt_printf("\nConnection already established, send data");
		}

		if ((in & 0x01)== 1){// Syn if  switch 1 is on
			//IOWR_ALTERA_AVALON_PIO_DATA(LED_BASE,0x01); //switch on or switch off the LED
			if(flag_syn == 0){
				flag_syn = 1; //setting flag bit to send only 1 syn packet when switch is ON
				flag_a = 1;

				/******** Creating and Sending SYN packet******************************/
		/** SYN = 1 , SYN-ACK = 5 , ACK = 4 , FIN  = 2 , FIN-ACK = 6
		 **** DATA = 8 , DATA ACK = 0X0C*****************************************/
				create_pkt();
				memmove(tx_frame+24, "\x01", 1);
				alt_printf(" timer started for SYN, status is : %x",IORD_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE));

				/***** Transmitting the packet ***************************************/
				alt_printf("\nSYN TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x \n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25]);
				alt_dcache_flush_all();
				alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
				alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
				while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);

				/********** Starting Timer for SYN packet *************************************/

				IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0x0002);
				IOWR_ALTERA_AVALON_TIMER_PERIODL(TIMER_0_BASE , 0xFFFF);
				IOWR_ALTERA_AVALON_TIMER_PERIODH(TIMER_0_BASE , 0xFFFF);
				IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 0x0007);
				/********** increment sequence number **************************************/
				TCP[0].seq_num++;
			}
		}

		/****** Condition for sending FIN Packet when switch 1 is OFF **********************************/

		else if((flag_a == 1 )&& (in & 0x01) == 0){
				data_flag = 0;
				flag_syn = 0;
				flag_a=0;
		/******** Sending Fin packet ***************************************************/;

				create_pkt();
				memcpy(tx_frame+24, "\x02", 1);
				alt_printf("\nFIN TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x \n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25]);
				alt_dcache_flush_all();
				alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
				alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
				while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);

		/************ Starting Timer for FIN Packet *******************************/
				IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0x0002);
				IOWR_ALTERA_AVALON_TIMER_PERIODL(TIMER_0_BASE , 0xAAAA);
				IOWR_ALTERA_AVALON_TIMER_PERIODH(TIMER_0_BASE , 0xFFFF);
				IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 0x0007);
				TCP[0].seq_num++;
		}

	/********* condition to check DATA transmission if both switch 1 and 2 are ON *************/
		if(in == 3){
			delay++;
	/***** setting delay to display LED blinking  *******************************/
			if(data_flag == 1 && delay == 1000000){
				delay = 0;
				data_flag = 0;
				create_pkt();
				data++;
				memcpy(tx_frame+24, "\x08",1);
				memcpy(tx_frame+25, &data, 1);
				alt_printf("\n DATA TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x  %x\n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25],tx_frame[26]);
				alt_dcache_flush_all();
				alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
				alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
				while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);

	/*** Starting Timer for DATA transmission  ****************************************/

				IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0x0002);
				IOWR_ALTERA_AVALON_TIMER_PERIODL(TIMER_0_BASE , 0xAAAA);
				IOWR_ALTERA_AVALON_TIMER_PERIODH(TIMER_0_BASE , 0x002F);
				IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 0x0007);
				TCP[0].seq_num++;
			}
			if (delay > 1000000){
				delay=0;
			}
		}

/******* Condition to check for re transmission *************************************/
/*** Stop Retransmitting and exit if retransmit count reaches 50 (actual ethernet exits after count 16) ********************/
		n = IORD_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE);
		if(n == 3){
			retr_count++;
				if(retr_count>=50){
					retr_count = 0;
					alt_printf("\n retransmission limit exceeded, DISCONNECTED!!!");
					break;
						}
/*** Stopping Timer if already running and starting timer for Data transmission ******************************************/
			IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 0x0008);
			IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0x0002);

			//Transmit Function
			alt_printf("\n RE TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x \n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25]);
			alt_dcache_flush_all();
			alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
			alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
			while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);

			/************************ start timer******************************************/
			IOWR_ALTERA_AVALON_TIMER_PERIODL(TIMER_0_BASE , 0xAAAA);
			IOWR_ALTERA_AVALON_TIMER_PERIODH(TIMER_0_BASE , 0x02FF);
			IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 0x0007);
			alt_printf("\nRetx timer started for SYN, status:%d  count:%d,",IORD_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE));

		}
	}
	return 0;
}

/****************************************************************************************
 * Subroutine to read incoming Ethernet frames
****************************************************************************************/
void rx_ethernet_isr (void *context)
{
	retr_count = 0;
	pckt_rxed++;
	in =  IORD_ALTERA_AVALON_PIO_DATA(SWITCH_BASE); //read the input from the switch

/***** If switch 3 is ON drop receiving packets ***************************************/

	if((in == 4)||(in == 5)||(in == 6)||(in == 7)){
			while (alt_avalon_sgdma_check_descriptor_status(&rx_descriptor) != 0);
			// Create new receive sgdma descriptor
			alt_avalon_sgdma_construct_stream_to_mem_desc( &rx_descriptor, &rx_descriptor_end, (alt_u32 *)rx_frame, 0, 0 );
			// Set up non-blocking transfer of sgdma receive descriptor
			alt_avalon_sgdma_do_async_transfer( sgdma_rx_dev, &rx_descriptor );
			return;
	}

	alt_printf("\nRX_FRAME: %x %x %x %x %x %x   %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x  %x  %x\n",rx_frame[2],rx_frame[3],rx_frame[4],rx_frame[5],rx_frame[6],rx_frame[7],rx_frame[8],rx_frame[9],rx_frame[10],rx_frame[11],rx_frame[12],rx_frame[13],rx_frame[14],rx_frame[15],rx_frame[16],rx_frame[17],rx_frame[18],rx_frame[19],rx_frame[20],rx_frame[21],rx_frame[22],rx_frame[23],rx_frame[24],rx_frame[25],rx_frame[26]);
	alt_dcache_flush_all();

	IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 0x0008);
	IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0x0000);

	succ_rxed++; // successfully received frame incrementing upon reception
	/**** check if SYN frame is received *********************************************/
		if ( rx_frame [24] == 0x01) {
			TCP[0].link = 1;

	/******* Sending SYN-ACK Frame ***************************************/

			TCP[0].ack_num++;
			create_pkt();
			memmove(tx_frame+24,"\x05",1);//Setting syn-ack

			//transmit_packet();
			alt_printf("\n SYN/ACK TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x \n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25]);
			alt_dcache_flush_all();
			alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
			alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
			while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);
			TCP[0].seq_num++;
		}

	/******************** Syn-Ack recieved *************************************************/
		else if((rx_frame[24]) == 0x05){
			TCP[0].link = 1;

	/**************** Send ACknowledgement ************************************************/
			TCP[0].ack_num++;
			create_pkt();
			memmove(tx_frame+24,"\x04",1);//setting ack

			//transmit_packet();
			alt_printf("\n Conn Est Ack TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x \n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25]);
			alt_dcache_flush_all();
			alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
			alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
			while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);

			data_flag = 1;
			TCP[0].seq_num++;
		}

	/***************** FIN Recieved *************************************************/
		else if((rx_frame[24]) == 0x02){
			TCP[0].link = 0;
			data_flag = 0;
	/***************** Sending FIN-ACK **********************************************/
			TCP[0].ack_num++;
			create_pkt();
			memmove(tx_frame+24,"\x06",1);

			//transmit_packet();
			alt_printf("\n FIN/ACK TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x \n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25]);
			alt_dcache_flush_all();
			alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
			alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
			while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);
			TCP[0].seq_num++;
		}

	/***************** Fin-Ack received **************************************************************/

		else if(rx_frame[24] == 0x06){
			TCP[0].link = 0;
			//ack_send();
			TCP[0].ack_num++;
			create_pkt();
			memmove(tx_frame+24,"\x04",1);//setting ack

			//transmit_packet();
			alt_printf("\n Conn Closed ACK TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x \n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25]);
			alt_dcache_flush_all();
			alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
			alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
			while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);
			alt_printf("\n Connection closed");
			alt_printf("\n********************* Statistics - TCP **********************");
			alt_printf("\nRxed Packets : %x",pckt_rxed);
			alt_printf("\nSuccessfull Packets Rxed : %x",succ_rxed);
			alt_printf("\nDropped Packets : %x",(pckt_rxed - succ_rxed));
			statistics_counter();

			TCP[0].seq_num++;
		}

	/*****************  ACK Received *******************************************************/
		else if(rx_frame[24] == 0x04){
				//ack();
			if(TCP[0].link == 1){
				alt_printf("\n Connection establishment ack received");
				data_flag = 1;
				TCP[0].ack_num++;
			}
			else {

	/***************** PRINT TCP STATISTICS ***********************************************/
				alt_printf("\n Connection closed");
				alt_printf("\n********************* Statistics - TCP **********************");
				alt_printf("\nRxed Packets : %x",pckt_rxed);
				alt_printf("\nSuccessfull Packets Rxed : %x",succ_rxed);
				alt_printf("\nDropped Packets : %x",(pckt_rxed - succ_rxed));

	/********** Calling function for printing Ethernet statistics ***************************/
				statistics_counter();
				TCP[0].ack_num++;
			}
		}

/******************Data pkt received and sent to LED's**********/
		else if(rx_frame[24] == 0x08){
			TCP[0].ack_num++;
			IOWR_ALTERA_AVALON_PIO_DATA(LED_BASE,rx_frame[25]);
			create_pkt();
			memmove(tx_frame+24,"\x0C",1);
			alt_printf("\n DATA ACK TX_FRAME: %x %x %x %x %x %x    %x %x %x %x %x %x    %x %x   %x %x   %x %x   %x %x   %x %x   %x  %x \n",tx_frame[2],tx_frame[3],tx_frame[4],tx_frame[5],tx_frame[6],tx_frame[7],tx_frame[8],tx_frame[9],tx_frame[10],tx_frame[11],tx_frame[12],tx_frame[13],tx_frame[14],tx_frame[15],tx_frame[16],tx_frame[17],tx_frame[18],tx_frame[19],tx_frame[20],tx_frame[21],tx_frame[22],tx_frame[23],tx_frame[24],tx_frame[25],tx_frame[26]);
			alt_dcache_flush_all();
			alt_avalon_sgdma_construct_mem_to_stream_desc(&tx_descriptor, &tx_descriptor_end, (alt_u32 *)tx_frame, 64, 0, 1, 1, 0);
			alt_avalon_sgdma_do_async_transfer( sgdma_tx_dev, &tx_descriptor );
			while (alt_avalon_sgdma_check_descriptor_status(&tx_descriptor) != 0);
			TCP[0].seq_num++;
		}
	/********************* DATA ACKNOWLEDGEMENT RECEIVED***********************************/
		else if(rx_frame[24] == 0x0C){
			TCP[0].ack_num++;
			alt_printf("\n Data ACK received");
			data_flag = 1;
		}

	// Wait until receive descriptor transfer is complete
	while (alt_avalon_sgdma_check_descriptor_status(&rx_descriptor) != 0);
	// Create new receive sgdma descriptor
	alt_avalon_sgdma_construct_stream_to_mem_desc( &rx_descriptor, &rx_descriptor_end, (alt_u32 *)rx_frame, 0, 0 );
	// Set up non-blocking transfer of sgdma receive descriptor
	alt_avalon_sgdma_do_async_transfer( sgdma_rx_dev, &rx_descriptor );


}

/********* Function for Creating Packet before transmission ***********************/
void create_pkt(){
	memset(tx_frame+26,0,37);
	memcpy(tx_frame+20, &TCP[0].seq_num, 2);
	memcpy(tx_frame+22, &TCP[0].ack_num, 2);
	memcpy(tx_frame+24, "\x00", 1);
	memmove(tx_frame, "\x00\x00",2);
	memmove(tx_frame+2, &TCP[0].dest_mac,6);
	memmove(tx_frame+8, &TCP[0].source_mac,6);
	memmove(tx_frame+14, "\x2E\x00", 2);
	memmove(tx_frame+16, &TCP[0].source_port, 2);
	memmove(tx_frame+18, &TCP[0].dest_port, 2);
}

/******* Printing  Ethernet Statistics *********************************************/
void statistics_counter(){
	volatile int *tse = (int *)TSE_BASE;
	alt_printf("\n********************* Statistics - Ethernet **********************");
	alt_printf("\nnum frames successfully received: %x ", *(tse + 0x1B));
	alt_printf("\nnum error frames received: %x ", *(tse + 0x22));
	alt_printf("\nnum frames correctly received: %x ", *(tse + 0x1B) - *(tse + 0x22));
	alt_printf("\n******************************************************************");
}

