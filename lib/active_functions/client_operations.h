#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

//#include "../readwrite/read-write.h"

#define CLIENT_FOLDER "src/client/client-files/"

extern int h_errno;

void client_put_operation(int cmd_sock, int data_sock){
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
	//if (send_data(data_sock, cmd_sock, file_to_send, (PUT | CHAR_INDICATOR)) < 0){
	Message put;
	make_packet(&put, file_to_send, 0, 0, PUT |CHAR_INDICATOR);
	send_packet(cmd_sock, &put);
	//stampa_mess(&put);

	//wait for ack:
	Message *ack = receive_packet(cmd_sock);

	FILE *segment_file_transfert;
	segment_file_transfert = fopen(complete_path, "rb"); // la b sta per binario, sennò la fread non funziona
					
	if (send_data(data_sock, cmd_sock, segment_file_transfert, 0) < 0){
		perror("errore in sendto");
		exit(1);
	}
	free(file_to_send);
	free(complete_path);
}



void client_get_operation(int cmd_sock, int data_sock){

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


//	while(1){

	/*	if (send_data(data_sock, cmd_sock, file_to_get, GET | CHAR_INDICATOR) < 0){
			perror("errore in sendto");
			exit(1);
		}*/

		Message get;
		make_packet(&get, file_to_get, 0, 0,GET |CHAR_INDICATOR);
		send_packet(cmd_sock, &get);
		//stampa_mess(&get);

		Message *ack = receive_packet(cmd_sock);
		//ricevi messaggi	
		int n = receive_data(data_sock, cmd_sock, new_file, NULL);
		if (n < 0) {
			perror("errore in thread_recvfrom");
			exit(1);	
		}
//	}

	fclose(new_file);
	free(file_to_get);
	free(complete_path);
}



void client_list_operation(int cmd_sock, int data_sock){

	//faccio partire timer 
	//setsockopt(new_cmd_sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

	/*if (send_data(data_sock, cmd_sock, NULL, LIST) < 0){
		perror("errore in sendto");
		exit(1);
	}*/

	Message list_mex, *ack = NULL;
	make_packet(&list_mex, NULL, 0, 0, LIST);


	struct timeval timeout;
	do{
		send_packet(cmd_sock, &list_mex);
		printf("sent request\n");
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		//attesa di ack + data
		ack = receive_packet(cmd_sock);
	}
	while (ack == NULL || (ack -> flag & ACK) == 0);

	unsigned char *list = NULL;

	printf("waiting for data:\n");
	if (receive_data(data_sock, cmd_sock, &list, NULL ) < 0) {
		perror("errore in recvfrom");
		exit(1);
	}

	//stampo contenuto
	 printf("list message content:\n%s.\n", list);
	 free(list);
}
