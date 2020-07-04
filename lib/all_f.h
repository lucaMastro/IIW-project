/*
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
#include <sys/sem.h>*/
#include <stdio.h>

//active client
void client_put_operation(int cmd_sock, int data_sock, char *server_name, 
		char *local_name);
void client_get_operation(int cmd_sock, int data_sock, char *server_name, 
		char *local_name);
void client_list_operation(int cmd_sock, int data_sock);

//active server
int check_for_existing_file(int semaphore, char *path,
		FILE **to_create, char **new_file_name);
void server_put_operation(int cmd_sock, int data_sock, char *file_name, 
		int sem_id);
void server_get_operation(int cmd_sock, int data_sock, char *file_requested);
char *make_file_list();
void server_list_operation(int cmd_sock, int data_sock);

//common-passive.h
void stampa_mess(Message *mex);
int connect_retry(int sockfd, struct sockaddr_in *addr, socklen_t alen,
		Message *ack);

//client-passive.h
void send_syn(int sockfd, char *ip, int *new_ports);
void make_connection(int sockfd, char *ip, int *cmd_sock, int *data_sock);
//server-passive.h
void timer_handler(int signo);
void manage_connection_request(int cmd_sock, int data_sock, timer_t *conn_timer);

//reliable_conn
void initialize_struct(Sending_queue *queue);
void *waiting_for_ack(void *q);
int is_packet_lost(); //it is temporanly in read-write.h

//red-write
ssize_t writen(int fd, ssize_t n, struct sockaddr_in *to,
		unsigned char *buff);
void *make_packet(Message *mess_to_fill, void *read_data_from_here, 
		int seq_num, int ack_num, int flag_to_set);
void send_data(int connected_fd, int cmd_sock, void *data, int type, 
		char *read_from_ack);
void receive_data(int data_sock, int cmd_sock, void *write_here,
		int *save_here_flag);
void write_data(Message *mex, void *dest, unsigned char *src, int *str_len,
		int *old_str_len);
void send_packet(int sockfd, Message *mex, struct sockaddr_in *to);
int is_command_mex(Message *mex);
Message *receive_packet(int sockfd, struct sockaddr_in *from);
void check_last_ack(int cmd_sock, int data_sock, Message *ack);

//deserialize
unsigned char *deserialize_seq_num(Message *mex, unsigned char *buffer);
unsigned char *deserialize_ack_num(Message *mex, unsigned char *buffer);
unsigned char *deserialize_flag(Message *mex,unsigned char *buffer);
unsigned char *deserialize_length(Message *mex,unsigned char *buffer);
unsigned char *deserialize_header(Message *mex,unsigned char *buffer);
void deserialize_data(Message *mex, unsigned char *buffer);
Message *deserialize_message(unsigned char *buffer);

//serialize
unsigned char *serialize_seq_num(Message *mex, unsigned char *buffer);
unsigned char *serialize_ack_num(Message *mex, unsigned char *buffer);
unsigned char *serialize_flag(Message *mex,unsigned char *buffer);
unsigned char *serialize_length(Message *mex,unsigned char *buffer);
unsigned char *serialize_header(Message *mex,unsigned char *buffer);
unsigned char *serialize_message(Message *mex);
void serialize_data(Message *mex, unsigned char *buffer);


