#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>


void write_data(Message *mex, void *dest, unsigned char *src, int *str_len, int *old_str_len);
void send_ack(int cmd_sock, int ack_num);
void check_last_ack(int cmd_sock, int data_sock, Message *ack);




/*	scrive esattamente n byte	*/
ssize_t writen(int fd, ssize_t n, struct sockaddr_in *to, unsigned char *buff){

	size_t nleft;
	size_t nwritten;
	unsigned char *ptr;

	ptr = buff;
	nleft = n;

	/*	writing header	*/
	while (nleft > 0){
		if ( (nwritten = sendto(fd, ptr, nleft, 0, (struct sockaddr *)to,
						sizeof(*to))) <= 0 ){
			if (nwritten < 0 && errno != EINTR){
				nwritten = 0;
			}
			else
				return -1;
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	//return nleft;
	return nwritten;
}	


void *make_packet(Message *mess_to_fill, void *read_data_from_here, int seq_num, int ack_num, int flag_to_set){

	int bytes_read;

	mess_to_fill -> seq_num = seq_num;
	mess_to_fill -> ack_num = ack_num;
	mess_to_fill -> flag = flag_to_set;
	void* return_addr = NULL;
	int min;

	if (read_data_from_here != NULL){
		if ( !(CHAR_INDICATOR & flag_to_set) ){
			bytes_read = fread(mess_to_fill -> data, 1, MSS, (FILE*) read_data_from_here);
			mess_to_fill -> length = bytes_read;
		}
		else{
			int str_len = strlen((char*)read_data_from_here);
			min = str_len < MSS ? str_len : MSS;

			mess_to_fill -> length = min;
			memcpy(mess_to_fill -> data, (unsigned char*) read_data_from_here, mess_to_fill -> length);
			//in questo caso ritorno la posizione della stringa da cui continuare a leggere
			return_addr = (unsigned char*)read_data_from_here + min;
		}
	}
	else
		mess_to_fill -> length = 0;

	if (mess_to_fill -> length < MSS){
		mess_to_fill -> flag += END_OF_DATA;
	}

	return return_addr;
}


int is_packet_lost(){
	int n =  rand() % 101 ; 
	//printf("random: %d\n", n);
	return ( n < p); 
	//n = rand() %101 => n in [0, 100]
	//n < p with prob p.
}


ssize_t send_data(int data_sock, int cmd_sock, void *data, int type,
		char *read_from_ack){
	ssize_t bytes_sent = 0;
	int len_ser = HEADER_SIZE;
	unsigned char *new_data_pointer = (unsigned char*) data;
	int flag = 0;
	int sending_sock;
	int is_command = 0;

//	int packet_sent = 0;
	pthread_t tid;
	Sending_queue *queue = (Sending_queue*) 
		malloc(sizeof(Sending_queue));
	if (queue == NULL){
		perror("error in malloc of sendingqueue");
		exit(EXIT_FAILURE);
	}

	initialize_struct(queue);
	struct sembuf sops;
	sops.sem_flg = 0;
	sops.sem_num = 0;
	sops.sem_op = -1;

	srand(time(0));

	int sem = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if (sem < 0){
		perror("error semaphore");
		exit(EXIT_FAILURE);
	}

	if (semctl(sem, 0, SETVAL, SENDING_WINDOW) < 0){ 
		perror("error initializing sem");
		exit(EXIT_FAILURE);
	}

	queue -> semaphore = sem;
	queue -> cmd_sock = cmd_sock;
	queue -> data_sock = data_sock;

	pthread_create(&tid, NULL, waiting_for_ack, (void*) queue);
	//printf("pthread created\n");
	//fflush(stdout);

	do{
		while (semop(queue -> semaphore, &sops, 1)< 0) {
			if (errno != EINTR){
				perror("error trying get coin");
				exit(EXIT_FAILURE);
				
			}
		}
		queue -> num_on_fly_pack ++;
		
		//cerco la sock su cui inviare:
		Message *mex = (Message *)
			malloc(sizeof(Message));
		if (mex == NULL){
			perror("error malloc");
			exit(EXIT_FAILURE);
		}
		mex -> length = 0;
		mex -> flag = type;
		memset((void*) mex -> data, 0, MSS);

		if ( (is_command = is_command_mex(mex)) ){
			sending_sock = cmd_sock;
			queue -> send_base = 0;
			queue -> should_read_data = 1;
			queue -> next_seq_num = 0;
		}
		else
			sending_sock = data_sock;

		if ( (type & CHAR_INDICATOR) == CHAR_INDICATOR)
			new_data_pointer = (unsigned char *) 
				make_packet(mex, new_data_pointer, 
				queue -> next_seq_num, //seq_num
				0, 					//ack_num
				type);
		else
			make_packet(mex, data, 
				queue -> next_seq_num, 
				0, 
				type);

		//sending_sock = (is_command_mex(mex) ) ? cmd_sock : data_sock;

		//store the message in the queue:
//		stampa_mess(mex);
//		printf("\n");
		//queue -> on_fly_message_queue[mex -> seq_num % SENDING_WINDOW]	= mex;
		queue -> on_fly_message_queue[mex -> seq_num] =	mex;
		//incremento seq_num
		queue -> next_seq_num = (queue -> next_seq_num + 1) % 
			(MAX_SEQ_NUM + 1);
//		printf("nsn %u\n", queue ->next_seq_num);

		if ( !is_packet_lost() )
			send_packet(sending_sock, mex, NULL);
		
	//	stampa_mess(mex);
	//	printf("packet %u\n\n", mex -> seq_num);

		//save flag for exiting
		flag = mex -> flag;
		
	} 
	while( (flag & END_OF_DATA) != END_OF_DATA ); //quando è == 1 ho inviato l'ultimo	

	if (pthread_join(tid, NULL) < 0){
		perror("error in join");
		exit(EXIT_FAILURE);
	}

	if (is_command)
		sprintf(read_from_ack, "%s", queue -> buf);

	free(queue);
	return bytes_sent;
}


void receive_data(int data_sock, int cmd_sock, void *write_here, 
		int *save_here_flag){

	int max_size = HEADER_SIZE + MSS;
	int str_len = 0;
	int old_str_len;
	uint16_t flag = 0;
	uint8_t expected_seq_num = 1;
	
	Message ack;
	make_packet(&ack, NULL, 0, 0, ACK);
	do {
		//printf("esn %u\n", expected_seq_num);
		Message *mex;
		//leggo
		mex = receive_packet(data_sock, NULL);
		if (mex == NULL){
			perror("error receiving packet in receive_data");
			exit(EXIT_FAILURE);
		}

	//	stampa_mess(mex);
	//	printf("\n");
		//salvo flag per il controllo a serverside
		if (save_here_flag != NULL )
			*save_here_flag = flag;
			
		//devo scrivere solo se è il seq_num atteso:
		//è possibile che arrivino pacchetti con solo header
		if (mex -> seq_num == expected_seq_num){
			flag = mex -> flag;
			if (mex -> length > 0)
				write_data(mex, write_here, mex -> data, 
						&str_len, &old_str_len);	
			
			ack.ack_num = expected_seq_num; 
			expected_seq_num = (expected_seq_num + 1) % (MAX_SEQ_NUM + 1);
		}

		if ( !is_packet_lost() )
			send_packet(cmd_sock, &ack, NULL);
		//printf("ack num: %u\n\n", ack.ack_num);
		
	}
	while ((flag & END_OF_DATA) != END_OF_DATA);


	//check if last ack is lost:
	check_last_ack(cmd_sock, data_sock, &ack);
	
/*	struct timeval to;
	to.tv_sec = Tsec <<2;
	to.tv_usec = (Tnsec / 1000)<<2; //nano secs to micro secs
	while(1){
		setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
		Message *m = receive_packet(data_sock, NULL);
		if (m == NULL){ //read(..) -> -1 
			//printf("\n\nRETURNED NULL\n");
		if (errno != EAGAIN){
				perror("error in last receive_pack of receive_data");
				exit(EXIT_FAILURE);
			}	
			else{
//				printf("ELSE: TUTTO OK\n\n");
				break;
			}
		}
		if (!is_packet_lost())
			send_packet(cmd_sock, &ack, NULL);
		//printf("ack num in timerized while: %u\n\n", ack.ack_num);
		free(m);
	}
	//deleting timeout
	to.tv_sec = 0;
	to.tv_usec = 0; 
	setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
*/		
//	return n_read;

}

void check_last_ack(int cmd_sock, int data_sock, Message *ack){
	struct timeval to;
	to.tv_sec = Tsec <<2;
	to.tv_usec = (Tnsec / 1000)<<2; //nano secs to micro secs
	//to.tv_sec = Tsec;
	//to.tv_usec = Tnsec / 1000; //nano secs to micro secs
	Message *m;
	while(1){
		setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
		m = receive_packet(data_sock, NULL);
		if (m == NULL){ //read(..) -> -1 
			//printf("\n\nRETURNED NULL\n");
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
		//printf("ack num in timerized while: %u\n\n", ack.ack_num);
		free(m);
	}
	free(m);
	//deleting timeout
	to.tv_sec = 0;
	to.tv_usec = 0; 
	setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));

}


void write_data(Message *mex, void *dest, unsigned char *src, int *str_len, int *old_str_len){
	//check sul flag
	if ( (mex -> flag & CHAR_INDICATOR) != 0){

		*old_str_len = *str_len;
		*str_len += mex -> length;

		unsigned char **str_dest = (unsigned char **) dest;  //necessario perché un messaggio stringa potrebbe essere diviso in 2 pacchetti
		*str_dest = (unsigned char*) realloc(*str_dest, (*str_len + 1));
		if (*str_dest == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);	
		}
		//memcpy( (*str_dest) + (*old_str_len), src, mex -> length);
		memcpy( (*str_dest) + (*old_str_len), mex -> data, 
				mex -> length);

		*(*str_dest + *str_len) = '\0';
	}
	else{
		fwrite(src, 1, mex -> length, (FILE *) dest);
	}

	free(mex);
}

int is_command_mex(Message *mex){
	return (mex -> flag & (ACK | SYN | SYN_ACK | PUT | GET | LIST | FIN));
}


void send_packet(int sockfd, Message *mex, struct sockaddr_in *to){

	int bytes_sent;
	int max_size = HEADER_SIZE + mex -> length;	
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

	ssize_t n_read;
	int max_size = HEADER_SIZE + MSS;
	unsigned char *tmp;
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

	if (n_read < 0){
		return NULL;
		/*perror("error in read");
		exit(EXIT_FAILURE);*/
	}

	//alloco la struttura in deserialize
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

	make_packet(&cmd_mex, data, 0, 0, cmd);

	//in ascolto su entrambe le socket
	
	fd_set read_set;
	FD_ZERO(&read_set);
	int maxfd;

	to.tv_sec = Tsec;
	to.tv_usec = Tnsec / 1000;
//	to.tv_sec = 5;
//	to.tv_usec = 0;
	int select_ret;
	
	maxfd = data_sock < cmd_sock ? (cmd_sock + 1) : (data_sock + 1);
	while (1){
		send_packet(cmd_sock, &cmd_mex, NULL);
		
		FD_SET(cmd_sock, &read_set);
		FD_SET(data_sock, &read_set);

		//l'ultimo parametro è struct timeval, per un timer
		//if (select(maxfd, &read_set, NULL, NULL, &to) < 0){
		select_ret = select(maxfd, &read_set, NULL, NULL, &to);

		if (select_ret <0){
			perror("error in select");
			exit(EXIT_FAILURE);
		}

		if (FD_ISSET(cmd_sock, &read_set)){
			//check se flag del messaggio ricevuto è un FIN
			rec = receive_packet(cmd_sock, NULL);
			if (rec == NULL){
				perror("error receiving packet cmd");
				exit(EXIT_FAILURE);
			}
			if ( (flag = rec -> flag) & FIN) {
				printf("connection close serverside\n");
				exit(EXIT_SUCCESS);
			}
			else if (flag & FILE_NOT_FOUND){
				printf("[Error]: file doesnt exist server-side\n");
				return -1;
			}
			else if (flag & ACK){
				if (rec -> length > 0 && flag & CHAR_INDICATOR)
					printf("The file will be saved as %s on server.\n", 
							rec -> data);
				break; //esco dal while
			}
		}
		else if (FD_ISSET(data_sock, &read_set) )
			break; //ho perso ack del comando, mi invia dati
	}

	return 0;
}
