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

char local_ip[MAX_LENGTH] = "127.0.0.1";

void create_heartbeat_message_to_server(char *buf) {
    sprintf(buf, "Client ID: %d\r\n"
                 "Message Type: %d\r\n\r\n",
            client_id, MSG_HEARTBEAT);

    return;
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

        clientfd = connect_to_server(&server);
        if (clientfd < 0) {
            printf("\nHeartbeat response not received: Server not active\n");
            printf("Trying again in %d sec... \n\n\n", interval_g);
            connection = 1;
            continue;
        }

        if (connection == 1) {
            connection = 0;
            printf("\nConnection to server restored\n\n");
        }

        create_heartbeat_message_to_server(buf);

        status = write(clientfd, buf, MAX_LENGTH);
        if (status < 0) {
            printf("Write failed: %s\n", strerror(errno));
        }

        get_response_from_server(clientfd, &response);

        status = parse_response_from_server(&response, client_id);
        if (status < 0) {
            printf("Heartbeat response not received: Server not active\n");
            printf("Trying again in %d sec... \n\n\n", interval_g);
            connection = 1;
        }

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

    port = atoi(argv[1]);
    heartbeat_interval = atoi(argv[2]);

    initialize_local_fault_detector(heartbeat_interval, port);

    printf("\nLocal fault detector started with %d interval\n", heartbeat_interval);

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
