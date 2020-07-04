unsigned char *deserialize_seq_num(Message *mex, unsigned char *buffer){
	memcpy(&(mex -> seq_num), buffer,1);

	return buffer + 1;
}

unsigned char *deserialize_ack_num(Message *mex, unsigned char *buffer){
	memcpy(&(mex -> ack_num), buffer,  1);

	return buffer + 1;
}

unsigned char *deserialize_flag(Message *mex,unsigned char *buffer){
	memcpy(&(mex -> flag), buffer, 2);
	return buffer + 2;
}

unsigned char *deserialize_length(Message *mex,unsigned char *buffer){
	memcpy(&(mex -> length), buffer, 2);

	return buffer + 2;
}

unsigned char *deserialize_header(Message *mex,unsigned char *buffer){
	buffer = deserialize_seq_num(mex, buffer);
	buffer = deserialize_ack_num(mex, buffer);
	buffer = deserialize_flag(mex, buffer);
	buffer = deserialize_length(mex, buffer);

	return buffer;
}


void deserialize_data(Message *mex, unsigned char *buffer){

	memcpy(mex -> data, buffer, mex -> length);
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
	
}
