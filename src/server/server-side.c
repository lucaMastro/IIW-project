#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "../../lib/readwrite/read-write.h"
#include "../../lib/functions/functions.h"

extern int h_errno;

#define SERV_PORT   5193 
#define MAXLINE     1024
#define DIM_FILE_LIST 200


struct port_thread{

  int port_client;
  int port_new_socket;
  int thread_index;
  int semaphore;
  //indirizzo ip mittente;
};

int generate_port(){
	return (rand() % (65535 + 1 - 49152)) + 49152;
}

int check_port(int port, int * array){
	int i; 
    for(i = 0; i < PORT_NUMBER_MAX; i++){ 
        if (array[i] == port)
			return 0;
    } 
    return 1; 
}

void insert_new_port(int port, int *array){

    static int position = 0; 
    int i = 0;

    array[position] = port; 

    printf("Port numbers busy : \n");
    while (array[i] != 0){ 
      printf("%d\n", array[i]);
      i++;
    }

    position ++;
    return;
}

void timer_handler (int signum){

  printf ("Timed out!\n");
  //uccidi thread 
  //rimuovi la sua porta dall array
}


/*	riceve dal main la nuova porta e porta client 
 *	rimane in attesa dell' ACK per 5 secondi
 *	quando scade il timer il thread muore
 *	e client dovrà rinviare il messaggio 
 *	se riceve ACK rimane in attesa di comandi	*/


void *thread_function(void * port){
	/* Start a timer that expires after 5 seconds */
	struct itimerval timer;
	timer.it_value.tv_sec = 5;
	timer.it_value.tv_usec = 0;  //500000 = 0.5 seconds
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, 0);
	signal(SIGALRM, timer_handler);  

	struct port_thread *my_port = (struct port_thread *) port;
	printf("thread index: %d\nporta client: %d\nporta thread: %d\n", (*my_port).thread_index, (*my_port).port_client, (*my_port).port_new_socket);

	int sockfd_thread;
	int semaphore = my_port -> semaphore;
	struct sockaddr_in *addr_thread = malloc (sizeof(addr_thread));

	//creo socket
	if ((sockfd_thread = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
		perror("errore in socket_thread");
		exit(1);
	}

	memset((void *)addr_thread, 0, sizeof(*addr_thread));
	addr_thread->sin_family = AF_INET;
	addr_thread->sin_addr.s_addr = htonl(INADDR_ANY); 
	addr_thread->sin_port = htons((*my_port).port_new_socket);


	/* assegna l'indirizzo al socket */
	if (bind(sockfd_thread, (struct sockaddr *)addr_thread, sizeof(*addr_thread)) < 0) {
		perror("errore in thread_bind");
		exit(1);
	}

	Message *received_ACK = malloc(sizeof(Message));
	if (received_ACK == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}

	//waiting for ACK
	int n;
	struct sockaddr_in client_addr;
	client_addr.sin_family = AF_INET;
	struct timeval timeout = {2,0}; //set timeout for 2 seconds

	while(1){
		//n = readn(sockfd_thread, received_ACK, NULL);

		//n = readmessage(sockfd_thread, &received_ACK, &client_addr);
		n = receive_data(sockfd_thread, NULL, NULL, &client_addr, NULL);
		if (n < 0) {
			perror("errore in thread_recvfrom");
			exit(1);
		}


  		//disattivo timer ack
		timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = 0;
		setitimer (ITIMER_REAL, &timer, 0);
		break;
	}

	printf("Ack received\n\n");
	
	printf("connect\n");
	socklen_t size = sizeof(client_addr);
	if (connect(sockfd_thread, (struct sockaddr*) &client_addr, size) < 0){
		perror("error in connect");
		exit(EXIT_FAILURE);
	}


	//waiting for command
	Message *command_message_thread = malloc(sizeof(Message)); 
	if (command_message_thread == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}

	int flag;
	unsigned char *name_file;
	while(1){

		printf("\nServer waitings for command\n");

		name_file = NULL;
		n = receive_data(sockfd_thread, NULL, &name_file, addr_thread, &flag);
		if (n < 0) {
			perror("errore in thread_recvfrom");
			exit(1);
		}

		switch(flag & (LIST | PUT | GET | FIN)){

			//list case
			case LIST:

				printf("list case received\n");

				char file_thread[DIM_FILE_LIST];
				memset((void*) file_thread, 0, DIM_FILE_LIST);
				list_function(file_thread, sizeof(file_thread));
				printf("len: %ld\n", strlen(file_thread));
				
				//invio ack + data
				printf("invio ack + data\n");
				if (send_data(sockfd_thread, NULL, (unsigned char*)file_thread, &client_addr, ACK) < 0){
						perror("errore in sendto");
						exit(1);
				}


				break;

			//get case
			case GET:

				printf("get case received\n");
				printf("Request of download for '%s' \n",name_file);
				
				//verifico se il file esiste effettivamente
				if(access(name_file, F_OK) == -1){
					printf("The file %s doesn't exist\n", name_file);
					//avverti client
					break;
				}

				//invio richiesta di put al client

				//apro file
				FILE *segment_file_transfert;
				segment_file_transfert = fopen(name_file, "rb"); // la b sta per binario, sennò la fread non funziona

				//inizio trasmissione, praticamnete put case lato server
				if (send_data(sockfd_thread, segment_file_transfert, NULL, addr_thread, 0) < 0){
					perror("errore in sendto");
					exit(1);
				}

				//attesa ack

				break;

			//put case file
			case PUT:

				printf("put case received\n");
				//creo il file
				FILE *file_received;

				//controllo se già non esiste, in caso rinominalo es pippo1.txt
				printf("trying getting coin\n");
				if (check_for_existing_file(semaphore,
							name_file,
							&file_received,
							my_port -> thread_index ) < 0){
					exit(EXIT_FAILURE);
				}
				
				n = receive_data(sockfd_thread, file_received, NULL, addr_thread, NULL);
				if (n < 0) {
					perror("errore in thread_recvfrom");
					exit(1);
				}
				
				fclose(file_received);
				break;

			case FIN:

				//invio ack 
				printf("Exit case received\n");
				//dealloco, rimuovo la mia porta
				free(command_message_thread);
				delete_new_port((*my_port).port_new_socket);
				return NULL;
		}

		free(name_file);

	}

	return NULL;
}


int main(int argc, char **argv) {
  
	socklen_t len;
	struct sockaddr_in addr;
	int sockfd;
	pthread_t tid;
	int thread_count = 0; 
    int new_port_num;
   	int val = 0;
	int flag;
	int semaphore; //sempahore to check name files in put requests
	Message *message_main, *syn_request;
	
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket */
		perror("errore in socket");
		exit(1);
	}
	
	semaphore = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
	if (semaphore < 0){
		perror("error inizializing semaphore");
		exit(EXIT_FAILURE);
	}
	if (semctl(semaphore, 0, SETVAL, 1) < 0){
		perror("error inizializing sem value");
		exit(EXIT_FAILURE);	
	}

	memset((void *)&addr, 18, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	addr.sin_port = htons(SERV_PORT ); 

	/* assegna l'indirizzo al socket */
	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("errore in bind");
		exit(1);
	}

	//messaggio di SYN_ACK
	/*message_main = malloc (sizeof(Message));
	if (message_main == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}

	message_main -> seq_num = 0;
	message_main -> ack_num = 0;
	message_main -> flag = 1;
	message_main -> rec_win = 0;
	message_main -> length = 0;
*/
	array_port = (int *) malloc ((sizeof (int)) * PORT_NUMBER_MAX);
	struct port_thread *struct_port;
	syn_request = malloc(sizeof(Message));
	if (syn_request == NULL){
		perror("error in malloc");
		exit(EXIT_FAILURE);
	}
	while (1) {

		//dovrà essere deallocata nel thread
		struct_port = malloc (sizeof(struct port_thread));

		struct sockaddr_in client_addr;
		client_addr.sin_family = AF_INET;

		//if (readmessage(sockfd, &syn_request, &client_addr) < 0){
		if (receive_data(sockfd, NULL, NULL, &client_addr, &flag) < 0){
			perror("errore in recvfrom");
			exit(1);
		}
		/*	check sul flag	*/
		printf("syn received");

	    // prendo numero porta client
    	getpeername(sockfd, (struct sockaddr*)&client_addr, &len);
	//	if(ntohs(client_addr.sin_port) == 5193) continue;

    	/* genero una nuova porta per la socket del thread e
		 * passo le porte al thread */
    	struct_port -> port_client = ntohs(client_addr.sin_port);
    
	    //check se numero di porta già occupato
		//while(val == 0)
		do{
			//numero di porta non valido, val = 1 se porta non presente
			new_port_num = generate_port();
			val = check_port(new_port_num, array_port);
		}
		while(val == 0);

		//val = 0;
		struct_port -> port_new_socket = new_port_num;

		//inserisco il nuovo numero di porta
		insert_new_port(new_port_num, array_port);

		struct_port -> semaphore = semaphore;
		struct_port -> thread_index = thread_count;
		pthread_create(&tid, NULL, thread_function, (void *)struct_port);
		thread_count ++;

		//message_main -> new_port = new_port_num;


		unsigned char port_num_string[6];
		memset(port_num_string, 0, 6);
		sprintf(port_num_string, "%d", new_port_num);

	//	if (writemess(sockfd, message_main, &client_addr) < 0){
		if (send_data(sockfd, NULL, port_num_string, &client_addr, SYN_ACK) < 0){
			perror("errore in sendto");
			exit(EXIT_FAILURE);
		}
	}
	exit(0);
}
