
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



unsigned char *serialize_flag(Message *mex,unsigned char *buffer){
	memcpy(buffer, &(mex -> flag), 2);
	return buffer + 2;
}


unsigned char *serialize_length(Message *mex,unsigned char *buffer){
	memcpy(buffer, &(mex -> length), 2);

	return buffer + 2;
}

/*	Byte header	*/
unsigned char *serialize_header(Message *mex,unsigned char *buffer){
	unsigned char *tmp = buffer;
	tmp = serialize_seq_num(mex, tmp);
	tmp = serialize_ack_num(mex, tmp);
	tmp = serialize_flag(mex, tmp);
	tmp = serialize_length(mex, tmp);

	return tmp;
}


void serialize_data(Message *mex, unsigned char *buffer){
	memcpy(buffer, mex -> data, mex -> length);
}


unsigned char *serialize_message(Message *mex){
	int max_size = HEADER_SIZE + MSS;	
	unsigned char *tmp;
	unsigned char *serialized = (unsigned char *)
		malloc(sizeof(unsigned char) * max_size);
	if (serialized == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}

	tmp = serialize_header(mex, serialized);
	serialize_data(mex, tmp);	

	return serialized;
}
