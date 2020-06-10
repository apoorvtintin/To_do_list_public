#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "c_s_iface.h"
#include "util.h"

typedef struct sockaddr SA;

struct server_info {
	char server_ip[1024];
	int port;
};

struct server_info server;

void display_initial_text() {
	printf("\n*********************************\n");
	printf("\nHello, Welcome to To Do List\n");
	printf("Select one option from the below list\n\n");
	printf("1. Add new task\n");
	printf("2. Remove task\n");
	printf("3. Modify task\n");
	printf("4. Quit\n\n");
}

int connect_to_server() {
	struct sockaddr_in servaddr;
	int sockfd;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&servaddr, 0, sizeof(struct sockaddr_in));

	servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server.server_ip);
    servaddr.sin_port = htons(server.port);

	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
		return -1;
    }

	return sockfd;
}

void create_add_message_to_server(char *buf, struct message_add *message,
								int client_id) {
	sprintf(buf,
			"Client ID: %d\r\n"
			"Message Type: %d\r\n"
			"Task: %s\r\n"
			"Task status: %d\r\n"
			"Due date: %s\r\n\r\n",
			client_id, MSG_ADD, message->task, message->task_status, message->task_date);
}

void handle_new_task() {
	int clientfd = 0;
	struct message_add message;
	struct message_storage tot_msg;
	char buf[8192];
	char resp_buf[8192];

	memset(&message, 0, sizeof(struct message_add));
	memset(&tot_msg, 0, sizeof(struct message_storage));

	clientfd = connect_to_server();
	if (clientfd < 0) {
		return;
	}

	printf("\nAdding new Task\n");
	printf("\nEnter task: ");
	scanf("%s", message.task);
	printf("\nEnter due date: ");
	scanf("%s", message.task_date);

	message.task_status = TASK_NOT_DONE;

	create_add_message_to_server(buf, &message, 1);

	//write(clientfd, buf, 8192);

	while(sock_readline(clientfd, resp_buf, 8192)) {
		if(!strncmp(resp_buf, "\r\n", strlen("\r\n"))) {
			break;
		}
		
		printf("%s", resp_buf);
	}

	close(clientfd);

	printf("\nAdded task.\n");

	return;
}

void handle_remove_task() {
	return;
}

void handle_mod_task() {
	return;
}

void *heartbeat_signal(void *vargp) {
	int clientfd = 0;
	struct message_storage tot_msg;

	while(1) {
		sleep(10);

		clientfd = connect_to_server();
		if (clientfd < 0) {
			return NULL;
		}

		memset(&tot_msg, 0, sizeof(struct message_storage));

		tot_msg.header.client_id = 1;
		tot_msg.header.msg_type = MSG_HEARTBEAT;

		write(clientfd, &tot_msg, sizeof(struct message_storage));

		close(clientfd);
	}

	return NULL;
}

int main(int argc, char *argv[]) {
	int choice = 0;
	int status = 0;
	pthread_t tid;

	if (argc != 3) {
		printf("Check arguments again!!!!\n");
		exit(1);
	}

	// Spawn the heartbeat thread
	status = pthread_create(&tid, NULL, heartbeat_signal, NULL);
	if (status < 0) {
		printf("Heartbeat thread create failed\n");
	}

	memset(&server, 0, sizeof(struct server_info));

	memcpy(server.server_ip, argv[1], 1024);

	server.port = atoi(argv[2]);

	printf("Server IP: %s\n", server.server_ip);
	printf("Server port: %d\n",server.port);

	while (1) {
		/* Display options */
		display_initial_text();

		/* Take inputs */
		printf("Choice: ");
		scanf("%d", &choice);

		switch(choice) {
			case 1:
				handle_new_task();
				break;
			case 2:
				handle_remove_task();
				break;
			case 3:
				handle_mod_task();
				break;
			case 4:
				printf("\nGoodbye!!\n");
				exit(0);
				break;
			default:
				printf("\nEnter an option from the list\n");
				break;
		}

		/* Connect to server */

		/* */
	}

	return 0;
}
