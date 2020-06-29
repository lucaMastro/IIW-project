
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


char *change_name(char *name_file, char *folder_path){
	//name_file is <name>.<ext>. 
	//name_file still exists. i will make:
	//<name>(i).<ext>
	//where i is the lowest integer possible
	
	char *new_file_name, **multiple_dots, *tmp;
	char str_index[3]; //max i = 99
	int i = 1, j, tokens_num = 0, ret_access;
	int len = strlen(folder_path) + strlen(name_file) + 3 + 1;
	//+3 = (i)
	//+1 = '\0'
	
	multiple_dots = (char **) malloc(sizeof(char*));
	if (multiple_dots == NULL){
		perror("error malloc multiple dots");
		exit(EXIT_FAILURE);
	}

	multiple_dots[0] = strtok(name_file, ".");
	tokens_num++;
	while( (tmp = strtok(NULL, ".")) != NULL ){
		tokens_num++;
		multiple_dots = (char **)
			realloc(multiple_dots, sizeof(char*) * tokens_num);

		if (multiple_dots == NULL){
			perror("error in realloc multiple_dots");
			exit(EXIT_FAILURE);
		}
		multiple_dots[tokens_num - 1] = tmp;
	}

	new_file_name = (char *) malloc(sizeof(char) * len);
	if (new_file_name == NULL){
		perror("error in malloc new_file_name");
		exit(EXIT_FAILURE);
	}

	do{
		memset((void*) str_index, 0, 3);
		sprintf(str_index, "%d", i);

		memset((void*) new_file_name, 0, len);
		sprintf(new_file_name, "%s%s", folder_path, multiple_dots[0]);
		for (j = 1; j < tokens_num - 1; j++){
			strcat(new_file_name, ".");
			strcat(new_file_name, multiple_dots[j]);
		}
		strcat(new_file_name, "(");
		
		strcat(new_file_name, str_index);

		strcat(new_file_name, ").");
		strcat(new_file_name, multiple_dots[j]);		


		//check if still exist:
		ret_access = access(new_file_name, F_OK);

		if (ret_access == -1 && errno != ENOENT){
				perror("error in access");
				exit(EXIT_FAILURE);
		}		
		i++;
	} while (ret_access == 0); //if 0, still exist

	return new_file_name;

}
