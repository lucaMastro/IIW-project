#include "../structs/message_struct.h"
#include <stdlib.h>
#include <string.h>

unsigned char *deserialize_seq_num(Message *mex, unsigned char *buffer){
	memcpy(&(mex -> seq_num), buffer,4);

	return buffer + 4;
}

unsigned char *deserialize_ack_num(Message *mex, unsigned char *buffer){
	memcpy(&(mex -> ack_num), buffer,  4);

	return buffer + 4;
}

unsigned char *deserialize_flag(Message *mex,unsigned char *buffer){
	memcpy(&(mex -> flag), buffer, 2);
	return buffer + 2;
}

/*
unsigned char *deserialize_new_port(Message *mex,unsigned char *buffer){
	memcpy(&(mex -> new_port), buffer, 2);

	return buffer + 2;
}*/

unsigned char *deserialize_rec_win(Message *mex,unsigned char *buffer){
	memcpy(&(mex -> rec_win), buffer, 2);

	return buffer + 2;
}

unsigned char *deserialize_length(Message *mex,unsigned char *buffer){
	memcpy(&(mex -> length), buffer, 4);

	return buffer + 4;
}

unsigned char *deserialize_header(Message *mex,unsigned char *buffer){
	buffer = deserialize_seq_num(mex, buffer);
	buffer = deserialize_ack_num(mex, buffer);
	buffer = deserialize_flag(mex, buffer);
//	buffer = deserialize_new_port(mex, buffer);
	buffer = deserialize_rec_win(mex, buffer);
	buffer = deserialize_length(mex, buffer);

	return buffer;
}

/*
Message *deserialize_data(Message *mex, unsigned char *buffer){
	
	//check for data-type:
//	if ( (mex -> flag & 128) == 128){//char indicator turned on
	mex -> list_data = buffer;
	if ( (mex -> flag & 128) == 128){//char indicator turned on
	
		mex -> list_data [mex -> length] = '\0';
	
	}
//	}
//	else{ //char indicator turned off
		/*unsigned i;
		size_t size = mex -> length;
		for (i = 0; i < size; i++){
			if (fputc(buffer[i], mex -> file_data) == EOF){
				perror("error in fputc");
				exit(EXIT_FAILURE);
			}		
		}
		
//		unsigned length = mex -> length;
//		mex -> list_data = (unsigned char*) malloc(sizeof(unsigned char) * length);
//		if (mex -> list_data == NULL){
//			perror("error in malloc");
//			exit(EXIT_FAILURE);
//		}

//		copy_data_field(length, mex -> list_data, buffer);

//	}
	return mex;
}

Message *deserialize_message(unsigned char *buffer){
	Message *mex = (Message*) malloc(sizeof(Message));
	if (mex == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) mex, 0, sizeof(Message));

	unsigned char *tmp = buffer;
	tmp = deserialize_header(mex, tmp);
	deserialize_data(mex, tmp);

	return mex;
	
}*/