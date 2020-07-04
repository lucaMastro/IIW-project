
//thread local starages: so that operations on this variables  are atomic
__thread Sending_queue *queue;
__thread timer_t timer_id;

#ifdef ADAPT_TO
#include <math.h>
__thread struct itimerspec start_timer; //if timer is adaptive, it must be
										//reachable by retrasmission 
										//handler. it should be global, but
										//reentrant
#endif


int is_in_window(uint8_t ack_num){
	//this function checks if the ack_num is currentily in the trasmission
	//window. just check num by num from send_base if the numer is in the 
	//window.
	
	uint8_t base = queue -> send_base;
	int i;
	for (i = 0; i < SENDING_WINDOW; i++ ){ //sending_window times
		if ( (base + i) % (MAX_SEQ_NUM + 1) == ack_num)
			return 1;	//its in window
	}
	return 0; //not in window
}

void free_mex_and_update_base(uint8_t ack_num){
	//this function is called when a new ack is received. in that case, 
	//because of comulative ack, it can ack multiple packets. this function
	//start a cycle from send_base and free stored mex until 
	//stored_mex.seq_num == ack_num
	//it also update send_base, adding 1 for each acked mex.
	
	uint8_t old_base = queue -> send_base;
	uint8_t new_send_base = (ack_num + 1) % (MAX_SEQ_NUM + 1);
	int i;
	Message *to_ack;

	for (i = old_base; i != new_send_base; i = (i + 1) % (MAX_SEQ_NUM + 1)){
		to_ack = queue -> on_fly_message_queue[i];
		free(to_ack);
		to_ack = NULL;
		queue -> num_on_fly_pack--; //diminuisco numero pacchetti in volo
	}
	//in general, i can ack more packets with an ack: then the 
	//flying packets can be decremented of more than 1 for ack.
	//the differents beetwen the new send-base and the old send-base
	//is the number of packet i aim to ack
	queue -> send_base = new_send_base;
}


void initialize_struct(Sending_queue *queue){
	int i;

	queue -> send_base = 1;
	queue -> next_seq_num = 1;
	queue -> num_on_fly_pack = 0;
	queue -> should_read_data = 0;
	queue -> buf = NULL;

	//initializing queue 
	queue -> on_fly_message_queue = (Message **)
		malloc(sizeof(Message*) * MAX_SEQ_NUM + 1);
	if (queue -> on_fly_message_queue == NULL){
		perror("error_in_malloc");
		exit(EXIT_FAILURE);
	}

	//set all message* NULL
	for (i = 0; i < MAX_SEQ_NUM +1; i++){
		queue -> on_fly_message_queue[i] = NULL;	
	}
}



void retrasmission(Sending_queue *queue){
	//starting from send_base, this function will retrasmits as much messages
	//as queue -> num_on_fly_pack is.
	
	int i, on_fly = queue -> num_on_fly_pack;
	Message *m;
	int start_ind = queue -> send_base;
			
	for (i = start_ind; i < start_ind + on_fly; i++){
		m = queue -> on_fly_message_queue[i % (MAX_SEQ_NUM + 1)];
		if (m != NULL){		
			if (!is_packet_lost())
				send_packet(queue -> data_sock, m, NULL);
		}
		else
			break;
	}


#ifdef ADAPT_TO
	//rx, raddoppio timer, inutile ai fini di test: non c'è ritardo, ma 
	//solo perdita
	
/*	start_timer.it_value.tv_nsec = start_timer.it_value.tv_nsec << 1;
	if (start_timer.it_value.tv_nsec > 999999999){
		start_timer.it_value.tv_sec += 1; 
		start_timer.it_value.tv_nsec -= 1000000000; 
	}
	start_timer.it_value.tv_sec = start_timer.it_value.tv_sec << 1; 

	start_timer.it_interval.tv_sec = start_timer.it_interval.tv_sec;
	start_timer.it_interval.tv_nsec = start_timer.it_interval.tv_nsec;
	*/
	if (start_timer.it_value.tv_sec == 0 &&
			start_timer.it_value.tv_nsec < Tmin)
		start_timer.it_value.tv_nsec = Tmin;

/*			DEBUGGING PRINTF
 * 			printf("rx start timer:\n");
			printf("%lu\n%lu\n%lu\n%lu\n\n", 
					start_timer.it_value.tv_sec,
					start_timer.it_value.tv_nsec,
					start_timer.it_interval.tv_sec,
					start_timer.it_interval.tv_nsec	);*/
#else
	//initialize a new static timer only if not adaptive case
	struct itimerspec start_timer;
	start_timer.it_value.tv_sec = Tsec;
	start_timer.it_value.tv_nsec = Tnsec;
	start_timer.it_interval.tv_sec = 0;
	start_timer.it_interval.tv_nsec = 0;
#endif
	if (timer_settime(timer_id, 0, &start_timer, NULL) < 0){
		perror("error in starting timer in rx");
		exit(EXIT_FAILURE);
	}

}




void retrasmission_handler(int signo){
	retrasmission(queue);
}


void *waiting_for_ack(void *q){

	queue = (Sending_queue*) q;

	int sem = queue -> semaphore;
	int was_last = 0;
	Message *to_ack;

	struct sembuf sops;
	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = 1;

	Message *ack;

#ifdef ADAPT_TO

	//it needs this variable only in adaptive timer case
	struct itimerspec estimatedRTT;
	struct itimerspec sampleRTT;
	struct itimerspec oldRTT;
	struct itimerspec devRTT;
	struct itimerspec estimatedDEV;
	struct itimerspec sampleDEV;

	sampleDEV.it_value.tv_sec = 0;
	sampleDEV.it_value.tv_nsec = 0;

	devRTT.it_value.tv_sec = 0;
	devRTT.it_value.tv_nsec = 0;

	sampleDEV.it_value.tv_sec = 0;
	sampleDEV.it_value.tv_nsec = 0;
	
#endif
	
	//chosed SIGUSR1 as a signal of retrasmission
	signal(SIGUSR1, retrasmission_handler);  
	
	struct sigevent se;	
	se.sigev_notify = SIGEV_THREAD_ID;
	se._sigev_un._tid = syscall(SYS_gettid);
	se.sigev_signo = SIGUSR1;

#ifndef ADAPT_TO
	//declare a struct only if not adaptive case. in fact, if timer is 
	//adaptive, its a global thread local storage variable
	struct itimerspec start_timer;
#endif

	//initializing timer in both cases at costant value
	start_timer.it_value.tv_sec = Tsec;
	start_timer.it_value.tv_nsec = Tnsec;
	start_timer.it_interval.tv_sec = 0;
	start_timer.it_interval.tv_nsec = 0;

	struct itimerspec stop_timer;
	stop_timer.it_value.tv_sec = 0;
	stop_timer.it_value.tv_nsec = 0;
	stop_timer.it_interval.tv_sec = 0;
	stop_timer.it_interval.tv_nsec = 0;

	//creating timer
	if (timer_create(CLOCK_REALTIME, &se, &timer_id) < 0){
		perror("error in timer_create");
		exit(EXIT_FAILURE);
	}

	//starting timer
	if (timer_settime(timer_id, 0, &start_timer, NULL) < 0){
		perror("error in starting timer");
		exit(EXIT_FAILURE);
	}

	do{

		ack = receive_packet(queue -> cmd_sock, NULL);
		if (ack == NULL){
			perror("error receiving ack");
			exit(EXIT_FAILURE);
		}
		
		if (! (ack -> flag & ACK || ack -> flag & SYN_ACK) ) {
			if (ack -> flag & FIN){
				printf("[Error]: connection closed server-side\n");
				free(ack);
				exit(EXIT_FAILURE);
			}
			free(ack);
			continue;
		}

		if (!is_in_window(ack -> ack_num)) //acking only if new ack received
			continue;

		else{ //new ack

			//disattivo timer:
#ifndef ADAPT_TO
			if (timer_settime(timer_id, 0, &stop_timer, NULL) < 0){
				perror("error in stopping timer");
				exit(EXIT_FAILURE);
			}
#else
			//sampl rtt will contain the remaining time. 
			//i have to calculate the sample rtt with the difference from
			//the old rtt
			if (timer_gettime(timer_id, &sampleRTT) < 0){
				perror("error in stopping timer");
				exit(EXIT_FAILURE);
			}
			//old rtt will contain a copy of the previous timer
			if (timer_settime(timer_id, 0, &stop_timer, &oldRTT) < 0){
				perror("error in stopping timer");
				exit(EXIT_FAILURE);
			}

			//calculating just like tcp
			//each operation is done on .tv_sec and .tv_nsec field
			sampleRTT.it_value.tv_sec = oldRTT.it_value.tv_sec - 
				sampleRTT.it_value.tv_sec;
			sampleRTT.it_value.tv_nsec = oldRTT.it_value.tv_nsec - 
				sampleRTT.it_value.tv_nsec;

			estimatedRTT.it_value.tv_sec = (1 - a) * oldRTT.it_value.tv_sec;
			estimatedRTT.it_value.tv_nsec = (1 - a)*oldRTT.it_value.tv_nsec;
			estimatedRTT.it_value.tv_sec += a * sampleRTT.it_value.tv_sec;
			estimatedRTT.it_value.tv_nsec += a * sampleRTT.it_value.tv_nsec;

			sampleDEV.it_value.tv_sec = abs(
					sampleRTT.it_value.tv_sec - devRTT.it_value.tv_sec
					);
			sampleDEV.it_value.tv_nsec = abs(
					sampleRTT.it_value.tv_nsec - devRTT.it_value.tv_nsec
					);
			sampleDEV.it_value.tv_sec = b * sampleDEV.it_value.tv_sec;
			sampleDEV.it_value.tv_nsec = b * sampleDEV.it_value.tv_nsec;

			devRTT.it_value.tv_sec = (1 - b) * devRTT.it_value.tv_sec;
			devRTT.it_value.tv_nsec = (1 - b) * devRTT.it_value.tv_nsec;
			
			devRTT.it_value.tv_sec += b * sampleDEV.it_value.tv_sec;
			devRTT.it_value.tv_nsec += b * sampleDEV.it_value.tv_nsec;


			start_timer.it_value.tv_sec = estimatedRTT.it_value.tv_sec;
			start_timer.it_value.tv_sec += devRTT.it_value.tv_sec << 2; //*4
			
			start_timer.it_value.tv_nsec = estimatedRTT.it_value.tv_nsec;
			start_timer.it_value.tv_nsec += devRTT.it_value.tv_nsec << 2; 

			//moving nanosec to sec, if possible
			while (start_timer.it_value.tv_nsec > 999999999){
				start_timer.it_value.tv_sec += 1; 
				start_timer.it_value.tv_nsec -= 1000000000; 
			}
			
			
		/*	printf("normal start timer:\n");
			printf("%lu\n%lu\n%lu\n%lu\n\n", 
					start_timer.it_value.tv_sec,
					start_timer.it_value.tv_nsec,
					start_timer.it_interval.tv_sec,
					start_timer.it_interval.tv_nsec	);*/

			if (start_timer.it_value.tv_sec == 0 &&
				start_timer.it_value.tv_nsec < Tmin)
			start_timer.it_value.tv_nsec = Tmin;

#endif
			//getting flag of message to ack. check for ending while is done
			//on that flag. if it got the ack of the last packet, doesnt wait
			//anymore
			to_ack = queue -> on_fly_message_queue[ack -> ack_num];
			was_last = (to_ack -> flag & END_OF_DATA);

			free_mex_and_update_base( ack -> ack_num);

			//check if have to read data from ack: only in cmd mex
			if (queue -> should_read_data){
				queue -> buf = (char*) 
					malloc(sizeof(char) * (ack -> length + 1));

				if (queue -> buf == NULL){
					perror("error inizializing buf queue");
					exit(EXIT_FAILURE);
				}
				memset((void*)queue -> buf, 0, ack -> length + 1);

				memcpy(queue -> buf, ack -> data, ack -> length);
			}

			//unlock sender operation releasing a coin
			if (semop(sem, &sops, 1) < 0){
				perror("error unlocking semaphore");
				exit(EXIT_FAILURE);
			}

			//restarting timer
			if (timer_settime(timer_id, 0, &start_timer, NULL) < 0){
				perror("error in starting timer");
				exit(EXIT_FAILURE);
			}
		}

		free(ack);
	}
	while(was_last == 0); 

	//deleting timer and terminating thread execution
	timer_delete(timer_id);
	pthread_exit(NULL);
}
