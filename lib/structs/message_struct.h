//#include <stdint.h>
//#include <stdio.h>

/*	tra l'intervallo di numerazione I e la finestra di trasmissione N 
 *	deve valere:
 *			      I
 *			N <= ---
 *				  2
 *
 *	quindi se I è a k bit:
 *			
 *			N <= 2^(k - 1)
 *
 *		I è a 8 bit. allora N <= 2^7 = 128
 *	*/			  


#ifndef MESSAGE_H
# define MESSAGE_H 
	
#define HEADER_SIZE 	6	
#define MSS				500
#define MAX_SEQ_NUM 	255

#define SYN 				1
#define SYN_ACK 			2
#define ACK 				4
#define FIN 				8
#define LIST 				16
#define GET 				32
#define PUT 				64
#define CHAR_INDICATOR 		128
#define END_OF_DATA			256
#define FILE_NOT_FOUND		512


typedef struct message{
	uint8_t seq_num;
	uint8_t ack_num;
	//uint8_t seq_ack_num; //[<4 bit seq> <4 bit ack>]
	uint16_t flag;
	uint16_t length;
	unsigned char list_data[MSS];
} Message;
#endif
