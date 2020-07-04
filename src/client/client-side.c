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
#include <sys/ipc.h> 
#include <sys/sem.h> 
#include <time.h> 
#include <signal.h> 
#include <sys/types.h>
#include <sys/syscall.h>
#include <errno.h>

#define SERV_PORT   5193 
#define MAX_CMD_SIZE 200

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


int cmd_sock, data_sock;


void sigint_handler(){
	client_exit_operation(cmd_sock, data_sock);
	exit(EXIT_SUCCESS);
}


int manage_cmd_line(char *command, int data_sock, int cmd_sock){
	//this function parse the command line and check for command to execute
	
	char *list[4];
	if (fgets(command, MAX_CMD_SIZE, stdin) == NULL){
		client_exit_operation(cmd_sock, data_sock);
		exit(EXIT_SUCCESS); 
	}				
	else{
		command[strlen(command) - 1] = '\0'; //eliminating \n
		if ( !(strcmp(command, "")) ) //empty string
			return 1;
		
		//initializing the token list
		for (int i = 0; i < 4; i++)
			list[i] = NULL;

		//command to execute
		list[0] = strtok(command, " ");

		//storing params
		for (int i = 1; i < 4; i++)
			list[i] = strtok(NULL, " ");
		
		//list command executede when there arent params
		if ( !strcmp(list[0], "ls") ){
			if (list[1] == NULL)
				client_list_operation(cmd_sock, data_sock);
			else
				show_man(0);
		}

		//get executed only when there is at least one param
		else if ( !strcmp(list[0], "get") ){
			if (list[1] != NULL && list[3] == NULL){
				if (list[2] == NULL)
					list[2] = list[1];
				client_get_operation(cmd_sock, data_sock, list[1], list[2]);
			}
			else
				show_man(1);
		}

		//put executed only when there is at least one param
		else if ( !strcmp(list[0], "put") ){
			if (list[1] != NULL && list[3] == NULL){
				if (list[2] == NULL)
					list[2] = list[1];
				client_put_operation(cmd_sock, data_sock, list[1], list[2]);
			}
			else
				show_man(2);
		}

		//exit executed only when there arent params
		else if ( !strcmp(list[0], "exit") ){
			if (list[1] == NULL){
				client_exit_operation(cmd_sock, data_sock);
				exit(EXIT_SUCCESS);
			}
			else
				show_man(3);
		}

		//clear executed only when there arent params
		else if ( !strcmp(list[0], "clear") ){
			if (list[1] == NULL)
				printf("\e[1;1H\e[2J");
			else
				show_man(3);
		}

		//command not found
		else{
			printf("[Error]: Command not valid.\n");
		}
	}
	return 1;
}



int main(int argc, char *argv[ ]) {	
	
	int sockfd;
	char command[MAX_CMD_SIZE];
	fd_set read_set;
	int maxfd;
	Message *cmd;
	int valid_cmd = 1;	

	//check on arguments
	if (argc != 2) { 
		printf("[Usage]: %s <server IP address>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	//making socket to write address listen sock, cmd_sock and data_sock
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
		perror("errore in socket");
		exit(EXIT_FAILURE);
	}

	if ((cmd_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
		perror("errore in socket");
		exit(EXIT_FAILURE);
	}
	if ((data_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
		perror("errore in socket");
		exit(EXIT_FAILURE);
	}

	make_connection(sockfd, argv[1], &cmd_sock, &data_sock);
	close(sockfd);

	signal(SIGINT, sigint_handler);
	
	//initializing the set
	FD_ZERO(&read_set);

	printf("\e[1;1H\e[2J");
	printf("Welcome to udt-reliable go-back-n ftp protocol.\n");
	printf("You can use these operation:\n");
	printf("\t1. List, by running\n\tls\n\n");
	printf("\t2. Get, by running\n\tget <name_with_wich_file_is_store_on_server> [<name_with_wich_save_file>]\n\n");
	printf("\t3. Put, by running\n\tput <local_file_name> [<name_with_wich save_file_on_server>]\n\n");
	printf("\t4. Exit, by running\n\texit\n\n");
	printf("\t5. Clean, to clear the shell, by running\n\tclear\n\n");

	//setting the max fd
	maxfd = fileno(stdin) < cmd_sock ? (cmd_sock + 1) : (fileno(stdin) + 1);

	while (1){
		//printing prompt only when is executed a valid command. not printing
		//when the cycle restart after that select is unlocked by server
		if (valid_cmd){
			printf("\n");
			printf("ftp > "); //prompt
			fflush(stdout);
		}

		valid_cmd = 0;

		//adding stdin and cmd_sock
		FD_SET(cmd_sock, &read_set);
		FD_SET(fileno(stdin), &read_set);

		//it doesnt need to add data_sock at set
		if (select(maxfd, &read_set, NULL, NULL, NULL) < 0
				&& errno != EINTR){
			perror("error in select");
			exit(EXIT_FAILURE);
		}


		if (FD_ISSET(cmd_sock, &read_set)){
			//select unlocked by socket. maybe server crashed
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
		else{
			valid_cmd = manage_cmd_line(command, data_sock, cmd_sock);

#ifdef GET_TIME
			//close client and get time
			exit(EXIT_SUCCESS);
#endif
		}
	}

	exit(EXIT_SUCCESS);
}

