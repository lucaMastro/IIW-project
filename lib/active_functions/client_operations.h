//#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <errno.h>
//#include <string.h>

//#include "../readwrite/read-write.h"

#define CLIENT_FOLDER "src/client/client-files/"

extern int h_errno;

void client_put_operation(int sockfd){
	char *file_to_send, *complete_path;
	int res_scan;
	int len = strlen(CLIENT_FOLDER);
	
	printf("What file do you want to upload ?\n");
	do{
		if ( (res_scan = scanf("%ms", &file_to_send)) < 0 && errno != EINTR){
			perror("error in scanf");
			exit(EXIT_FAILURE);
		}
		if (res_scan == 0)
			printf("input not valid\n");
	}
	while(res_scan  == 0);

	len += strlen(file_to_send) ;

	complete_path = (char *) malloc(sizeof(char) * len);
	if (complete_path == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) complete_path, 0, len);
	strcat(complete_path, CLIENT_FOLDER);
	strcat(complete_path, file_to_send);

	printf("looking for %s\n", complete_path);

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

	//check se lunghezza > MSS in caso impossibile inviare


	//invio comando: invio solo il nome del file, non l'intero apth
	if (send_data(sockfd, file_to_send, (PUT | CHAR_INDICATOR)) < 0){
		perror("errore in write");
		exit(1);
	}

	FILE *segment_file_transfert;
	segment_file_transfert = fopen(complete_path, "rb"); // la b sta per binario, sennò la fread non funziona
					
	if (send_data(sockfd, segment_file_transfert, 0) < 0){
		perror("errore in sendto");
		exit(1);
	}
	free(file_to_send);
	free(complete_path);
}



void client_get_operation(int sockfd){

	char *file_to_get;
	char *complete_path;
	int len = strlen(CLIENT_FOLDER);
	int res_scan;
	int ret_access;

	printf("Which file you want to download ?\n");
	do{
		if ( (res_scan = scanf("%ms", &file_to_get)) < 0 && errno != EINTR){
			perror("error in scanf");
			exit(EXIT_FAILURE);
		}
		if (res_scan == 0)
			printf("input not valid\n");
	}
	while(res_scan  == 0);


	len += strlen(file_to_get);
	complete_path = (char *) malloc(sizeof(char) * len);
	if (complete_path == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) complete_path, 0, len);
	strcat(complete_path, CLIENT_FOLDER);
	strcat(complete_path, file_to_get);

	//controllo se non è già presente nel client
	if ((ret_access = access(complete_path, F_OK)) == -1 &&
		errno != ENOENT){
			perror("error in access");
			exit(EXIT_FAILURE);
	}	
	
	else if (ret_access == 0){ //the file still exist
		printf("file still exist\n");
		//possibilità di aggiunger (1) anche qui.
		return;
	}

	//crea file
	FILE *new_file; 
	new_file = fopen(complete_path, "w+");
	if(new_file == NULL){
		printf("error in creation of '%s'\n", complete_path);		
		exit(1);		
	}	

	//invio comando con nome
	if (send_data(sockfd, file_to_get, GET | CHAR_INDICATOR) < 0){
		perror("errore in sendto");
		exit(1);
	}

	//ricevi messaggi	
	int n = receive_data(sockfd, new_file, NULL);
	if (n < 0) {
		perror("errore in thread_recvfrom");
		exit(1);	
	}	
	fclose(new_file);

	free(file_to_get);
	free(complete_path);
}



void client_list_operation(int sockfd){

	//faccio partire timer 
	//setsockopt(new_sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

	if (send_data(sockfd, NULL, LIST) < 0){
		perror("errore in sendto");
		exit(1);
	}
	printf("sent request\n");

	//attesa di ack + data
	unsigned char *list = NULL;

	if (receive_data(sockfd, &list, NULL ) < 0) {
		perror("errore in recvfrom");
		exit(1);
	}

	//stampo contenuto
	 printf("list message content:\n%s\n", list);
	 free(list);
}
