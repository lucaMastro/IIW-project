
# define MAXSLEEP 128

void stampa_mess(Message *mex){
	printf("\nMessage:\n");
	printf("seq_num: %u\n", mex -> seq_num);
	printf("ack_num: %u\n", mex -> ack_num);
	printf("flag:    %u\n", mex -> flag);
	//printf("rec_win: %u\n", mex -> rec_win);
	printf("length:  %u\n", mex -> length);
	
	if ((mex -> flag & CHAR_INDICATOR) == CHAR_INDICATOR)
		printf("data:    %s\n", mex -> list_data);
}



int connect_retry(int sockfd, struct sockaddr_in *addr, socklen_t alen, 	
		Message *ack){
	int nsec;
	for (nsec = 1; nsec <= MAXSLEEP; nsec <<=1) {
	
		if (connect (sockfd, (struct sockaddr *)addr, alen) == 0){
			if (ack != NULL){
				send_packet(sockfd, ack, NULL);
			}
			return(0);	/*connessione accettata */
		}
	/* Ritardo prima di un nuovo tentativo di connessione */
		if (nsec <= MAXSLEEP/2)
			sleep(nsec);
	}
	return(-1);
}
