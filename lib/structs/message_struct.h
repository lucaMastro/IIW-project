#include <stdint.h>
#include <stdio.h>

#ifndef MESSAGE_H
# define MESSAGE_H 
	
#define HEADER_SIZE 17
#define MSS 500

#define SYN 				1
#define SYN_ACK 			2
#define ACK 				4
#define FIN 				8
#define LIST 				16
#define GET 				32
#define PUT 				64
#define CHAR_INDICATOR 		128
#define END_OF_DATA			256

typedef struct message{
	uint32_t seq_num;
	uint32_t ack_num;
	uint16_t flag;
	//uint16_t new_port;
	uint16_t rec_win;
	uint32_t length;
	unsigned char list_data[MSS];
//	FILE *file_data;
} Message;
#endif
