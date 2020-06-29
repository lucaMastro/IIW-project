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



int main(int argc, char *argv[ ]) {
	int sockfd, cmd_sock, data_sock;
	struct sockaddr_in servaddr, cmd_server_addr, data_server_addr;
//	int command;
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

	
	printf("cmd_sock = %d\ndata_sock = %d\n", cmd_sock, data_sock);
		fd_set read_set;
		FD_ZERO(&read_set);
		int maxfd;
		Message *cmd;
		printf("Welcome to udt-reliable go-back-n ftp protocol.\n");
		while (1){
			//memset((void*) command, 0, MAX_CMD_SIZE);
			printf("\nftp > ");
			fflush(stdout);
			FD_SET(cmd_sock, &read_set);
			FD_SET(fileno(stdin), &read_set);

			maxfd = fileno(stdin) < cmd_sock ? 
				(cmd_sock + 1) : (fileno(stdin) + 1);

			//l'ultimo parametro è struct timeval, per un timer
			if (select(maxfd, &read_set, NULL, NULL, NULL) < 0){
				perror("error in select");
				exit(EXIT_FAILURE);
			}

			printf("select unlocked\n");
			
			if (FD_ISSET(cmd_sock, &read_set)){
				char buf[1024];
				read(cmd_sock, buf, 1024 );
				printf("buf %s.\n", buf);
				//check se flag del messaggio ricevuto è un FIN
				cmd = receive_packet(cmd_sock, NULL);
				if (cmd -> flag & FIN){
					printf("unlock select by cmd socket. exiting\n");
					exit(EXIT_SUCCESS);
					
				}
			}
			else{
				//stdin:
				if (fgets(command, MAX_CMD_SIZE, stdin) == NULL){
					exit(EXIT_SUCCESS); //ctrl+c. need handler
				}				
				
				if ( strstr(command, "ls"))
					client_list_operation(cmd_sock, data_sock);
				
				else if ( strstr(command, "get\n")){
					client_get_operation(cmd_sock, data_sock);
				}
				else if ( strstr(command, "put")){
					client_put_operation(cmd_sock, data_sock);
				}
				else if ( strstr(command, "exit")){
					exit(EXIT_SUCCESS);
				}
			}

		}
	

	exit(0);
}
