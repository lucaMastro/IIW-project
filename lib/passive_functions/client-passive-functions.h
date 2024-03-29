
void send_syn(int sockfd, char *ip, int *new_ports){
	struct sockaddr_in server_listen_addr;
	Message syn, *syn_ack;
	char *tmp, data[12]; //12 is max size of syn-ack:
						 //num port <= 2^16 = 65536
						 //2*5 + 1 space + '\0'

	int cmd_port, data_port;

	//making server list address
	memset((void*) &server_listen_addr, 0, sizeof(server_listen_addr));
	server_listen_addr.sin_family = AF_INET;
	server_listen_addr.sin_port = (htons(SERV_PORT));
	if (inet_pton(AF_INET, ip, &server_listen_addr.sin_addr) <= 0){
		printf("error in inet_pton per %s\n", ip);
		exit(EXIT_FAILURE);
	}

	//connecting and sending syn
	connect(sockfd, (struct sockaddr*)&server_listen_addr,
			sizeof(server_listen_addr) );

	memset((void*)data, 0, 12);
	//last param of send_data isnt null, then if ack has length > 0, the 
	//data will be stored in that param
	send_data(sockfd, sockfd, NULL, SYN, data);

	//parsing the data for port numbers
	tmp = strtok(data, " ");
	cmd_port = atoi(tmp);

	tmp = strtok(NULL, " ");
	data_port = atoi(tmp);
	
	new_ports[0] = cmd_port;
	new_ports[1] = data_port;
}


void make_connection(int sockfd, char *ip, int *cmd_sock, int *data_sock){

	int new_ports[2];
	Message ack;
	struct sockaddr_in cmd_server_addr, data_server_addr;

	send_syn(sockfd, ip, new_ports);
	
	//making server thread addresses
	memset((void*)&cmd_server_addr, 0, sizeof(cmd_server_addr));
	cmd_server_addr.sin_family = AF_INET;
	cmd_server_addr.sin_port = htons(new_ports[0]);
	if (inet_pton(AF_INET, ip, &cmd_server_addr.sin_addr) <= 0){
		printf("error in inet_pton per %s\n", ip);
		exit(EXIT_FAILURE);
	}

	memset((void*)&data_server_addr, 0, sizeof(data_server_addr));
	data_server_addr.sin_family = AF_INET;
	data_server_addr.sin_port = htons(new_ports[1]);
	if (inet_pton(AF_INET, ip, &data_server_addr.sin_addr) <= 0){
		printf("error in inet_pton per %s\n", ip);
		exit(EXIT_FAILURE);
	}

	make_packet(&ack, NULL, 0, 0, ACK);

	//connect new sockets and sending ack on both
	if (connect_retry(*cmd_sock, &cmd_server_addr, 
				sizeof(cmd_server_addr), &ack) < 0){
		printf("connection refused on cmd sock.\n");
		exit(EXIT_FAILURE);
	}

	if (connect_retry(*data_sock, &data_server_addr, 
				sizeof(data_server_addr), &ack) < 0){
		printf("connection refused on data sock.\n");
		exit(EXIT_FAILURE);
	}

}


void show_man(int kind){
	//man for ls:
	//if kind = 1 --> ls
	//if kind = 2 --> get
	//if kind = 3 --> put
	
	switch(kind){
		case (0):
			printf("Usage of List Command\n");
			printf("=====================\n\n");

			printf("ftp > ls\n\n");
			break;
		case (1):
			printf("Usage of Get Command\n");
			printf("====================\n\n");

			printf("ftp > get <file_name_on_server> <file_name_in_local>\n");
			printf("take care: the destination folder is standard: client-file\n\n");
			break;
		case (2):
			printf("Usage of Put Command\n");
			printf("====================\n\n");

			printf("ftp > put <file_name_in_local> <file_name_on_server>\n");
			printf("take care: the source folder is standard: client-file\n\n");
			break;		
		case (3):
			printf("Usage of Exit Command\n");
			printf("=====================\n\n");

			printf("ftp > exit\n");
			break;		
		case(4):
			printf("Usage of Clear Command\n");
			printf("=====================\n\n");

			printf("ftp > clear\n\n");

	}
}
