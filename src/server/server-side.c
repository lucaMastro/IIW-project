//#define _GNU_SOURCE 
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
#include <sys/types.h>
#include <sys/syscall.h>
extern int h_errno;

#include "../../lib/structs/message_struct.h"
#include "../../lib/structs/sending_queue.h"
#include "../../lib/all_f.h"

#include "../../lib/passive_functions/passive-functions.h"
#include "../../lib/passive_functions/reliable-conn/sending_queue_manager.h"
//#include "../../lib/passive_functions/passive-functions.h"
#include "../../lib/serialize/serialize.h"
#include "../../lib/serialize/deserialize.h"
#include "../../lib/readwrite/read-write.h"
#include "../../lib/active_functions/server_operations.h"


#define SERV_PORT   5193 
#define DIM_FILE_LIST 200
#define MAX_PORT_NUMBER 200
#define SERVER_FOLDER "src/server/server-files/"

struct port_thread{

  int port_client;
  int port_new_socket[2];
  int thread_index;
  int semaphore;
  //indirizzo ip mittente;
};



int check_port(int port, int * array){
	int i; 
    for(i = 0; i < MAX_PORT_NUMBER; i++){ 
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

void generate_port(int *array_port, int *new_port_nums){
	int port_num, val, i = 0;
	srand(time(0));

	do{
		port_num = (rand() % (65535 + 1 - 49152)) + 49152;
		val = check_port(port_num, array_port);
		if (val == 1){
			new_port_nums[i] = port_num;
			//inserisco il nuovo numero di porta
			insert_new_port(port_num, array_port);
			i++;
		}
	}
	while(i < 2);
}



void timer_handler (int signum){

  printf ("Timed out!\n");
  //uccidi thread 
  //rimuovi la sua porta dall array
}



void *thread_function(void * port){
	int semaphore;
	struct itimerval timer;
	struct port_thread *my_port;
	int n;
	int flag;
	char *name_file;

	int cmd_sock, data_sock;
	struct sockaddr_in cmd_addr_thread, data_addr_thread;
	struct sockaddr_in cmd_addr_client, data_addr_client;
	const socklen_t size = sizeof(struct sockaddr_in);

	/* Start a timer that expires after 5 seconds */
	timer.it_value.tv_sec = 5;
	timer.it_value.tv_usec = 0;  //500000 = 0.5 seconds
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, 0);
	signal(SIGALRM, timer_handler);  

	my_port = (struct port_thread *) port;
/*	printf("thread index: %d\nporta thread: %d\n",
			my_port -> thread_index, 
			//my_port -> port_client, 
			my_port -> port_new_socket);*/

	semaphore = my_port -> semaphore;
	//struct sockaddr_in *cmd_addr_thread = malloc (sizeof(addr_thread));

	//creo socket
	if ((cmd_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
		perror("errore in socket_thread");
		exit(EXIT_FAILURE);
	}

	memset((void *)&cmd_addr_thread, 0, sizeof(cmd_addr_thread));
	cmd_addr_thread.sin_family = AF_INET;
	cmd_addr_thread.sin_addr.s_addr = htonl(INADDR_ANY); 
	cmd_addr_thread.sin_port = htons(my_port -> port_new_socket[0]);


	/* assegna l'indirizzo al socket */
	if (bind(cmd_sock, (struct sockaddr *)&cmd_addr_thread,
				sizeof(cmd_addr_thread)) < 0) {
		perror("errore in thread_bind");
		exit(EXIT_FAILURE);
	}


	//waiting for ACK
	cmd_addr_client.sin_family = AF_INET;
	//struct timeval timeout = {2,0}; //set timeout for 2 seconds
	do{
		n = receive_unconnected(cmd_sock, NULL, NULL, &cmd_addr_client, 
				&flag);
		if (n < 0) {
			perror("errore in thread_recvfrom");
			exit(1);
		}
	} while (!(flag & ACK));

	//disattivo timer ack
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, 0);

	printf("Ack received\n\n");
	
	if (connect(cmd_sock, (struct sockaddr*) &cmd_addr_client, size) < 0){
		perror("error in connect");
		exit(EXIT_FAILURE);
	}
	printf("connected\n");
	

	//creating a new socket for data.
	data_addr_thread.sin_family = AF_INET;
	data_addr_thread.sin_addr.s_addr = htonl(INADDR_ANY); 
	data_addr_thread.sin_port = htons(my_port -> port_new_socket[1]);

	if ((data_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
		perror("errore in command sock");
		exit(EXIT_FAILURE);
	}

	/* assegna l'indirizzo al socket */
	if (bind(data_sock, (struct sockaddr *)&data_addr_thread,
				//sizeof(cmd_addr_thread)) < 0) {
				size) < 0) {
		perror("errore in thread_bind");
		exit(EXIT_FAILURE);
	}

	
	//reading for the new sock
	data_addr_client.sin_family = AF_INET;
	//struct timeval timeout = {2,0}; //set timeout for 2 seconds
	flag = 0;
	do{
		n = receive_unconnected(data_sock, NULL, NULL, &data_addr_client, 
				&flag);
		if (n < 0) {
			perror("errore in thread_recvfrom");
			exit(1);
		}
	} while(!(flag & ACK));

	if (connect(data_sock, (struct sockaddr*) &data_addr_client, size) < 0){
		perror("error in connect");
		exit(EXIT_FAILURE);
	}
	printf("connected\n");



	//waiting for command
	Message *cmd, ack;
	/*Message *cmd = (Message*) malloc(sizeof(Message));
	if (cmd ==NULL){
		perror("error malloc");
		exit(EXIT_FAILURE);
	}*/

	make_packet(&ack, NULL, 0, 0, ACK);
	while(1){

		printf("\nServer waitings for command\n");

		name_file = NULL;

		//n = receive_data(data_sock, cmd_sock, &name_file, &flag);

		//waiting for command mex:
		cmd = receive_packet(cmd_sock, NULL);
		flag = cmd -> flag;

		//stampa_mess(cmd);
		//sending ack
		ack.ack_num = cmd -> seq_num;
		send_packet(cmd_sock, &ack);
		
		switch(flag & (LIST | PUT | GET | FIN)){

			//list case
			case LIST:

				printf("list case received\n");
				server_list_operation(cmd_sock, data_sock);

				break;

			//get case
			case GET:

				printf("get case received\n");
				printf("Request of download for '%s' \n", cmd -> list_data);
				server_get_operation(cmd_sock, data_sock, cmd -> list_data);
				
				break;

			//put case file
			case PUT:

				printf("put case received\n");
				server_put_operation(cmd_sock, data_sock, 
						cmd -> list_data, semaphore);
				break;

			case FIN:

				printf("Exit case received\n");
				//invio ack 
				//
				//
				
				
				//rilascio delle risorse
		}

		//it's allocate in each PUT-GET-LIST request
	//	memset((void*)name_file, 1, strlen(name_file));
		free(name_file);
		free(cmd);

	}

	return NULL;
}


int main(int argc, char **argv) {
  
	struct sockaddr_in addr;
	int sockfd;
	pthread_t tid;
	int thread_count = 0; 
    int new_port_nums[2];
	int flag;
	int semaphore; //sempahore to check name files in put requests
	
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket */
		perror("errore in socket");
		exit(1);
	}
	

	//semaphore to manage put requests
	semaphore = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
	if (semaphore < 0){
		perror("error inizializing semaphore");
		exit(EXIT_FAILURE);
	}
	if (semctl(semaphore, 0, SETVAL, 1) < 0){
		perror("error inizializing sem value");
		exit(EXIT_FAILURE);	
	}

	//setting address
	memset((void *)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	addr.sin_port = htons(SERV_PORT); 

	/* assegna l'indirizzo al socket */
	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("errore in bind");
		exit(1);
	}

	int array_port[MAX_PORT_NUMBER];
	struct port_thread struct_port;

	while (1) {

		struct sockaddr_in *client_addr;
		client_addr = (struct sockaddr_in*)
						malloc(sizeof(struct sockaddr_in));

		if (client_addr == NULL){
			perror("error in malloc");
			exit(EXIT_FAILURE);
		}

		client_addr -> sin_family = AF_INET;

		//waiting for syn request
		if (receive_unconnected(sockfd, NULL, NULL, client_addr, &flag) < 0){
			perror("errore in recvfrom");
			exit(1);
		}
		/*	check sul flag	*/
		if ((flag & SYN) != SYN) continue;


		printf("syn received");

		generate_port(array_port, new_port_nums);

		//val = 0;
		struct_port.port_new_socket[0] = new_port_nums[0];
		struct_port.port_new_socket[1] = new_port_nums[1];
		printf("my new ports: %u, %u\n", struct_port.port_new_socket[0],
				struct_port.port_new_socket[1]);


		struct_port.semaphore = semaphore;
		struct_port.thread_index = thread_count;
		pthread_create(&tid, NULL, thread_function, (void *)&struct_port);
		thread_count ++;


		//6 bytes: num part <= 2^16 = 65536. 
		//then 2 num ports: 10, 1 space, numbers and 1 '\0' 
		unsigned char port_num_string[12];
		memset(port_num_string, 0, 12);
		sprintf(port_num_string, "%u %u", new_port_nums[0], new_port_nums[1]);
		printf("string num: %s\n", port_num_string);

		//sending syn-ack

		if (send_unconnected(sockfd, NULL, port_num_string, client_addr, SYN_ACK) < 0){
			perror("errore in sendto");
			exit(EXIT_FAILURE);
		}
		free(client_addr);
	}

	exit(0);
}
