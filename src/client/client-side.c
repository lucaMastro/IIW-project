#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <unistd.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SERV_PORT   5193 
#define TIMER 		2

#include "../../lib/readwrite/read-write.h"


int main(int argc, char *argv[ ]) {
	int sockfd, new_sockfd;
	struct sockaddr_in servaddr, new_serveraddr;
	Message *syn_request, *new_message, *ack;
	int command;

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
	if (send_data(sockfd, NULL, NULL, &servaddr, SYN) < 0){
		perror("errore in sendto");
		exit(EXIT_FAILURE);
	}

	socklen_t size = sizeof(servaddr);
	if (connect(sockfd, (struct sockaddr *)&servaddr, size) < 0){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}

	while(1){
		//in attesa di SYN ACK, ovvero in attesa del nuovo numero di porta
		unsigned char *new_port = NULL;
		if (receive_data(sockfd, NULL, &new_port, NULL, NULL) < 0) {
			perror("errore in recvfrom");
			exit(1);
		}
		printf("new_port = %s\n", new_port);

		int new_port_num = atoi((char*)new_port);
		free(new_port);

		/*	ricevuto il pacchetto, bisogna controllare se il flag di syn-ack
		 *	è attivo. se non lo è, scarta il pacchetto, se lo è, quel 
		 *	pacchetto conterrà nel campo data il nuovo numero di porta del 
		 *	server su cui devo inviare ack. per questo motivo devo chiamare
		 *	atoi() sul campo data. */

		/*	probabilmente è necessario un check sul flag	*/

		//invio ack su nuova porta, quindi creo nuova socket e una nuova struttura addr
		if ((new_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("errore in socket");
			exit(EXIT_FAILURE);
		}
		memset((void *)&new_serveraddr, 0, sizeof(new_serveraddr)); 
		new_serveraddr.sin_family = AF_INET;
		new_serveraddr.sin_port = htons(new_port_num);//porta presa dal mess

		if (inet_pton(AF_INET, argv[1], &new_serveraddr.sin_addr) <= 0) {
			/* inet_pton (p=presentation) vale anche per indirizzi IPv6 */
			fprintf(stderr, "errore in inet_pton per %s", argv[1]);
			exit(1);
		}

		//devo dare tempo al thread di generarsi, problema di sincronizzazione!!!!
		sleep(1);

		//if (writemess(new_sockfd, ack, &new_serveraddr) < 0){
		if (send_data(new_sockfd, NULL, NULL, &new_serveraddr, ACK) < 0){
			perror("errore in sendto");
			exit(1);
		}

		//connect?

	/*	Message *command_message = malloc(sizeof(Message)); 
		Message *data_msg = malloc(sizeof(Message));*/

		char *file_upload = (char *)malloc(100*sizeof(char));
		char *file_download = (char *)malloc(100*sizeof(char));
		struct stat st;
		struct timeval timeout = {TIMER,0}; //set timeout for 2 seconds
		int w_tx = 1;
		int *save_flag = (int *) malloc(sizeof(4));

		//invio comandi:
		while(1){

			printf("\nEnter any of the following commands: \n 1 -> ls \n 2 -> get \n 3 -> put \n 4 -> exit \n ");		
			scanf("%d", &command);

			switch(command){

				//list case
				case 1:
					
					//faccio partire timer 
					//setsockopt(new_sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

					if (send_data(new_sockfd, NULL, NULL, &new_serveraddr, LIST) < 0){
						perror("errore in sendto");
						exit(1);
					}

					printf("in attesa di ack + data\n");

					//attesa di ack + data
					unsigned char *list = NULL;

					if (receive_data(new_sockfd, NULL, &list, NULL, save_flag) < 0) {
						perror("errore in recvfrom");
						exit(1);
					}

					if(*save_flag == ACK){

						printf("ricevuto ack\n");
					}


					//stampo contenuto
					 printf("list message content:\n%s\n", list);
					 free(list);
					 break;

				//get case
				case 2:

					printf("Which file you want to download ?\n");
					scanf("%s", file_download);

					//controllo se non è già presente nel client

					//invio comando con nome
					if (send_data(new_sockfd, NULL, file_download, &new_serveraddr, GET) < 0){
						perror("errore in sendto");
						exit(1);
					}


					//in attesa di ack

					//crea file
					FILE* new_file; 
					new_file = fopen(file_download, "w+");

					if(new_file == NULL){

						printf("error in creation of '%s'\n", file_download);
						exit(1);
					}

					//ricevi messaggi
					int n = receive_data(new_sockfd, new_file, NULL, &new_serveraddr, NULL);
					if (n < 0) {
						perror("errore in thread_recvfrom");
						exit(1);
					}
					
					fclose(new_file);
					break;

				//put case
				case 3:
					
					printf("What file do you want to upload ?\n");
					scanf("%s", file_upload);

					//verifico se il file esiste effettivamente
					if(access(file_upload, F_OK) == -1){
						printf("The file %s doesn't exist\n", file_upload);
						break;
					}
					//check se lunghezza > MSS in caso impossibile inviare


					//invio comando
					if (send_data(new_sockfd, NULL, (unsigned char *)file_upload, &new_serveraddr, PUT) < 0){
						perror("errore in sendto");
						exit(1);
					}

					FILE *segment_file_transfert;
					segment_file_transfert = fopen(file_upload, "rb"); // la b sta per binario, sennò la fread non funziona
					
					//devo conoscere la dimensione del file
					if(stat(file_upload, &st) == -1){
						printf("error in stat()\n");
						exit(1);
					}

					if (send_data(new_sockfd, segment_file_transfert, NULL, &new_serveraddr, 0) < 0){
						perror("errore in sendto");
						exit(1);
					}

					//aspetto ack
					
					//free(file_upload);
					break;

				//exit case
				case 4:

					if (send_data(new_sockfd, NULL, NULL, &new_serveraddr, FIN) < 0){
						perror("errore in sendto");
						exit(1);
					}

					exit(1);

				//command not valid
				default:

					printf("command added not valid\n");
					break;
	
			}	
			memset(file_upload, 0, 100);
			memset(file_download, 0, 100);

		}
	}

	exit(0);
}
