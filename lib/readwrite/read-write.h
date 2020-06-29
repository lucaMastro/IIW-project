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
			bytes_read = fread(mess_to_fill -> list_data, 1, MSS, (FILE*) read_data_from_here);
			mess_to_fill -> length = bytes_read;
		}
		else{
			int str_len = strlen((char*)read_data_from_here);
			min = str_len < MSS ? str_len : MSS;

			mess_to_fill -> length = min;
			memcpy(mess_to_fill -> list_data, (unsigned char*) read_data_from_here, mess_to_fill -> length);
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



ssize_t send_data(int data_sock, int cmd_sock, void *data, int type){
	ssize_t bytes_sent = 0;
	int len_ser = HEADER_SIZE;
	unsigned char *tmp, *new_data_pointer = (unsigned char*) data;
	int flag;
	int sending_sock;

	int packet_sent = 0;
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


	int sem = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if (sem < 0){
		perror("error semaphore");
		exit(EXIT_FAILURE);
	}

	if (semctl(sem, 0, SETVAL, RECEIVE_WINDOW) < 0){ 
		perror("error initializing sem");
		exit(EXIT_FAILURE);
	}

	queue -> semaphore = sem;
	queue -> cmd_sock = cmd_sock;
	queue -> data_sock = data_sock;

	pthread_create(&tid, NULL, waiting_for_ack, (void*) queue);

	do{
		while (semop(queue -> semaphore, &sops, 1)< 0) {
			if (errno != EINTR){
				perror("error trying get coin");
				exit(EXIT_FAILURE);
				
			}
		}
		
		//cerco la sock su cui inviare:
		Message *mex = (Message *)
			malloc(sizeof(Message));
		if (mex == NULL){
			perror("error malloc");
			exit(EXIT_FAILURE);
		}
		mex -> length = 0;
		mex -> flag = type;
		memset((void*) mex -> list_data, 0, MSS);


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

		sending_sock = (is_command_mex(mex) ) ? cmd_sock : data_sock;

		//store the message in the queue:
		queue -> on_fly_message_queue[mex -> seq_num % RECEIVE_WINDOW]	= mex;
		//incremento seq_num
		queue -> next_seq_num = (queue -> next_seq_num + 1) % MAX_SEQ_NUM;

		send_packet(sending_sock, mex);
		packet_sent ++;
		
		//save flag for exiting
		flag = mex -> flag;
		
	} //while(mex.length == MSS);
	while( (flag & END_OF_DATA) != END_OF_DATA ); //quando è == 1 ho inviato l'ultimo	

	if (pthread_join(tid, NULL) < 0){
		perror("error in join");
		exit(EXIT_FAILURE);
	}
	free(queue);
	return bytes_sent;
}


ssize_t receive_data(int data_sock, int cmd_sock, void *write_here, 
		int *save_here_flag){

	int max_size = HEADER_SIZE + MSS;
	ssize_t n_read;
	int str_len = 0;
	int old_str_len;
	uint16_t flag;
	uint8_t expected_seq_num = 1;
	
	int test_timer = 0;

	Message ack;
	make_packet(&ack, NULL, 0, 0, ACK);
	do {
		Message *mex;
		//leggo
		mex = receive_packet(data_sock, NULL);
		flag = mex -> flag;

		//salvo flag per il controllo a serverside
		if (save_here_flag != NULL )
			*save_here_flag = flag;
			
		//devo scrivere solo se è il seq_num atteso:
		//è possibile che arrivino pacchetti con solo header
		if (mex -> seq_num == expected_seq_num){
			
			if ( (mex -> seq_num == 2 || mex -> seq_num == 12) && test_timer == 0)
			//if (mex -> seq_num == 2 && test_timer == 0)
				test_timer++;
			else{
				if (mex -> length > 0){
					//write_data(mex, write_here, tmp, &str_len, &old_str_len);	
					write_data(mex, write_here, mex -> list_data, &str_len, &old_str_len);	
				}	

			//test retrasmission. simulate lost of a entire window
				ack.ack_num = expected_seq_num; 
				expected_seq_num = (expected_seq_num + 1) % MAX_SEQ_NUM;
				
			}
		}

		send_packet(cmd_sock, &ack);
	}
	while ((flag & END_OF_DATA) != END_OF_DATA);
		
	return n_read;

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
		memcpy( (*str_dest) + (*old_str_len), mex -> list_data, 
				mex -> length);

		*(*str_dest + *str_len) = '\0';
	}
	else{
		fwrite(src, 1, mex -> length, (FILE *) dest);
	}

	free(mex);
}

ssize_t send_unconnected(int fd, FILE *file_to_send, unsigned char *data_char, struct sockaddr_in *to, int type){
	ssize_t bytes_sent = 0;
	int len_ser = HEADER_SIZE;
	unsigned char *tmp;

	Message mex;
	
	do{
		memset((void*) mex.list_data, 0, MSS);
		if (data_char == NULL)
			make_packet(&mex, file_to_send, 0,0, type);
		else
			data_char = (unsigned char*) make_packet(&mex, data_char, 0, 0, (CHAR_INDICATOR | type));
		
		len_ser += mex.length;
		unsigned char *packet_ser = (unsigned char*) malloc(sizeof(unsigned char) * len_ser);
		if (packet_ser == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}
		tmp = serialize_header(&mex, packet_ser);
		memcpy(tmp, mex.list_data, mex.length);
		
		bytes_sent += writen(fd, len_ser, to, packet_ser);	

		free(packet_ser);
		len_ser = HEADER_SIZE;

	} 
	while( (mex.flag & END_OF_DATA) != END_OF_DATA ); //quando è == 1 ho inviato l'ultimo
	
	return bytes_sent;
}

size_t receive_unconnected(int fd, FILE *write_here, unsigned char **write_string_here, struct sockaddr_in *from, int *save_here_flag){
	int max_size = HEADER_SIZE + MSS;
	ssize_t n_read;
	int str_len = 0;
	int old_str_len;
	unsigned char *tmp;
	unsigned char *serialized = (unsigned char *) malloc(sizeof(char) * max_size);
	if (serialized == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) serialized, 0, max_size);
	
	Message mex;
	
	do {
		if (from != NULL){
			socklen_t len = sizeof(*from);
			n_read = recvfrom(fd, serialized, max_size, 0, (struct sockaddr*)from, &len);
		}
		else
			n_read = recvfrom(fd, serialized, max_size, 0, NULL, NULL);

		tmp = deserialize_header(&mex, serialized);

		if (save_here_flag != NULL)
			*save_here_flag = mex.flag;

		if (mex.length > 0){
			if ( (mex.flag & CHAR_INDICATOR) == CHAR_INDICATOR){
				old_str_len = str_len;
				str_len += mex.length;
				
				*write_string_here = (unsigned char*) realloc(*write_string_here, str_len);
				if (*write_string_here == NULL){
					perror("error in malloc");
					exit(EXIT_FAILURE);
				}

				memcpy(*write_string_here + old_str_len, tmp, mex.length);
			}
			else
				fwrite(tmp, 1, mex.length, write_here);
			
		}
	}
	//while(n_read == max_size);
	while ((mex.flag & END_OF_DATA) != END_OF_DATA);

	
	return n_read;

}


int is_command_mex(Message *mex){
	return (mex -> flag & (ACK | SYN | SYN_ACK | PUT | GET | LIST | FIN));
}


void send_packet(int sockfd, Message *mex){

	int bytes_sent;
	int max_size = HEADER_SIZE + mex -> length;	
	unsigned char* packet_ser = serialize_message(mex);

	bytes_sent += write(sockfd, packet_ser, max_size);

	free(packet_ser);
}

Message *receive_packet(int sockfd, struct timeval *time_out){

	ssize_t n_read;
	int max_size = HEADER_SIZE + MSS;
	unsigned char *tmp;
	unsigned char *serialized = (unsigned char *) malloc(sizeof(char) * max_size);
	if (serialized == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) serialized, 0, max_size);

	if (time_out != NULL){
		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
					(void*)time_out, sizeof(*time_out)) < 0){
			perror("error setting timeout in receive packet");
			exit(EXIT_FAILURE);
		}
	}

	if ( (n_read = read(sockfd, serialized, max_size)) < 0){
		if (errno != EWOULDBLOCK && errno != EAGAIN){
			perror("error in read");
			exit(EXIT_FAILURE);
			
		}
		else{
			free(serialized);
			return NULL;
		}
	}

	//alloco la struttura in deserialize
	Message *mex = deserialize_message(serialized);

	free(serialized);
	return mex;
}
