
#define CLIENT_FOLDER "client-files/"

void client_put_operation(int cmd_sock, int data_sock, char *file_to_send,
		char *server_name){
	char *complete_path;
	int len = strlen(CLIENT_FOLDER);
	
	len += strlen(file_to_send) ;

	complete_path = (char *) malloc(sizeof(char) * len);
	if (complete_path == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) complete_path, 0, len);
	sprintf(complete_path, "%s%s", CLIENT_FOLDER, file_to_send);


	//verifico se il file esiste effettivamente
	if(access(complete_path, F_OK) == -1){
		if (errno == ENOENT){
			printf("The file %s doesn't exist\n", complete_path);
			return;
		}
		else{
			perror("error in access");
			exit(EXIT_FAILURE);
		}	

	}

	send_read_cmd(cmd_sock, data_sock, PUT | CHAR_INDICATOR, server_name);

	FILE *segment_file_transfert;
	segment_file_transfert = fopen(complete_path, "rb");					

	send_data(data_sock, cmd_sock, segment_file_transfert, 0, NULL);

	free(complete_path);
}



void client_get_operation(int cmd_sock, int data_sock, char *file_to_get, 
		char *local_name){

	char *complete_path;
	int len = strlen(CLIENT_FOLDER);
	int ret_access;

	len += strlen(local_name);
	complete_path = (char *) malloc(sizeof(char) * len + 1);
	if (complete_path == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) complete_path, 0, len + 1);
	sprintf(complete_path, "%s%s", CLIENT_FOLDER, local_name);

	if (send_read_cmd(cmd_sock, data_sock, GET | CHAR_INDICATOR, 
				file_to_get) < 0) //file doesnt exist server-side
		return;


	// check for file in local
	if ((ret_access = access(complete_path, F_OK)) == -1 &&
		errno != ENOENT){
			perror("error in access");
			exit(EXIT_FAILURE);
	}	
	
	else if (ret_access == 0){ //the file still exist
		printf("file still exist\n");
		//freeding the path and creating a new path:
		// <folder>/<name>(1).<ext>
		free(complete_path);
		complete_path = change_name(local_name, CLIENT_FOLDER, NULL);
		printf("it will be saved as %s\n", complete_path);
	}

	//crea file
	FILE *new_file; 
	new_file = fopen(complete_path, "w+");
	if(new_file == NULL){
		printf("error in creation of '%s'\n", complete_path);		
		exit(1);		
	}	
	//ricevi messaggi	
	receive_data(data_sock, cmd_sock, new_file, NULL);

	fclose(new_file);
	free(complete_path);
}



void client_list_operation(int cmd_sock, int data_sock){

	send_read_cmd(cmd_sock, data_sock, LIST, NULL);
	unsigned char *list = NULL;
	receive_data(data_sock, cmd_sock, &list, NULL );
	//printing list content
	printf("%s\n", list);
	free(list);
}


void client_exit_operation(int cmd_sock, int data_sock){
	Message fin, *ack;
	make_packet(&fin, NULL, 0, 0, FIN);

	//reading ack:
	//waiting for a little
	struct timeval to;
	to.tv_sec = Tsec;
	to.tv_usec = Tnsec / 1000;

	for (int i = 0; i < 10; i++){ //after 10 times, it will close
		if (!is_packet_lost())
			send_packet(cmd_sock, &fin, NULL);
		setsockopt(cmd_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
		ack = receive_packet(cmd_sock, NULL);	
		if (ack == NULL){
			if (errno == EAGAIN)
				continue;
			else if (errno == ECONNREFUSED)
				break; //lost ack
			else{
				perror("error receiving ack");
				exit(EXIT_FAILURE);
			}
		}
	}
	close(cmd_sock);
	close(data_sock);
}
