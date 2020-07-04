/*	relation between numeration interval I and sending window N:
 *
 *			      I
 *			N <= ---
 *				  2
 *
 *	then, if I is a k-bit value:
 *			
 *			N <= 2^(k - 1)
 *
 * ***************************************************************
 *
 * I is a 8 bit field. then:
 * 		SENDING_WINDOW <= 2^7 = 128
 *
 *
 *	*/			  



#define SENDING_WINDOW	30	
#define Tsec			0	
#define Tnsec			50000000 //50 ms
#define p 				20 //% lost	

#ifdef ADAPT_TO

	#define Tmin	2000000 //2 ms
	#define a 		0.125  
	#define b 		0.25

#endif

typedef struct sending_queue{
	Message **on_fly_message_queue; //messages buffering queue 
	uint8_t send_base;
	int semaphore;	
	uint8_t next_seq_num;
	int cmd_sock;
	int data_sock;
	int num_on_fly_pack;

	int should_read_data;			//boolean to check if reading from ack
	char *buf;						//buffer for data read from ack

}Sending_queue;
