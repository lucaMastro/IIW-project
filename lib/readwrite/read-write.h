
void *make_packet(Message *mess_to_fill, void *read_data_from_here, int seq_num, int ack_num, int flag_to_set){

	//assuming mess_to_fill still initialized
	//the buffer read_data_from_here contains the entire bytes to send.
	//this means it's a FILE* or a char*
	
	int bytes_read;
	void* return_addr = NULL;
	int min;
	int str_len;

	//filling mex
	mess_to_fill -> seq_num = seq_num;
	mess_to_fill -> ack_num = ack_num;
	mess_to_fill -> flag = flag_to_set;

	if (read_data_from_here != NULL){
		//find the data type
		if ( !(CHAR_INDICATOR & flag_to_set) ){ //its a FILE*
			//reading MSS bytes
			bytes_read = fread(mess_to_fill -> data, 1, MSS, 
					(FILE*) read_data_from_here);

			mess_to_fill -> length = bytes_read;
		}
		else{
			//its a string
			//
			//detecting the size of data
			str_len = strlen((char*)read_data_from_here);
			min = str_len < MSS ? str_len : MSS;

			mess_to_fill -> length = min;
			memcpy(mess_to_fill -> data, (unsigned char*) read_data_from_here,
					mess_to_fill -> length);

			//in the next call, i will use this pointer as read_data_from_here:
			//if stringlen > MSS, i need more than one message. then, i have
			//to know where next message's data start.
			return_addr = (unsigned char*)read_data_from_here + min;
		}
	}
	else
		mess_to_fill -> length = 0;

	if (mess_to_fill -> length < MSS)
		mess_to_fill -> flag += END_OF_DATA;
	
	return return_addr;
}


int is_packet_lost(){
	//simulating loss
	int n =  rand() % 101 ; 
	return ( n < p); 
}


void send_data(int data_sock, int cmd_sock, void *data, int type,
		char *read_from_ack){

	int len_ser = HEADER_SIZE;
	unsigned char *new_data_pointer = (unsigned char*) data;
	int flag = 0;
	int sending_sock;
	int is_command = 0;
	struct sembuf sops;

	pthread_t tid;

	//initializing queue struct
	Sending_queue *queue = (Sending_queue*) 
		malloc(sizeof(Sending_queue));
	if (queue == NULL){
		perror("error in malloc of sendingqueue");
		exit(EXIT_FAILURE);
	}

	initialize_struct(queue);

	//initialize struct for semaphore decrement
	sops.sem_flg = 0;
	sops.sem_num = 0;
	sops.sem_op = -1;

	//randomize the results of is_packet_lost()
	srand(time(0));

	//making semaphore
	int sem = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if (sem < 0){
		perror("error semaphore");
		exit(EXIT_FAILURE);
	}

	//initializing semaphore at SENDING_WINDOW. i can send at least
	//SENDING_WINDOW messages without an ack
	if (semctl(sem, 0, SETVAL, SENDING_WINDOW) < 0){ 
		perror("error initializing sem");
		exit(EXIT_FAILURE);
	}

	queue -> semaphore = sem;
	queue -> cmd_sock = cmd_sock;
	queue -> data_sock = data_sock;

	//initializing thread
	pthread_create(&tid, NULL, waiting_for_ack, (void*) queue);

	do{
		//trying decrement. if call is interrupted, restart it
		while (semop(queue -> semaphore, &sops, 1)< 0) {
			if (errno != EINTR){
				perror("error trying get coin");
				exit(EXIT_FAILURE);
				
			}
		}
		
		Message *mex = (Message *)
			malloc(sizeof(Message));
		if (mex == NULL){
			perror("error malloc");
			exit(EXIT_FAILURE);
		}
		mex -> flag = type;
		memset((void*) mex -> data, 0, MSS);

		//if is command message, the queue -> next_seq_num is 0
		if ( (is_command = is_command_mex(mex)) ){
			sending_sock = cmd_sock;
			queue -> send_base = 0;
			queue -> should_read_data = 1;
			queue -> next_seq_num = 0;
		}
		else
			sending_sock = data_sock;

		//making packet with data
		if ( (type & CHAR_INDICATOR) == CHAR_INDICATOR)
			new_data_pointer = (unsigned char *) 
				make_packet(mex, new_data_pointer, 
				queue -> next_seq_num, //seq_num
				0, 					   //ack_num
				type);
		else
			make_packet(mex, data, 
				queue -> next_seq_num, 
				0, 
				type);

		//storing message
		queue -> on_fly_message_queue[mex -> seq_num] =	mex;

		//increasing seq_num
		queue -> next_seq_num = (queue -> next_seq_num + 1) % 
			(MAX_SEQ_NUM + 1);

		if ( !is_packet_lost() )
			send_packet(sending_sock, mex, NULL);

		//increasing num of on fly packets
		queue -> num_on_fly_pack ++;
		
		//save flag for exiting
		flag = mex -> flag;
		
	} 
	while( (flag & END_OF_DATA) != END_OF_DATA ); //exiting when last is
												  //sent	

	//waiting for thread retrasmission on ack
	if (pthread_join(tid, NULL) < 0){
		perror("error in join");
		exit(EXIT_FAILURE);
	}

	//if message was a cmd, storing data in the buffer
	if (is_command)
		sprintf(read_from_ack, "%s", queue -> buf);

	free(queue);
}


void receive_data(int data_sock, int cmd_sock, void *write_here, 
		int *save_here_flag){

	int max_size = HEADER_SIZE + MSS;
	int str_len = 0;
	int old_str_len;
	uint16_t flag = 0;
	uint8_t expected_seq_num = 1;
	
	Message ack;

	//making first ack message
	make_packet(&ack, NULL, 0, 0, ACK);
	do {
		//reading data mex
		Message *mex;
		mex = receive_packet(data_sock, NULL);
		if (mex == NULL){
			perror("error receiving packet in receive_data");
			exit(EXIT_FAILURE);
		}

		//store flag for server-side check
		if (save_here_flag != NULL )
			*save_here_flag = flag;
			
		//checking if expected seq_num
		if (mex -> seq_num == expected_seq_num){

			//saving flag for ending
			flag = mex -> flag;

			//writing data only if there are bytes to write
			if (mex -> length > 0)
				write_data(mex, write_here, mex -> data, 
						&str_len, &old_str_len);	
			
			//changing ack num
			ack.ack_num = expected_seq_num; 

			//increasing expected seq num
			expected_seq_num = (expected_seq_num + 1) % (MAX_SEQ_NUM + 1);
		}

		//sending ack. if received expected seq num, ack -> num is changed
		//else, it is an old ack
		if ( !is_packet_lost() )
			send_packet(cmd_sock, &ack, NULL);
	}
	while ((flag & END_OF_DATA) != END_OF_DATA);

	//check if last ack is lost:
	check_last_ack(cmd_sock, data_sock, &ack);
}

void check_last_ack(int cmd_sock, int data_sock, Message *ack){
	struct timeval to;
	to.tv_sec = Tsec <<2;
	to.tv_usec = (Tnsec / 1000)<<2; //nano secs to micro secs
	Message *m;
	while(1){
		setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
		m = receive_packet(data_sock, NULL);
		if (m == NULL){ //read(..) -> -1 
			if (errno != EAGAIN){
				perror("error in last receive_pack of receive_data");
				exit(EXIT_FAILURE);
			}	
			else
				break;
			
		}
		if (ack != NULL && !is_packet_lost()){
			send_packet(cmd_sock, ack, NULL);
		}
		free(m);
	}
	free(m); //if break is executed, m is not freed inside while

	//deleting timeout
	to.tv_sec = 0;
	to.tv_usec = 0; 
	setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
}


void write_data(Message *mex, void *dest, unsigned char *src, int *str_len, 
		int *old_str_len){
	//this function write on buf or string checking on flag. this understand
	//buffer dest type at runtime
	
	//checking flag
	if ( (mex -> flag & CHAR_INDICATOR) != 0){

		//next operations are done because the string message could be 
		//divided in 2 different messages
		*old_str_len = *str_len;
		*str_len += mex -> length;

		unsigned char **str_dest = (unsigned char **) dest;
		*str_dest = (unsigned char*) realloc(*str_dest, (*str_len + 1));
		if (*str_dest == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);	
		}
		//it write on the correct position: it doesnt have to overwrite
		//what was written in the first message
		memcpy( (*str_dest) + (*old_str_len), mex -> data, 
				mex -> length);
		*(*str_dest + *str_len) = '\0';
	}
	else
		fwrite(src, 1, mex -> length, (FILE *) dest); //writing mex -length
													  //bytes on file
	//freeding read mex
	free(mex);
}

int is_command_mex(Message *mex){
	return (mex -> flag & (ACK | SYN | SYN_ACK | PUT | GET | LIST | FIN));
}


void send_packet(int sockfd, Message *mex, struct sockaddr_in *to){
	//send a single packet

	ssize_t bytes_sent;
	int max_size = HEADER_SIZE + mex -> length;	

	//serializing a message. buffer is allocated in serialize_message()
	unsigned char* packet_ser = serialize_message(mex);

	if (to == NULL)
		bytes_sent = write(sockfd, packet_ser, max_size);
	else
		bytes_sent = sendto(sockfd, packet_ser, max_size, 0,
				(struct sockaddr*)to, sizeof(*to));

	if (bytes_sent < 0){
		perror("error in sending bytes");
		exit(EXIT_FAILURE);
	}

	free(packet_ser);
}

Message *receive_packet(int sockfd, struct sockaddr_in *from){
	//receive a single packet

	ssize_t n_read;
	int max_size = HEADER_SIZE + MSS;
	unsigned char *tmp;

	//making buffer where read
	unsigned char *serialized = (unsigned char *) malloc(sizeof(char) * max_size);
	if (serialized == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) serialized, 0, max_size);

	if (from == NULL)
		n_read = read(sockfd, serialized, max_size);
	else{
		socklen_t len = sizeof(*from);
		n_read = recvfrom(sockfd, serialized, max_size, 0,
				(struct sockaddr*)from, &len);
	}

	if (n_read < 0)
		return NULL;

	//Message* returned is allocated in deserialize_message()
	Message *mex = deserialize_message(serialized);

	free(serialized);
	return mex;
}



int send_read_cmd(int cmd_sock, int data_sock, int cmd, 
		char *data){

	struct timeval to;
	Message cmd_mex;
	Message *rec;
	int flag = 0;

	//making cmd packet to send 
	make_packet(&cmd_mex, data, 0, 0, cmd);

	fd_set read_set;
	FD_ZERO(&read_set);
	int maxfd;

	to.tv_sec = Tsec;
	to.tv_usec = Tnsec / 1000;
	int select_ret;
	
	maxfd = data_sock < cmd_sock ? (cmd_sock + 1) : (data_sock + 1);
	while (1){
		//sending packet
		send_packet(cmd_sock, &cmd_mex, NULL);
		
		//listening on both cmd_sock and data_sock
		FD_SET(cmd_sock, &read_set);
		FD_SET(data_sock, &read_set);

		select_ret = select(maxfd, &read_set, NULL, NULL, &to);

		if (select_ret <0){
			perror("error in select");
			exit(EXIT_FAILURE);
		}

		if (FD_ISSET(cmd_sock, &read_set)){
			//cmd sock ready. reading packet
			rec = receive_packet(cmd_sock, NULL);
			if (rec == NULL){
				perror("error receiving packet cmd");
				exit(EXIT_FAILURE);
			}
			//checking if it a fin. server crashed
			if ( (flag = rec -> flag) & FIN) {
				printf("connection close serverside\n");
				exit(EXIT_SUCCESS);
			}
			//when client send get request on unexisting file
			else if (flag & FILE_NOT_FOUND){
				printf("[Error]: file doesnt exist server-side\n");
				return -1;
			}
			//command message ack
			else if (flag & ACK){
				//checking on length. if length > 0, its a put ack and 
				//the name of file on server will be different
				if (rec -> length > 0 && flag & CHAR_INDICATOR)
					printf("The file will be saved as %s on server.\n", 
							rec -> data);
				break; //ending while
			}
		}
		//server is still sending data on data_sock: this means the cmd message
		//ack was lost. just exiting from while and continue like i've been
		//read the ack
		else if (FD_ISSET(data_sock, &read_set) )
			break; 
	}

	return 0;
}
