#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h> 
//#include "../../structs/message_struct.h"
//#include "../../structs/sending_queue.h"


__thread Sending_queue *queue;
__thread timer_t timer_id;



void initialize_struct(Sending_queue *queue){
	int i;

	queue -> send_base = 1;
	queue -> next_seq_num = 1;
	queue -> all_acked = 1;

	queue -> on_fly_message_queue = (Message **)
		malloc(sizeof(Message*) * RECEIVE_WINDOW);
	if (queue -> on_fly_message_queue == NULL){
		perror("error_in_malloc");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < RECEIVE_WINDOW; i++){
		queue -> on_fly_message_queue[i] = NULL;	
	}

}



void retrasmission(Sending_queue *queue){
	//ciclo da send base e ritrasmetto i mess:
	int i;
	Message *m;
	int start_ind = queue -> send_base % RECEIVE_WINDOW;

	for (i = start_ind; i < start_ind + RECEIVE_WINDOW; i++){
		m = queue -> on_fly_message_queue[i];
		if ( m != NULL)
			send_packet(queue -> data_sock, m);
		
		else
			break;
	}

	struct itimerspec restart_timer;
	restart_timer.it_value.tv_sec = Tsec;
	restart_timer.it_value.tv_nsec = Tnsec;
	restart_timer.it_interval.tv_sec = 0;
	restart_timer.it_interval.tv_nsec = 0;

	if (timer_settime(timer_id, 0, &restart_timer, NULL) < 0){
		perror("error in starting timer in rx");
		exit(EXIT_FAILURE);
	}

}




void retrasmission_handler(int signo){
	retrasmission(queue);
}


void *waiting_for_ack(void *q){
	//Sending_queue *queue = (Sending_queue*) q;
	queue = (Sending_queue*) q;

	int sem = queue -> semaphore;
	int flag = 0;
	int acked_seq_num = -1;
	int freeding_pos;
	int was_last;
	Message *to_ack;

	struct sembuf sops;
	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = 1;

	Message *ack;

	signal(SIGALRM, retrasmission_handler);  
	
	struct sigevent se;	
	se.sigev_notify = SIGEV_THREAD_ID;
	se._sigev_un._tid = syscall(SYS_gettid);
	se.sigev_signo = SIGALRM;

	
	struct itimerspec start_timer, stop_timer;

	start_timer.it_value.tv_sec = Tsec;
	start_timer.it_value.tv_nsec = Tnsec;
	start_timer.it_interval.tv_sec = 0;
	start_timer.it_interval.tv_nsec = 0;

	stop_timer.it_value.tv_sec = 0;
	stop_timer.it_value.tv_nsec = 0;
	stop_timer.it_interval.tv_sec = 0;
	stop_timer.it_interval.tv_nsec = 0;

//	struct itimerspec timer;

	if (timer_create(CLOCK_REALTIME, &se, &timer_id) < 0){
		perror("error in timer_create");
		exit(EXIT_FAILURE);
	}

	if (timer_settime(timer_id, 0, &start_timer, NULL) < 0){
		perror("error in starting timer");
		exit(EXIT_FAILURE);
	}

	do{

		ack = receive_packet(queue -> cmd_sock, NULL);
		if (! (ack -> flag & ACK) ) {
			free(ack);
			continue;
		}
	
		//ack -> ack num is the seq_num of the packet
		//i have to ack. the first packet has seq_num == 1,

		if ( ack -> ack_num < queue -> send_base)continue; //ack_duplicato
		else{ //nuovo ack
			//disattivo timer:
			if (timer_settime(timer_id, 0, &stop_timer, NULL) < 0){
				perror("error in stopping timer");
				exit(EXIT_FAILURE);
			}
			freeding_pos = ack -> ack_num % RECEIVE_WINDOW;
			to_ack = queue -> on_fly_message_queue[freeding_pos];
			
			was_last = (to_ack -> flag & END_OF_DATA);
			free(to_ack);
			to_ack = NULL;

			queue -> send_base = ack -> ack_num + 1;

			//sblocco invio
			if (semop(sem, &sops, 1) < 0){
				perror("error unlocking semaphore");
				exit(EXIT_FAILURE);
			}
			if (timer_settime(timer_id, 0, &start_timer, NULL) < 0){
				perror("error in starting timer");
				exit(EXIT_FAILURE);
			}
		}

		free(ack);
		//ack = NULL;

	}while(was_last == 0); //ho ricevuto ack dell'ultimo

	pthread_exit(NULL);
}
