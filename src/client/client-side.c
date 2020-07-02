#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <unistd.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h> 
#include <time.h> 
#include <signal.h> 
#include <sys/types.h>
#include <sys/syscall.h>

#define SERV_PORT   5193 

#include "../../lib/structs/message_struct.h"
#include "../../lib/structs/sending_queue.h"
#include "../../lib/all_f.h"

#include "../../lib/serialize/serialize.h"
#include "../../lib/serialize/deserialize.h"
#include "../../lib/passive_functions/reliable-conn/sending_queue_manager.h"
#include "../../lib/readwrite/read-write.h"
#include "../../lib/passive_functions/client-passive-functions.h"
#include "../../lib/passive_functions/common-passive-functions.h"
#include "../../lib/active_functions/client_operations.h"

#define TIMER 		2
#define MAX_CMD_SIZE 200


int cmd_sock, data_sock;


void sigint_handler(){
	client_exit_operation(cmd_sock, data_sock);
	exit(EXIT_SUCCESS);
}


int manage_cmd_line(char *command, int data_sock, int cmd_sock){
	
	char *list[4];
	if (fgets(command, MAX_CMD_SIZE, stdin) == NULL){
		printf("here");
		client_exit_operation(cmd_sock, data_sock);
		exit(EXIT_SUCCESS); //ctrl+c. need handler
	}				
	else{
		command[strlen(command) - 1] = '\0'; //eliminating \n
		if ( !(strcmp(command, "")) )
			return 1;
		
		for (int i = 0; i < 4; i++)
			list[i] = NULL;

		list[0] = strtok(command, " ");
		for (int i = 1; i < 4; i++)
			list[i] = strtok(NULL, " ");
		

		if ( !strcmp(list[0], "ls") ){
			if (list[1] == NULL)
				client_list_operation(cmd_sock, data_sock);
			else
				show_man(0);
		}

		else if ( !strcmp(list[0], "get") ){
			if (list[1] != NULL && list[3] == NULL){
				if (list[2] == NULL)
					list[2] = list[1];
				client_get_operation(cmd_sock, data_sock, list[1], list[2]);
			}
			else
				show_man(1);
		}

		else if ( !strcmp(list[0], "put") ){
			if (list[1] != NULL && list[3] == NULL){
				if (list[2] == NULL)
					list[2] = list[1];
				client_put_operation(cmd_sock, data_sock, list[1], list[2]);
			}
			else
				show_man(2);
		}
		else if ( !strcmp(list[0], "exit") ){
			if (list[1] == NULL){
				client_exit_operation(cmd_sock, data_sock);
				exit(EXIT_SUCCESS);
			}
			else
				show_man(3);
		}
		else if ( !strcmp(list[0], "clear") ){
			if (list[1] == NULL)
				printf("\e[1;1H\e[2J");
			else
				show_man(3);
		}
		else{
			printf("[Error]: Command not valid.\n");
		}
	}
	return 1;
}

int main(int argc, char *argv[ ]) {
	
	
	int sockfd;
	char command[MAX_CMD_SIZE];

	if (argc != 2) { /* controlla numero degli argomenti */
		printf("utilizzo: %s <indirizzo IP server>\n", argv[0]);
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket */
		perror("errore in socket");
		exit(EXIT_FAILURE);
	}

	if ((cmd_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket */
		perror("errore in socket");
		exit(EXIT_FAILURE);
	}
	if ((data_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket */
		perror("errore in socket");
		exit(EXIT_FAILURE);
	}

	make_connection(sockfd, argv[1], &cmd_sock, &data_sock);
	close(sockfd);

	signal(SIGINT, sigint_handler);
	
	fd_set read_set;
	FD_ZERO(&read_set);
	int maxfd;
	Message *cmd;
	int valid_cmd = 1;	
	printf("\e[1;1H\e[2J");
	printf("Welcome to udt-reliable go-back-n ftp protocol.\n");
	printf("You can use these operation:\n");
	printf("\t1. List, by running\n\tls\n\n");
	printf("\t2. Get, by running\n\tget <name_with_wich_file_is_store_on_server> <name_with_wich_save_file>\n\n");
	printf("\t3. Put, by running\n\tput <local_file_name> <name_with_wich save_file_on_server>\n\n");
	printf("\t4. Exit, by running\n\texit\n\n");
	printf("\t5. Clean, to clear the shell, by running\n\tclear\n\n");
	while (1){
		if (valid_cmd){
			printf("\n");
		//memset((void*) command, 0, MAX_CMD_SIZE);
			printf("ftp > ");
			fflush(stdout);
		}
		valid_cmd = 0;
		FD_SET(cmd_sock, &read_set);
		FD_SET(fileno(stdin), &read_set);

		maxfd = fileno(stdin) < cmd_sock ? 
			(cmd_sock + 1) : (fileno(stdin) + 1);

		if (select(maxfd, &read_set, NULL, NULL, NULL) < 0
				&& errno != EINTR){
			perror("error in select");
			exit(EXIT_FAILURE);
		}


		if (FD_ISSET(cmd_sock, &read_set)){
			//check se flag del messaggio ricevuto è un FIN
			cmd = receive_packet(cmd_sock, NULL);
			if (cmd == NULL){
				perror("here error receiving packet cmd");
				exit(EXIT_FAILURE);
			}
			if (cmd -> flag & FIN){
				printf("unlock select by cmd socket. exiting\n");
				exit(EXIT_SUCCESS);
			}
		}
		else
			valid_cmd = manage_cmd_line(command, data_sock, cmd_sock);
	}

	exit(0);
}

