
#include "../structs/message_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


unsigned char *serialize_seq_num(Message *mex, unsigned char *buffer){
	memcpy(buffer, &(mex -> seq_num), 1);

	return buffer + 1;
}

unsigned char *serialize_ack_num(Message *mex, unsigned char *buffer){
	memcpy(buffer, &(mex -> ack_num), 1);

	return buffer + 1;
}

/*
unsigned char *serialize_ack_num(Message *mex, unsigned char *buffer){
	memcpy(buffer, &(mex -> seq_ack_num), 1);

	return buffer + 1;
}*/


unsigned char *serialize_flag(Message *mex,unsigned char *buffer){
	memcpy(buffer, &(mex -> flag), 2);
	return buffer + 2;
}


/*unsigned char *serialize_rec_win(Message *mex,unsigned char *buffer){
	memcpy(buffer, &(mex -> rec_win), 2);

	return buffer + 2;
}*/

unsigned char *serialize_length(Message *mex,unsigned char *buffer){
	memcpy(buffer, &(mex -> length), 2);

	return buffer + 2;
}

/*	Byte header	*/
unsigned char *serialize_header(Message *mex,unsigned char *buffer){
	unsigned char *tmp = buffer;
	tmp = serialize_seq_num(mex, tmp);
	tmp = serialize_ack_num(mex, tmp);
	//tmp = serialize_seq_ack(mex, tmp);
	tmp = serialize_flag(mex, tmp);
	tmp = serialize_length(mex, tmp);

	return tmp;
}

/*
unsigned char *serialize_data(Message *mex, unsigned char *buffer){
	//serialize mex -> file_data
	
	if ((mex -> flag & 128) == 128){ //char indicator turned on
		copy_data_field(mex -> length, buffer, mex -> list_data);
		//mex -> list_data = buffer;
	}
	else{
		fread(buffer, sizeof(char), mex -> length, mex -> file_data);
	}


	return buffer + mex -> length;
}*/

/*
unsigned char *serialize_message(Message *mex){
	unsigned buf_size = HEADER_SIZE;
	unsigned data_size = mex -> length;

	if (data_size != 0)
		buf_size += data_size;
		
		creo il buffer che verrà trasmesso. conterrà header e eventuali dati 
	 *	serializzati	
	unsigned char *serialized = (unsigned char*) malloc(sizeof(unsigned char) * buf_size);

	if (serialized == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
//	memset((void*) serialized, 0, buf_size);

		serializzo header ed eventuali dati nello stesso buffer	
	unsigned char *temp = serialized;  indica la posizione dove inserire dati
										all'interno del buffer	
	
	temp = serialize_header(mex, temp);
	//printf("head diff %u\n", serialized - temp);

	if (data_size != 0){
		temp = serialize_data(mex, temp);
		//printf("diff %d\n\n", temp-tmp);
	}

	return serialized;
}*/
