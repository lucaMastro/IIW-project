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


#define SENDING_WINDOW	9		
#define Tsec			0
#define Tnsec			20000000 //20 ms
#define p 				40 //40% lost	

#ifdef ADAPT_TO
#define Tmin	2000000 //2 ms
#define a 	0.125  
#define b 	0.25
#endif

typedef struct sending_queue{
	Message **on_fly_message_queue;
	uint8_t send_base;
	int semaphore;
	uint8_t next_seq_num;
	int cmd_sock;
	int data_sock;
	int num_on_fly_pack;
	int should_read_data;
	char *buf;

}Sending_queue;
