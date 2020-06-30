void timer_handler(int signo){
	printf("Timed out\n");
}

void manage_connection_request(int cmd_sock, int data_sock){
	//waiting for acks:
	struct sockaddr_in cmd_addr_client, data_addr_client;
	Message *mex;
	struct sigevent se;
	struct itimerspec start_timer, stop_timer;
	timer_t conn_timer;

	signal(SIGALRM, timer_handler);
	//timer:
	se.sigev_notify = SIGEV_THREAD_ID;
	se._sigev_un._tid = syscall(SYS_gettid);
	se.sigev_signo = SIGALRM;

	start_timer.it_value.tv_sec = 5;
	start_timer.it_value.tv_nsec = 0;
	start_timer.it_interval.tv_sec = 0;
	start_timer.it_interval.tv_nsec = 0;
	
	stop_timer.it_value.tv_sec = 0;
	stop_timer.it_value.tv_nsec = 0;
	stop_timer.it_interval.tv_sec = 0;
	stop_timer.it_interval.tv_nsec = 0;

	if (timer_create(CLOCK_REALTIME, &se, &conn_timer) < 0){
		perror("error creating conn_timer");
		exit(EXIT_FAILURE);
	}
	
	cmd_addr_client.sin_family = AF_INET; 
	data_addr_client.sin_family = AF_INET; 

	//start timer
	if (timer_settime(conn_timer, 0, &start_timer, NULL) < 0){
		perror("error starting conn_timer");
		exit(EXIT_FAILURE);
	}
	do{
		mex = receive_packet(cmd_sock, &cmd_addr_client);
	} while( !(mex -> flag & ACK));

	//stop_timer
	if (timer_settime(conn_timer, 0, &stop_timer, NULL) < 0){
		perror("error stopping conn_timer");
		exit(EXIT_FAILURE);
	}

	connect_retry(cmd_sock, &cmd_addr_client, 
			sizeof(cmd_addr_client), NULL);

	//start timer
	if (timer_settime(conn_timer, 0, &start_timer, NULL) < 0){
		perror("error starting conn_timer");
		exit(EXIT_FAILURE);
	}

	do{
		mex = receive_packet(data_sock, &data_addr_client);
	} while( !(mex -> flag & ACK));

	//stop_timer
	if (timer_settime(conn_timer, 0, &stop_timer, NULL) < 0){
		perror("error stopping conn_timer");
		exit(EXIT_FAILURE);
	}

	connect_retry(data_sock, &data_addr_client, 
			sizeof(data_addr_client), NULL);


	//deleting timer
	timer_delete(conn_timer);

}
