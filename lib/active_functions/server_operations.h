/*#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "../readwrite/read-write.h" */

#define SERVER_FOLDER "src/server/server-files/"

int check_for_existing_file(int semaphore, char *path, 
		FILE **to_create, char *new_file_name){
	
	char *complete_path;
	int len = strlen(SERVER_FOLDER);
	int ret_access;
	struct sembuf sops;
	sops.sem_flg = 0;
	sops.sem_num = 0;

	//try getting coin from sem
	sops.sem_op = -1;

	len += strlen(path);
	complete_path = (char*) malloc(sizeof(char) * len);
	if (complete_path == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) complete_path, 0, len);
	strcat(complete_path, SERVER_FOLDER);
	strcat(complete_path, path);
	printf("%s\n", complete_path);

	if (semop(semaphore, &sops, 1) == -1){
		perror("error in getting coin");
		return -1;
	}

	printf("coin got\n");
	//got the coin. check for existing
	if ((ret_access = access(complete_path, F_OK)) == -1 &&
		errno != ENOENT){
			perror("error in access");
			exit(EXIT_FAILURE);
	}	
	
	else if (ret_access == 0){ //the file still exist
		printf("file still exist\n");
		//rinonimo file e continuo
	}

	*to_create = fopen(complete_path, "w+");
	if (*to_create == NULL){
		printf("error in %s fopen\n", complete_path);
		return -1;
	}

	/*	file create. unlock the semaphore	*/
	sops.sem_op = 1;

/*	if (thread_num == 0)
		sleep(10);*/
	if (semop(semaphore, &sops, 1) == -1){
		perror("error in releasing coin");
		return -1;
	}	
	printf ("coin released\n");
	free(complete_path);

	return 0;
}

void server_put_operation(int sockfd, char *file_name, int sem_id){

	int n;
	FILE *file_received;
	char *new_file_name = NULL;
	//controllo se già non esiste, in caso rinominalo es pippo1.txt
	if (check_for_existing_file(sem_id,
				file_name,
				&file_received,
				new_file_name) < 0){
		exit(EXIT_FAILURE);	
	}

	n = receive_data(sockfd, file_received, NULL);
	if (n < 0) {
		perror("errore in thread_recvfrom");
		exit(EXIT_FAILURE);	
	}	
	fclose(file_received);
}

void server_get_operation(int sockfd, char *file_requested){
	
	char *complete_path;
	int len = strlen(SERVER_FOLDER) + strlen(file_requested);

	complete_path = (char *) malloc(sizeof(char) * len);
	if (complete_path == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) complete_path, 0, len);
	strcat(complete_path, SERVER_FOLDER);
	strcat(complete_path, file_requested);
	
	printf("Request of download for '%s' \n",file_requested);			

	//verifico se il file esiste effettivamente
	if(access(complete_path, F_OK) == -1){
		if (errno == ENOENT){
			//send error message
		}
		else{
			perror("error in access");
			exit(EXIT_FAILURE);
		}

	}

	//apro file
	FILE *file_to_send;
	file_to_send = fopen(complete_path, "rb"); 

	if (send_data(sockfd, file_to_send, 0) < 0){
		perror("errore in sendto");
		exit(EXIT_FAILURE);
	}
	//attesa ack
}


char *make_file_list(){

	int len = 0, old_len, curr_name_len;
	char *file_list = NULL;
	char *next_pos;

	DIR *dir = opendir(SERVER_FOLDER);
	struct dirent *file_info;

	if (dir == NULL){
		perror("error opening directory");
		exit(EXIT_FAILURE);
	}

	while ( (file_info = readdir(dir)) != NULL){
		if ( strcmp(file_info -> d_name, "..") == 0  || 
				strcmp(file_info -> d_name, ".") == 0) 
			continue; 

		curr_name_len = strlen(file_info -> d_name);

		old_len = len; //that's the offset
		len += curr_name_len;
		len++; //space beetwen names
		
		file_list = (char*)realloc(file_list, len);
		if (file_list == NULL){
			perror("error in realloc");
			exit(EXIT_FAILURE);
		}
		next_pos = file_list + old_len;
		strcpy(next_pos, file_info -> d_name);
		strcpy(next_pos + len - 1, " ");

	}

	printf ("returning: %s.\n", file_list);
	return file_list;

}


void server_list_operation(int sockfd){
	char *file_list = make_file_list();
	
	printf("file list = %s\n", file_list);
	send_data(sockfd, file_list, CHAR_INDICATOR);
	free(file_list);
}
