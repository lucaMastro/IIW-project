#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include "../serialize/deserialize.h"
#include "../serialize/serialize.h"
#include "../functions/packet_queue_manager.h"
#include <sys/time.h>


void write_on_string(Message *mex, unsigned char **dest, int *old_str_len,
		int *str_len, unsigned char *src, Queue_manager *receive_queue, uint16_t *flag);
int is_command_message(Message *cmd);
void write_on_file(unsigned char *src, Message *mex, FILE *opened_stream, Queue_manager *receive_queue, uint16_t *flag);

void stampa_mess(Message *mex){
	printf("\nMessage:\n");
	printf("seq_num: %u\n", mex -> seq_num);
	printf("ack_num: %u\n", mex -> ack_num);
	printf("flag:    %u\n", mex -> flag);
	printf("rec_win: %u\n", mex -> rec_win);
	printf("length:  %u\n", mex -> length);
	
/*	if ((mex -> flag & CHAR_INDICATOR) == CHAR_INDICATOR)
		printf("data:    %s\n", mex -> list_data);*/
}


/*	readn legge esattamente n byte	*/
ssize_t readn(int fd, size_t n, struct sockaddr_in *from, unsigned char *buff){
	size_t nleft = n;
	size_t nread;
	unsigned char *ptr;

	ptr = buff;
	while(nleft > 0){
		if (from != NULL) {
			socklen_t len = sizeof(*from);
			nread = recvfrom(fd, ptr, n, 0,(struct sockaddr*) from, &len);
		}
		else
			nread = recvfrom(fd, ptr, n, 0, NULL, NULL);

		if ( nread < 0 ){
			if (errno == EINTR)
				nread = 0;
			else
				return -1;
		}
		else if (nread == 0)
			break;

		nleft -= nread;
		ptr += nread;
	}
	return nleft;

}


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



void *make_packet(Message *mess, FILE *file, unsigned char *data_char, int type, int *seq_num){

	int bytes_read;

	mess -> seq_num = *seq_num;
	mess -> ack_num = 0;
	mess -> flag = type;
	//mess -> new_port = 0;
	mess -> rec_win = 0;
	void* return_addr = NULL;
	int min;

	if (file != NULL || data_char != NULL){
		if ( !(CHAR_INDICATOR & type) ){
			bytes_read = fread(mess -> list_data, 1, MSS, file);
			mess -> length = bytes_read;
		}
		else{
			if ( strlen((char*)data_char) < MSS)
				min = strlen((char*)data_char);
			else
				min = MSS;

			mess -> length = min;
			//strcpy((char*) mess -> list_data, (char*) data_char);
			memcpy(mess -> list_data, data_char, mess -> length);
			return_addr = data_char + min;
			//printf("imess -> list_data %s\n", mess -> list_data);
		}
	}
	else
		mess -> length = 0;

	if (mess -> length < MSS)
		mess -> flag += END_OF_DATA;

	return return_addr;
}


ssize_t send_data(int fd, FILE *file_to_send, unsigned char *data_char, struct sockaddr_in *to, int type){
	ssize_t bytes_sent = 0;
	int len_ser = HEADER_SIZE;
	unsigned char *tmp;
	int seq_num = 1;
	Message mex;
	
	Message mex_ritardato;

	mex.flag = type;
	do{
		memset((void*) mex.list_data, 0, MSS);

		
		if (data_char == NULL)
			make_packet(&mex, file_to_send, NULL, type, &seq_num);
		else
			data_char = (unsigned char*) make_packet(&mex, NULL, data_char, (CHAR_INDICATOR | type), &seq_num);
		
		if (is_command_message(&mex)){
			seq_num = 0;
		}

		mex.seq_num = seq_num;
		
		if (mex.seq_num == 1){
			mex_ritardato = mex;
			seq_num += mex.length;
			continue;
		}


		len_ser += mex.length;
		unsigned char *packet_ser = (unsigned char*) malloc(sizeof(unsigned char) * len_ser);
		if (packet_ser == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}
		tmp = serialize_header(&mex, packet_ser);
		memcpy(tmp, mex.list_data, mex.length);
		
		/*if (data_char != NULL){
			printf("datachar = %s\ntmp[data] = %s\n", data_char, tmp);
		}*/

		bytes_sent += writen(fd, len_ser, to, packet_ser);	
		//printf("bytes sent = %lu\n", bytes_sent);
		//printf("flag = %u\n\n", mex.flag);
		
		seq_num += mex.length;

		free(packet_ser);
		len_ser = HEADER_SIZE;

	} //while(mex.length == MSS);
	while( (mex.flag & END_OF_DATA) != END_OF_DATA ); //quando è == 1 ho inviato l'ultimo
	

	if (file_to_send != NULL){
		printf("PACCHETTO RITARDATO\n");

		len_ser += mex_ritardato.length;
		unsigned char *packet_ser = (unsigned char*) malloc(sizeof(unsigned char) * len_ser);
		if (packet_ser == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}
		tmp = serialize_header(&mex_ritardato, packet_ser);
		memcpy(tmp, mex_ritardato.list_data, mex_ritardato.length);
		
		/*if (data_char != NULL){
			printf("datachar = %s\ntmp[data] = %s\n", data_char, tmp);
		}*/

		bytes_sent += writen(fd, len_ser, to, packet_ser);	
		//printf("bytes sent = %lu\n", bytes_sent);
		//printf("flag = %u\n\n", mex.flag);
		
		seq_num += mex.length;

		free(packet_ser);
	}

	return bytes_sent;
}



ssize_t receive_data(int fd, FILE *write_here, unsigned char **write_string_here, struct sockaddr_in *from, int *save_here_flag){

	int max_size = HEADER_SIZE + MSS;
	ssize_t n_read;
	int str_len = 0;
	int old_str_len;
	uint16_t flag = 0;
	unsigned char *tmp;
	unsigned char *serialized = (unsigned char *) malloc(sizeof(char) * max_size);
	if (serialized == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) serialized, 0, max_size);


	Queue_manager receive_queue;
	receive_queue.cur_starting_pos = 0;
	receive_queue.queue = create_queue();
	receive_queue.free_bytes = MSS * QUEUE_SIZE;
	receive_queue.expected_seq_num = 1; 	
	
	do {
		//leggo
		if (from != NULL){
			socklen_t len = sizeof(*from);
			n_read = recvfrom(fd, serialized, max_size, 0, (struct sockaddr*)from, &len);
		}
		else
			n_read = recvfrom(fd, serialized, max_size, 0, NULL, NULL);

		//alloco la struttura
		Message *mex = (Message *) malloc(sizeof(Message));
		if (mex == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}
		//fillo la struttura
		tmp = deserialize_header(mex, serialized);
			
		//salvo il flag nella variabile per check di uscita
		//flag = mex -> flag; viene aggiornato quando scrivo dalla funzione
		//quando bufferizzo non devo mai uscire: devo aspettare un pacchetto mancante
		//quindi non ha senso aggiornarlo quando bufferizzo

		//salvo flag per il controllo a serverside
		if (save_here_flag != NULL )
			*save_here_flag = mex -> flag;

		//gestione messaggi di comando
		//if (is_command_message(mex)){
		if (mex -> seq_num == 0){
	
			//cmd mex hanno solo campi di tipo string, se ce l'hanno
			if (mex -> length > 0){
				write_on_string(mex, write_string_here, &old_str_len,
						 &str_len, tmp, &receive_queue, &flag);
			}
			//i nomi dei file sono limitati a un mss
			return n_read;
		}

			
		//è possibile che arrivino pacchetti con solo header
		if (mex -> length > 0){
			
			//devo scrivere solo se è il seq_num atteso:
			if (mex -> seq_num == receive_queue.expected_seq_num){

				//printf("lo scrivo. ftell = %ld\n", ftell(write_here));

				if ( (mex -> flag & CHAR_INDICATOR) == CHAR_INDICATOR){
					//writestring			
					write_on_string(mex, write_string_here, &old_str_len,
						 &str_len, tmp, &receive_queue, &flag);
					

					//scorro sulla lista e scrivo i dati
					//mex è stato freedato
					while ((mex = receive_queue.queue[receive_queue.cur_starting_pos]) 
							!= NULL){
							
						write_on_string(mex, write_string_here, &old_str_len,
							 &str_len, mex -> list_data, &receive_queue, &flag);
					}
				}
				else{


					write_on_file(tmp, mex, write_here, &receive_queue, &flag);
					//scorro sulla lista e scrivo i dati
					int pos = receive_queue.cur_starting_pos;
					while ( (mex = receive_queue.queue[pos]) != NULL){
						write_on_file(mex -> list_data, mex, write_here, &receive_queue, &flag);
						pos = (pos + 1) % QUEUE_SIZE;
						receive_queue.cur_starting_pos = pos;

					}
				}						

			}
			
			//pacchetto fuori ordine: va bufferizzato
			else{


				int offset = (mex -> seq_num - 1)/MSS;
				offset--;
				offset = offset % QUEUE_SIZE;
						
				//devo copiare tmp nel campo data del pacchetto:
				/*mex -> list_data = (unsigned char*) malloc(sizeof(unsigned char) * mex -> length);
				if (mex -> list_data == NULL){
					perror("error in malloc");
					exit(EXIT_FAILURE);
				}*/
				memcpy(mex -> list_data, tmp, mex -> length);

				receive_queue.queue[offset] = mex;
				receive_queue.free_bytes -= mex -> length;
			}	
		}
	}
	//esco quando ho ricevuto l'ultimo pacchetto e non ho niente bufferizzato

	while ((flag & END_OF_DATA) != END_OF_DATA || 
			receive_queue.queue[receive_queue.cur_starting_pos] != NULL); 

	free(receive_queue.queue);
		
	return n_read;

}


int is_command_message(Message *cmd){
	return (PUT | GET | LIST | FIN | SYN | SYN_ACK | ACK) & (cmd -> flag);
}



void write_on_string(Message *mex, unsigned char **dest, int *old_str_len,
		int *str_len, unsigned char *src, Queue_manager *receive_queue, uint16_t *flag){

	*old_str_len = *str_len;
	*str_len += mex -> length;

	*dest = (unsigned char*) realloc(*dest, *str_len);
	if (*dest == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);	
	}

	memcpy(*dest + *old_str_len, src, mex -> length);
					
	//aggiorno seq_num atteso
	receive_queue -> expected_seq_num = mex -> seq_num + mex -> length;
	*flag = mex -> flag;

	free(mex);

}

void write_on_file(unsigned char *src, Message *mex, FILE *opened_stream, Queue_manager *receive_queue, uint16_t *flag){
	fwrite(src, 1, mex -> length, opened_stream);

	//aggiorno seq_num atteso
	receive_queue -> expected_seq_num = mex -> seq_num + mex -> length;

	*flag = mex -> flag;

	free(mex);
}
