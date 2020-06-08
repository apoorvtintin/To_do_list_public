#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include "c_s_iface.h"

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

void handle_new_task() {
	int clientfd = 0;
	struct message_add message;
	struct message_storage tot_msg;

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

	tot_msg.header.client_id = 1;
	tot_msg.header.msg_type = MSG_ADD;
	
	memcpy(tot_msg.task, &message, sizeof(struct message_add));

	write(clientfd, &tot_msg, sizeof(struct message_storage));

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

int main(int argc, char *argv[]) {
	int choice = 0;

	if (argc != 3) {
		printf("Check arguments again!!!!\n");
		exit(1);
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
