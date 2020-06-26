#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_s_iface.h"
#include "util.h"

struct server_info server;
int interval_g = 0;
int client_id = 0;

int heartbeat_count_g = 0;
int heartbeat_received_g = 0;

char local_ip[MAX_LENGTH];

void create_heartbeat_message_to_server(char *buf) {
    sprintf(buf, "Client ID: %d\r\n"
                 "Request No: %d\r\n"
                 "Message Type: %d\r\n\r\n",
            client_id, heartbeat_count_g, MSG_HEARTBEAT);

    return;
}

void print_heartbeat_request_to_console() {

    printf("---------------------------------------\n");
    printf("|  Direction        | \t    Request    \n");
    printf("|  Request No       | \t    %d       \n", heartbeat_count_g);
    printf("|  Client ID        | \t    %d      \n", client_id);
    printf("|  Message type     | \t    MSG_HEARTBEAT  \n");
    printf("---------------------------------------\n");
}

void print_heartbeat_response_to_console(struct message_response *response) {

    printf("---------------------------------------\n");
    printf("|  Direction        | \t    Reply    \n");
    printf("|  Request No       | \t    %d       \n", response->req_no);
    printf("|  Client ID        | \t    %d      \n", response->client_id);
    printf("|  Status           | \t    %s      \n", response->status);
    printf("---------------------------------------\n");

    printf("\nHeatbeats received: %d/%d\n", heartbeat_received_g,
           heartbeat_count_g);
}

void *heartbeat_signal(void *vargp) {
    int clientfd = 0;
    int status = 0;
    int connection = 0;
    char buf[MAX_LENGTH];
    struct message_response response;

    memset(buf, 0, MAX_LENGTH);
    memset(&response, 0, sizeof(struct message_response));

    while (1) {
        sleep(interval_g);

        heartbeat_count_g++;
        clientfd = connect_to_server(&server);
        if (clientfd < 0) {
            printf("\nHeartbeat response not received: Server not active\n");
            printf("Heartbeat response received for %d out of %d requests\n",
                   heartbeat_received_g, heartbeat_count_g);
            printf("Trying again in %d sec... \n\n\n", interval_g);
            connection = 1;
            continue;
        }

        if (connection == 1) {
            connection = 0;
            printf("\nConnection to server restored\n\n");
        }

        create_heartbeat_message_to_server(buf);

        print_heartbeat_request_to_console();

        status = write(clientfd, buf, MAX_LENGTH);
        if (status < 0) {
            printf("Write failed: %s\n", strerror(errno));
        }

        get_response_from_server(clientfd, &response);

        status = parse_response_from_server(&response, client_id);
        if (status < 0) {
            printf("Heartbeat response not received: Server not active\n");
            printf("Heartbeat response received for %d out of %d requests\n",
                   heartbeat_received_g, heartbeat_count_g);
            printf("Trying again in %d sec... \n\n\n", interval_g);
            connection = 1;
        }

        heartbeat_received_g++;
        print_heartbeat_response_to_console(&response);

        close(clientfd);
    }

    return NULL;
}

void initialize_local_fault_detector(int heartbeat_interval, int port) {
    int status = 0;
    pthread_t tid;

    printf("Interval %d\n", heartbeat_interval);
    printf("Port %d\n", port);

    memset(&server, 0, sizeof(struct server_info));

    interval_g = heartbeat_interval;

    memcpy(server.server_ip, local_ip, 1024);
    server.port = port;

    status = pthread_create(&tid, NULL, heartbeat_signal, NULL);
    if (status < 0) {
        printf("Heartbeat thread create failed\n");
    }

    return;
}

int main(int argc, char *argv[]) {
    int port;
    int heartbeat_interval;
    char buf[MAX_LENGTH];

    memset(buf, 0, MAX_LENGTH);

    if (argc < 3) {
        printf("Usage: %s <port> <heartbeat interval>\n", argv[0]);
    }

    port = atoi(argv[2]);
    heartbeat_interval = atoi(argv[3]);
    memcpy(local_ip, argv[1], strlen(argv[1]));

    initialize_local_fault_detector(heartbeat_interval, port);

    printf("\nLocal fault detector started with %d interval\n",
           heartbeat_interval);

    while (1) {
        printf("\nEnter the new interval below to change it\n");
        printf("\nHeartbeat interval: ");

        fgets(buf, MAX_LENGTH, stdin);

        interval_g = atoi(buf);
        if (interval_g < 0) {
            printf("\nEnter a proper number\n");
            continue;
        }

        printf("Local fault detector changed to %d", interval_g);
    }
}
