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
int client_id = 0;

void display_initial_text() {
    printf("\n*********************************\n");
    printf("\nHello, Welcome to To Do List\n");
    printf("Select one option from the below list\n\n");
    printf("1. Add new task\n");
    printf("2. Remove task\n");
    printf("3. Modify task\n");
    printf("4. Quit\n\n");
}

void create_add_message_to_server(char *buf, struct message_add *message,
                                  int client_id) {
    sprintf(buf, "Client ID: %d\r\n"
                 "Message Type: %d\r\n"
                 "Task: %s\r\n"
                 "Task status: %d\r\n"
                 "Due date: %s\r\n\r\n",
            client_id, MSG_ADD, message->task, message->task_status,
            message->task_date);

    return;
}

void create_remove_message_to_server(char *buf, struct message_remove *message,
                                     int client_id) {
    sprintf(buf, "Client ID: %d\r\n"
                 "Message Type: %d\r\n"
                 "Key: %s\r\n\r\n",
            client_id, MSG_REMOVE, message->task);

    return;
}

void create_modify_message_to_server(char *buf, struct message_modify *message,
                                     int client_id) {
    sprintf(buf, "Client ID: %d\r\n"
                 "Message Type: %d\r\n"
                 "Flags: %d\r\n"
                 "Key: %s\r\n"
                 "New Task: %s\r\n"
                 "New Date: %s\r\n"
                 "New status: %d\r\n\r\n",
            client_id, MSG_MODIFY, message->mod_flags, message->task,
            message->new_task, message->new_date, message->new_task_status);

    return;
}

void get_inputs_for_message_add(struct message_add *message) {

    printf("\nAdding new Task\n");
    printf("\nEnter task: ");
    scanf("%" MAX_LENGTH_STR "s", message->task);
    printf("\nEnter due date: ");
    scanf("%" MAX_LENGTH_STR "s", message->task_date);

    message->task_status = TASK_NOT_DONE;

    return;
}

void get_inputs_for_message_remove(struct message_remove *message) {

    printf("\nRemoving Task\n");
    printf("\nEnter key: ");
    scanf("%" MAX_LENGTH_STR "s", message->task);

    return;
}

void get_inputs_for_message_modify(struct message_modify *message) {

    char temp[MAX_LENGTH];
    memset(temp, 0, MAX_LENGTH);

    printf("\nModifying Task\n");
    printf("\nEnter Task: ");
    scanf("%" MAX_LENGTH_STR "s", message->task);
    printf("\nEnter fields to modify\n");

    printf("\nEnter new Task name: ");
    scanf("%" MAX_LENGTH_STR "s", message->new_task);
    if (!strncmp(message->new_task, "NA", strlen("NA"))) {
    } else {
        message->mod_flags |= MOD_FLAGS_TASK_STRING_MODIFIED;
    }

    printf("\nEnter new Date: ");
    scanf("%" MAX_LENGTH_STR "s", message->new_date);
    if (!strncmp(message->new_date, "NA", strlen("NA"))) {
    } else {
        message->mod_flags |= MOD_FLAGS_DATE_MODIFIED;
    }

    printf("\nEnter new Task status: ");
    scanf("%" MAX_LENGTH_STR "s", temp);
    if (!strncmp(temp, "NA", strlen("NA"))) {
    } else {
        message->mod_flags |= MOD_FLAGS_STATUS_MODIFIED;
        if (!strncmp(temp, "DONE", strlen("DONE"))) {
            message->new_task_status = TASK_NOT_DONE;
        } else if (!strncmp(temp, "NOT_DONE", strlen("NOT_DONE"))) {
            message->new_task_status = TASK_DONE;
        }
    }
}

int send_and_get_response(char *buf, struct message_response *response) {
    int clientfd = 0;
    int status = 0;

    clientfd = connect_to_server(&server);
    if (clientfd < 0) {
        printf("connect failed: %s\n", strerror(errno));
        return -1;
    }

    status = write(clientfd, buf, MAX_LENGTH);
    if (status < 0) {
        printf("Write failed: %s\n", strerror(errno));
        close(clientfd);
        return -1;
    }

    get_response_from_server(clientfd, response);

    close(clientfd);

    status = parse_response_from_server(response, client_id);
    if (status < 0) {
        return -1;
    }

    return 0;
}

void print_task_details(char *task, char *date, enum t_status status,
                        struct message_response *response) {
    printf("\n*******************************************\n");
    printf("Task: %s\n", task);
    printf("Due date: %s\n", date);
    if (status == TASK_DONE) {
        printf("Task status: DONE\n");
    } else if (status == TASK_NOT_DONE) {
        printf("Task status: NOT DONE\n");
    }
    printf("Task Key: %lu\n", response->hash_key);
    printf("\n*******************************************\n");

    return;
}

void handle_new_task() {
    int status = 0;
    struct message_add message;
    struct message_response response;
    char buf[MAX_LENGTH];

    memset(&message, 0, sizeof(struct message_add));
    memset(&response, 0, sizeof(struct message_response));
    memset(buf, 0, MAX_LENGTH);

    get_inputs_for_message_add(&message);

    create_add_message_to_server(buf, &message, client_id);

    status = send_and_get_response(buf, &response);
    if (status < 0) {
        printf("\nTask not added successfully\n");
    } else {
        printf("\nTask added successfully\n");
        print_task_details(message.task, message.task_date, message.task_status,
                           &response);
    }

    return;
}

void handle_remove_task() {
    struct message_remove message;
    struct message_response response;
    int status = 0;
    char buf[MAX_LENGTH];

    memset(&message, 0, sizeof(struct message_remove));
    memset(&response, 0, sizeof(struct message_response));
    memset(buf, 0, MAX_LENGTH);

    get_inputs_for_message_remove(&message);

    create_remove_message_to_server(buf, &message, client_id);

    status = send_and_get_response(buf, &response);
    if (status < 0) {
        printf("\nTask not removed successfully\n");
    } else {
        printf("\nTask removed successfully\n");
    }

    return;
}

void handle_mod_task() {
    struct message_modify message;
    struct message_response response;
    int status = 0;
    char buf[MAX_LENGTH];

    memset(&message, 0, sizeof(struct message_remove));
    memset(&response, 0, sizeof(struct message_response));
    memset(buf, 0, MAX_LENGTH);

    get_inputs_for_message_modify(&message);
#if 0
	printf("Task %s\n", message.task);
	printf("New task %s\n", message.new_task);
	printf("New date %s\n", message.new_date);
	printf("New status %d\n", message.new_task_status);
	printf("Flags %x", message.mod_flags);
#endif

    create_modify_message_to_server(buf, &message, client_id);

    status = send_and_get_response(buf, &response);
    if (status < 0) {
        printf("\nTask modify failed\n");
    } else {
        printf("\nTask modified successfully\n");
    }

    return;
}

int validate_input_from_user(int choice) {

    if ((choice >= 1) && (choice <= 4)) {
        return 0;
    } else {
        return -1;
    }
}

int main(int argc, char *argv[]) {
    int choice = 0;
    int c = 0;
    char temp_choice;

    memset(&server, 0, sizeof(struct server_info));

    while ((c = getopt(argc, argv, "C:H:P:")) != -1) {
        switch (c) {
        case 'C':
            client_id = atoi(optarg);
            break;
        case 'H':
            memcpy(server.server_ip, optarg, strlen(optarg));
            break;
        case 'P':
            server.port = atoi(optarg);
            break;
        case '?':
            printf("\nPlease check arguments passed\n");
            break;
        }
    }

    printf("Server IP: %s\n", server.server_ip);
    printf("Server port: %d\n", server.port);

    while (1) {
        /* Display options */
        display_initial_text();

        /* Take inputs */
        printf("Choice: ");
        temp_choice = getchar();
        choice = (int)(temp_choice - 48);

        while ((temp_choice = getchar()) != '\n' && temp_choice != EOF)
            /* discard */;

        switch (choice) {
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
    }

    return 0;
}
