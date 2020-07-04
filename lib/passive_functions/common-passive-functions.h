
# define MAXSLEEP 128

void stampa_mess(Message *mex){
	printf("\nMessage:\n");
	printf("seq_num: %u\n", mex -> seq_num);
	printf("ack_num: %u\n", mex -> ack_num);
	printf("flag:    %u\n", mex -> flag);
	printf("length:  %u\n", mex -> length);
	
	if ((mex -> flag & CHAR_INDICATOR) == CHAR_INDICATOR)
		printf("data:    %s\n", mex -> data);
	printf("\n");
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


char *change_name(char *const_name_file, char *folder_path, char **new_name){
	//name_file is <name>.<ext>. 
	//name_file still exists. i will make:
	//<name>(i).<ext>
	//where i is the lowest integer possible
	
	//dont want change the const_name_file, thats why it will be copied
	//in a local variable
	char *name_file = (char*) malloc(strlen(const_name_file + 1));
	if (name_file == NULL){
		perror("error malloc name_file");
		exit(EXIT_FAILURE);
	}
	strcpy(name_file, const_name_file);
	
	char *new_file_name, **multiple_dots, *tmp;

	char str_index[3]; //max i = 99. it will still exist a(1).x, then 
					   //new name should me a(2).x. this buffer is only
					   //for number: sprintf(str_index, "%d", i)
					   //i is the index that will appear in the brackets

	int i = 1, j, tokens_num = 0, ret_access;
	int len = strlen(folder_path) + strlen(name_file) + 3 + 1;
	//+3 = (i)
	//+1 = '\0'
	
	multiple_dots = (char **) malloc(sizeof(char*));
	if (multiple_dots == NULL){
		perror("error malloc multiple dots");
		exit(EXIT_FAILURE);
	}

	//a file named a.x.y.z will be renamed in 
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

	//a.x.y.z -> a.x.y(1).z
	do{
		memset((void*) str_index, 0, 3);
		sprintf(str_index, "%d", i); //starting i == 1

		//making a.x.y
		memset((void*) new_file_name, 0, len);
		sprintf(new_file_name, "%s%s", folder_path, multiple_dots[0]);
		for (j = 1; j < tokens_num - 1; j++){
			strcat(new_file_name, ".");
			strcat(new_file_name, multiple_dots[j]);
		} //made a.x.y

		//making a.x.y(i).
		strcat(new_file_name, "(");
		strcat(new_file_name, str_index);
		strcat(new_file_name, ").");
		//made a.x.y(i).

		//appending last ext
		strcat(new_file_name, multiple_dots[j]);		

		//check if still exist:
		ret_access = access(new_file_name, F_OK);

		if (ret_access == -1 && errno != ENOENT){ 
				perror("error in access");
				exit(EXIT_FAILURE);
		}		
		i++;
	} while (ret_access == 0); //if 0, still exist
							   //if -1 and err is ENOENT, it will exit

	//freeding old local name
	free(name_file);

	//new_file_name is now <client_folder>a.x.y(i).z
	if (new_name != NULL){ //i changed name
		//storing the name file
		char *tmp = new_file_name + strlen(folder_path); 
		sprintf(*new_name, "%s", tmp); //storing only a.x.y(i).z
	}

	//returning complete path
	return new_file_name;

}
