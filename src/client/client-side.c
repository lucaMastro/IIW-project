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
#include <signal.h> 

#include "../../lib/structs/message_struct.h"
#include "../../lib/structs/sending_queue.h"
#include "../../lib/all_f.h"

#include "../../lib/serialize/serialize.h"
#include "../../lib/serialize/deserialize.h"
#include "../../lib/passive_functions/reliable-conn/sending_queue_manager.h"
#include "../../lib/readwrite/read-write.h"
#include "../../lib/passive_functions/passive-functions.h"
//#include "../../lib/passive_functions/passive-functions.h"
#include "../../lib/active_functions/client_operations.h"

#define SERV_PORT   5193 
#define TIMER 		2
#define MAX_CMD_SIZE 200



int main(int argc, char *argv[ ]) {
	int sockfd, cmd_sock, data_sock;
	struct sockaddr_in servaddr, cmd_server_addr, data_server_addr;
//	int command;
	char command[MAX_CMD_SIZE];

	if (argc != 2) { /* controlla numero degli argomenti */
		printf("utilizzo: client <indirizzo IP server>\n");
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket */
		perror("errore in socket");
		exit(EXIT_FAILURE);
	}

	memset((void *)&servaddr, 0, sizeof(servaddr));      /* azzera servaddr */
	servaddr.sin_family = AF_INET;       
	servaddr.sin_port = htons(SERV_PORT);  /* assegna la porta del server */
	/* assegna l'indirizzo del server prendendolo dalla riga di comando. L'indirizzo è una stringa da convertire in intero secondo network byte order. */
	if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
		printf("errore in inet_pton per %s", argv[2]);
		exit(1);
	}

	/*	invio di syn	*/
	if (send_unconnected(sockfd, NULL, NULL, &servaddr, SYN) < 1){
		perror("errore in sendto");
		exit(EXIT_FAILURE);
	}


		//in attesa di SYN ACK, ovvero in attesa del nuovo numero di porta
		unsigned char *new_port = NULL;
		char *tmp;
		if (receive_unconnected(sockfd, NULL, &new_port, NULL, NULL) < 0) {
			perror("errore in recvfrom");
			exit(1);
		}
		printf("new_port = %s\n", new_port);

		//gestione delle due porte:
		tmp = strtok((char*)new_port, " ");
		int cmd_port = atoi(tmp);
		
		tmp = strtok(NULL, " ");
		int data_port = atoi( tmp);

		printf("port int: %d, %d\n", cmd_port, data_port);

		free(new_port);

		//invio ack
		if ((cmd_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("errore in socket");
			exit(EXIT_FAILURE);
		}
		memset((void *)&cmd_server_addr, 0, sizeof(cmd_server_addr)); 
		cmd_server_addr.sin_family = AF_INET;
		cmd_server_addr.sin_port = htons(cmd_port);//porta presa dal mess

		if (inet_pton(AF_INET, argv[1], &cmd_server_addr.sin_addr) <= 0) {
			/* inet_pton (p=presentation) vale anche per indirizzi IPv6 */
			fprintf(stderr, "errore in inet_pton per %s", argv[1]);
			exit(1);
		}

		//devo dare tempo al thread di generarsi
		sleep(0.5);

		if (send_unconnected(cmd_sock, NULL, NULL, &cmd_server_addr, ACK) < 0){
			perror("errore in sendto");
			exit(1);
		}

		socklen_t size = sizeof(servaddr);
		if (connect(cmd_sock, (struct sockaddr *)&cmd_server_addr, size) < 0){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}

		close(sockfd);
		printf("connected\n");

		//connecting data_sock
		if ((data_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("errore in socket");
			exit(EXIT_FAILURE);
		}
		memset((void *)&data_server_addr, 0, sizeof(data_server_addr)); 
		data_server_addr.sin_family = AF_INET;
		data_server_addr.sin_port = htons(data_port);//porta presa dal mess

		if (inet_pton(AF_INET, argv[1], &data_server_addr.sin_addr) <= 0) {
			/* inet_pton (p=presentation) vale anche per indirizzi IPv6 */
			fprintf(stderr, "errore in inet_pton per %s", argv[1]);
			exit(1);
		}

		sleep(0.5);
		if (send_unconnected(data_sock, NULL, NULL, &data_server_addr, ACK) < 0){
			perror("errore in sendto");
			exit(1);
		}

		if (connect(data_sock, (struct sockaddr *)&data_server_addr, size) < 0){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}
	

	
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
