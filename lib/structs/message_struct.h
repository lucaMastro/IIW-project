//#include <stdint.h>
//#include <stdio.h>


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
	uint16_t flag;
	uint16_t length;
	unsigned char data[MSS];
} Message;
#endif
