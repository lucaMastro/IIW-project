
typedef struct sending_queue{
	Message **window_message;
	Message *send_base;
	uint8_t next_seq_num;
}Sending_queue;
