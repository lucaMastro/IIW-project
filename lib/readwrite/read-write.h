#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include "../serialize/deserialize.h"
#include "../serialize/serialize.h"
#include "../passive-functions/passive-functions.h"
#include <sys/time.h>


void write_data(Message *mex, void *dest, unsigned char *src, int *str_len, int *old_str_len);




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


void *make_packet(Message *mess_to_fill, void *read_data_from_here, int flag_to_set){

	int bytes_read;

	mess_to_fill -> seq_num = 0;
	mess_to_fill -> ack_num = 0;
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
	/*		if (str_len < MSS)
				min = str_len;
			else
				min = MSS;*/

			mess_to_fill -> length = min;
			memcpy(mess_to_fill -> list_data, (unsigned char*) read_data_from_here, mess_to_fill -> length);
			//in questo caso ritorno la posizione della stringa da cui continuare a leggere
			return_addr = (unsigned char*)read_data_from_here + min;
		}
	}
	else
		mess_to_fill -> length = 0;

	if (mess_to_fill -> length < MSS)
		mess_to_fill -> flag += END_OF_DATA;

	return return_addr;
}



ssize_t send_data(int connected_fd, void *data, int type){
	ssize_t bytes_sent = 0;
	int len_ser = HEADER_SIZE;
	unsigned char *tmp, *new_data_pointer = (unsigned char*) data;
	Message mex;

	mex.length = 0;
	mex.flag = type;
	do{
		memset((void*) mex.list_data, 0, MSS);
		
		if (data != NULL){
			if ( (type & CHAR_INDICATOR) == CHAR_INDICATOR)
				new_data_pointer = (unsigned char *) make_packet(&mex, new_data_pointer, type);
			else
				make_packet(&mex, data, type);
		}

		if (mex.length == 0)
			mex.flag = mex.flag | END_OF_DATA;

		len_ser += mex.length;
		//printf("len ser = %d\n", len_ser);
		unsigned char *packet_ser = (unsigned char*) malloc(sizeof(unsigned char) * len_ser);
		if (packet_ser == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}
		tmp = serialize_header(&mex, packet_ser);
		memcpy(tmp, mex.list_data, mex.length);

		bytes_sent += write(connected_fd, packet_ser, len_ser);
		
		free(packet_ser);
		len_ser = HEADER_SIZE;

	} //while(mex.length == MSS);
	while( (mex.flag & END_OF_DATA) != END_OF_DATA ); //quando è == 1 ho inviato l'ultimo	

	return bytes_sent;
}


ssize_t receive_data(int connected_sockfd, void *write_here, 
		int *save_here_flag){

	int max_size = HEADER_SIZE + MSS;
	ssize_t n_read;
	int str_len = 0;
	int old_str_len;
	uint16_t flag;
	uint8_t expected_seq_num = 0;
	unsigned char *tmp;
	unsigned char *serialized = (unsigned char *) malloc(sizeof(char) * max_size);
	if (serialized == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) serialized, 0, max_size);
	
	do {
		//leggo
		if ( (n_read = read(connected_sockfd, serialized, max_size)) < 0){
			perror("error in read");
			exit(EXIT_FAILURE);
		}

		//alloco la struttura
		Message *mex = (Message *) malloc(sizeof(Message));
		if (mex == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}
		//fillo la struttura
		tmp = deserialize_header(mex, serialized);
		//stampa_mess(mex);

		//printf("\n\n temp %s\n\n\n", tmp);
		//salvo flag per il controllo del ciclo
		flag = mex -> flag;

		//salvo flag per il controllo a serverside
		if (save_here_flag != NULL )
			*save_here_flag = flag;

			
		//è possibile che arrivino pacchetti con solo header
		if (mex -> length > 0){
			
			//devo scrivere solo se è il seq_num atteso:
			if (mex -> seq_num == expected_seq_num){
				write_data(mex, write_here, tmp, &str_len, &old_str_len);
			}
			
		}
	}
	//esco quando ho ricevuto l'ultimo pacchetto e non ho niente bufferizzato

	while ((flag & END_OF_DATA) != END_OF_DATA);
	free(serialized);

		
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
		memcpy( (*str_dest) + (*old_str_len), src, mex -> length);
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
			make_packet(&mex, file_to_send, type);
		else
			data_char = (unsigned char*) make_packet(&mex, data_char, (CHAR_INDICATOR | type));
		
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

		free(packet_ser);
		len_ser = HEADER_SIZE;

	} //while(mex.length == MSS);
	while( (mex.flag & END_OF_DATA) != END_OF_DATA ); //quando è == 1 ho inviato l'ultimo
	
	printf("bytes sent = %lu\n", bytes_sent);
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

		//printf("received flag = %u\n", mex.flag);
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

				//strcpy((char*) write_string_here, (char *)tmp);	
				//printf("tmp = %s\n", tmp);
				memcpy(*write_string_here + old_str_len, tmp, mex.length);
			}
			else
				fwrite(tmp, 1, mex.length, write_here);
			
		}
	}
	//while(n_read == max_size);
	while ((mex.flag & END_OF_DATA) != END_OF_DATA);

	printf("exited\n");
	
	return n_read;

}
