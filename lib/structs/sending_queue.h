
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
