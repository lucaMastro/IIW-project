#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <dirent.h>

#define SERVER_FOLDER "server-files/"

int check_for_existing_file(int semaphore, char *path, 
		FILE **to_create, char **new_name){
	
	char *complete_path;
	int len = strlen(SERVER_FOLDER);
	int ret_access;
	int flag = ACK;
	struct sembuf sops;
	sops.sem_flg = 0;
	sops.sem_num = 0;

	//try getting coin from sem
	sops.sem_op = -1;

	len += strlen(path);

	complete_path = (char*) malloc(sizeof(char) * (len + 1));
	if (complete_path == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) complete_path, 0, len + 1);
	strcat(complete_path, SERVER_FOLDER);
	strcat(complete_path, path);
	

	if (semop(semaphore, &sops, 1) == -1){
		perror("error in getting coin");
		return -1;
	}

	//got the coin. check for existing
	if ((ret_access = access(complete_path, F_OK)) == -1 &&
		errno != ENOENT){
			perror("error in access");
			exit(EXIT_FAILURE);
	}	
	
	else if (ret_access == 0){ //the file still exist
		printf("file still exist\n");
		//rinonimo file e continuo
		free(complete_path);
		int new_name_len = strlen(path) + 1;
		*new_name = (char*) malloc(sizeof(char) * new_name_len);
		if (*new_name == NULL){
			perror("error in new_name malloc");
			exit(EXIT_FAILURE);
		}
		memset((void*) *new_name, 0, new_name_len);
		complete_path = change_name(path, SERVER_FOLDER, new_name);
	}
	else//file doeesn exit
		*new_name = NULL;

	

	*to_create = fopen(complete_path, "w+");
	if (*to_create == NULL){
		printf("error in %s fopen\n", complete_path);
		return -1;
	}

	/*	file create. unlock the semaphore	*/
	sops.sem_op = 1;

	if (semop(semaphore, &sops, 1) == -1){
		perror("error in releasing coin");
		return -1;
	}	
	free(complete_path);

	return 0;
}

void server_put_operation(int cmd_sock, int data_sock, char *file_name, int sem_id){

	int n;
	FILE *file_received;
	char *new_file_name;
	//controllo se già non esiste, in caso rinominalo es pippo1.txt
	if (check_for_existing_file(sem_id,
				file_name,
				&file_received,
				&new_file_name) < 0){
		exit(EXIT_FAILURE);	
	}


	Message ack, *m;
	int flag = ACK;
	if (new_file_name != NULL)
		flag = flag | CHAR_INDICATOR;

	make_packet(&ack, new_file_name, 0, 0, flag);
	
	if (new_file_name != NULL)
		free(new_file_name);

	struct timeval to;
	to.tv_sec = (Tsec) << 2;
	to.tv_usec = (Tnsec / 1000) << 2;
	while(1) {
		if (!is_packet_lost())
			send_packet(cmd_sock, &ack, NULL);
		
		setsockopt(cmd_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
		m = receive_packet(cmd_sock, NULL);
		if (m == NULL){
			if (errno != EAGAIN){
				perror("error in new receive packet lsot");
				exit(EXIT_FAILURE);
			}
			else{
				break; //timeout, client has read err mex
			}
		}
		else
			free(m);
	}
	to.tv_sec = 0;
	to.tv_usec = 0;
	setsockopt(cmd_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));

	receive_data(data_sock, cmd_sock, file_received, NULL);
	if (fclose(file_received) == EOF){
		perror("error closing file in put");
		exit(EXIT_FAILURE);
	}
}

void server_get_operation(int cmd_sock, int data_sock, char *file_requested){
	
	char *complete_path;
	int len = strlen(SERVER_FOLDER) + strlen(file_requested) + 1;
	Message mex;

	complete_path = (char *) malloc(sizeof(char) * len);
	if (complete_path == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	memset((void*) complete_path, 0, len);
	sprintf(complete_path, "%s%s", SERVER_FOLDER, file_requested);
	
	//verifico se il file esiste effettivamente
	if(access(complete_path, F_OK) == -1){
		if (errno == ENOENT){
			//send error message
			make_packet(&mex, NULL, 0, 0, ACK | FILE_NOT_FOUND);
			Message *m;
			struct timeval to;
			to.tv_sec = (Tsec) << 2;
			to.tv_usec = (Tnsec / 1000) << 2;

			while(1) {
				if (!is_packet_lost())
					send_packet(cmd_sock, &mex, NULL);
				
				setsockopt(cmd_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
				m = receive_packet(cmd_sock, NULL);
				if (m == NULL){
					if (errno != EAGAIN){
						perror("error in new receive packet lsot");
						exit(EXIT_FAILURE);
					}
					else{
						break; //timeout, client has read err mex
					}
	
				}
				else
					free(m);
			}
			to.tv_sec = 0;
			to.tv_usec = 0;
			setsockopt(cmd_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
			return;
		}
		else{
			perror("error in access");
			exit(EXIT_FAILURE);
		}

	}
	else{
		make_packet(&mex, NULL, 0, 0, ACK);
		//doesnt matter if ack lost: i will send data
		if (!is_packet_lost())
			send_packet(cmd_sock, &mex, NULL);
	}

	//apro file
	FILE *file_to_send;
	file_to_send = fopen(complete_path, "rb"); 

	if (send_data(data_sock, cmd_sock, file_to_send, 0, NULL) < 0){
		perror("errore in sendto");
		exit(EXIT_FAILURE);
	}
	if (fclose(file_to_send) == EOF){
		perror("error closing file in get");
		exit(EXIT_FAILURE);
	}
}


char *make_file_list(){

	int len = 0, old_len, curr_name_len;
	char *file_list = NULL;

	struct dirent *file_info;

	DIR *dir = opendir(SERVER_FOLDER);
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
		
		file_list = (char*)realloc(file_list, len + 1);
		if (file_list == NULL){
			perror("error in realloc");
			exit(EXIT_FAILURE);
		}
		sprintf(file_list + old_len, "%s ", file_info -> d_name );
		*(file_list + len) = '\0';

	}

	if (closedir(dir) < 0){
		perror("error closing dir"); 
		exit(EXIT_FAILURE);
	}
	return file_list;

}


void server_list_operation(int cmd_sock, int data_sock){
	char *file_list = make_file_list();
	
	send_data(data_sock, cmd_sock, file_list, CHAR_INDICATOR, NULL);
	free(file_list);
}
