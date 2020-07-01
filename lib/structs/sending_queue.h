#define RECEIVE_WINDOW	8		
#define Tsec			0
#define Tnsec			2000000 //2 ms
#define p 				40 //40% lost	

#ifdef ADAPT_TO
#define Tmin	2000000 //2 ms
#define a 	0.125  
#define b 	0.25
#endif

typedef struct sending_queue{
	Message **on_fly_message_queue;
	//uint8_t on_fly_num_mess;
	uint8_t send_base;
	int semaphore;
	uint8_t next_seq_num;
	int cmd_sock;
	int data_sock;
	int num_on_fly_pack;
}Sending_queue;
