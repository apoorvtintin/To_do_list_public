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

struct all_servers {
    int count;
    struct server_info server[3];
};

struct all_servers server_arr;

int client_id = 0;
int request_no_g = 0;
int response_no_g = 0;

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
    sprintf(buf, "Request No: %d\r\n"
                 "Client ID: %d\r\n"
                 "Message Type: %d\r\n"
                 "Task: %s\r\n"
                 "Task status: %d\r\n"
                 "Due date: %s\r\n\r\n",
            request_no_g, client_id, MSG_ADD, message->task,
            message->task_status, message->task_date);

    return;
}

void create_remove_message_to_server(char *buf, struct message_remove *message,
                                     int client_id) {
    sprintf(buf, "Request No: %d\r\n"
                 "Client ID: %d\r\n"
                 "Message Type: %d\r\n"
                 "Key: %s\r\n\r\n",
            request_no_g, client_id, MSG_REMOVE, message->task);

    return;
}

void create_modify_message_to_server(char *buf, struct message_modify *message,
                                     int client_id) {
    sprintf(buf, "Request No: %d\r\n"
                 "Client ID: %d\r\n"
                 "Message Type: %d\r\n"
                 "Flags: %d\r\n"
                 "Key: %s\r\n"
                 "New Task: %s\r\n"
                 "New Date: %s\r\n"
                 "New status: %d\r\n\r\n",
            request_no_g, client_id, MSG_MODIFY, message->mod_flags,
            message->task, message->new_task, message->new_date,
            message->new_task_status);

    return;
}

void get_inputs_for_message_add(struct message_add *message) {

    printf("\nAdding new Task\n");
    printf("\nEnter task: ");
    scanf("%" MAX_LENGTH_STR "[^\n]%*c", message->task);
    printf("\nEnter due date: ");
    scanf("%" MAX_LENGTH_STR "[^\n]%*c", message->task_date);

    message->task_status = TASK_NOT_DONE;

    return;
}

void get_inputs_for_message_remove(struct message_remove *message) {

    printf("\nRemoving Task\n");
    printf("\nEnter key: ");
    scanf("%" MAX_LENGTH_STR "[^\n]%*c", message->task);

    return;
}

void get_inputs_for_message_modify(struct message_modify *message) {

    char temp[MAX_LENGTH];
    memset(temp, 0, MAX_LENGTH);

    printf("\nModifying Task\n");
    printf("\nEnter Key: ");
    scanf("%" MAX_LENGTH_STR "[^\n]%*c", message->task);
    printf("\nEnter fields to modify\n");

    printf("\nEnter new Task name: ");
    scanf("%" MAX_LENGTH_STR "[^\n]%*c", message->new_task);
    if (!strncmp(message->new_task, "NA", strlen("NA"))) {
    } else {
        message->mod_flags |= MOD_FLAGS_TASK_STRING_MODIFIED;
    }

    printf("\nEnter new Date: ");
    scanf("%" MAX_LENGTH_STR "[^\n]%*c", message->new_date);
    if (!strncmp(message->new_date, "NA", strlen("NA"))) {
    } else {
        message->mod_flags |= MOD_FLAGS_DATE_MODIFIED;
    }

    printf("\nEnter new Task status: ");
    scanf("%" MAX_LENGTH_STR "[^\n]%*c", temp);
    if (!strncmp(temp, "NA", strlen("NA"))) {
    } else {
        message->mod_flags |= MOD_FLAGS_STATUS_MODIFIED;
        if (!strncmp(temp, "DONE", strlen("DONE"))) {
            message->new_task_status = TASK_DONE;
        } else if (!strncmp(temp, "NOT_DONE", strlen("NOT_DONE"))) {
            message->new_task_status = TASK_NOT_DONE;
        }
    }
}

int send_and_get_response(char *buf, struct message_response *response) {
    int clientfd = 0;
    int status = 0;
    int success = 0;
    int i = 0;

    for (i = 0; i < server_arr.count; i++) {

        clientfd = connect_to_server(&server_arr.server[i]);
        if (clientfd < 0) {
            // printf("connect failed: %s\n", strerror(errno));
            continue;
        }

        status = write(clientfd, buf, MAX_LENGTH);
        if (status < 0) {
            printf("Write failed: %s\n", strerror(errno));
            close(clientfd);
            continue;
        }

        get_response_from_server(clientfd, response);

        close(clientfd);

        if (response->req_no == response_no_g) {
            printf("\nServer %d response, but reponse already recieved for "
                   "request no %d\n",
                   i + 1, response_no_g);
            continue;
        }

        response_no_g++;

        status = parse_response_from_server(response, client_id);
        if (status < 0) {
            continue;
        }

        success = 1;
    }

    if (success == 1) {
        return 0;
    } else {
        return -1;
    }
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

void print_add_request_to_console(struct message_add *message) {

    printf("---------------------------------------\n");
    printf("|  Direction        | \t    Request    \n");
    printf("|  Request No       | \t    %d       \n", request_no_g);
    printf("|  Client ID        | \t    %d      \n", client_id);
    printf("|  Message type     | \t    MSG_ADD  \n");
    printf("|  Task             | \t    %s \n", message->task);
    printf("|  Due date         | \t    %s \n", message->task_date);
    printf("|  Status           | \t    %s \n",
           get_task_status_str(message->task_status));
    printf("---------------------------------------\n");
}

void print_remove_request_to_console(struct message_remove *message) {

    printf("---------------------------------------\n");
    printf("|  Direction        | \t    Request    \n");
    printf("|  Request No       | \t    %d       \n", request_no_g);
    printf("|  Client ID        | \t    %d      \n", client_id);
    printf("|  Message type     | \t    MSG_REMOVE  \n");
    printf("|  Key              | \t    %s \n", message->task);
    printf("---------------------------------------\n");
}

void print_modify_request_to_console(struct message_modify *message) {

    printf("---------------------------------------\n");
    printf("|  Direction        | \t    Request    \n");
    printf("|  Request No       | \t    %d       \n", request_no_g);
    printf("|  Client ID        | \t    %d      \n", client_id);
    printf("|  Message type     | \t    MSG_MODIFY  \n");
    printf("|  Key              | \t    %s \n", message->task);
    printf("|  New Task         | \t    %s \n", message->new_task);
    printf("|  New date         | \t    %s \n", message->new_date);
    printf("|  New status       | \t    %s \n",
           get_task_status_str(message->new_task_status));

    printf("---------------------------------------\n");
}

void print_add_response_to_console(struct message_response *response) {

    printf("---------------------------------------\n");
    printf("|  Direction        | \t    Reply    \n");
    printf("|  Request No       | \t    %d       \n", response->req_no);
    printf("|  Client ID        | \t    %d      \n", response->client_id);
    printf("|  Status           | \t    %s      \n", response->status);
    printf("|  Key              | \t    %lu      \n", response->hash_key);
    printf("---------------------------------------\n");
}

void print_remove_response_to_console(struct message_response *response) {

    printf("---------------------------------------\n");
    printf("|  Direction        | \t    Reply    \n");
    printf("|  Request No       | \t    %d       \n", response->req_no);
    printf("|  Client ID        | \t    %d      \n", response->client_id);
    printf("|  Status           | \t    %s      \n", response->status);
    printf("---------------------------------------\n");
}

void print_modify_response_to_console(struct message_response *response) {

    printf("---------------------------------------\n");
    printf("|  Direction        | \t    Reply    \n");
    printf("|  Request No       | \t    %d       \n", response->req_no);
    printf("|  Client ID        | \t    %d      \n", response->client_id);
    printf("|  Status           | \t    %s      \n", response->status);
    printf("---------------------------------------\n");
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

    request_no_g++;
    create_add_message_to_server(buf, &message, client_id);

    print_add_request_to_console(&message);

    status = send_and_get_response(buf, &response);
    if (status < 0) {
        printf("\nTask not added successfully\n");
    } else {
        printf("\nTask added successfully\n");
        // print_task_details(message.task, message.task_date,
        // message.task_status,
        //                   &response);
    }

    print_add_response_to_console(&response);

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

    request_no_g++;
    get_inputs_for_message_remove(&message);

    print_remove_request_to_console(&message);

    create_remove_message_to_server(buf, &message, client_id);

    status = send_and_get_response(buf, &response);
    if (status < 0) {
        printf("\nTask not removed successfully\n");
    } else {
        printf("\nTask removed successfully\n");
    }

    print_remove_response_to_console(&response);

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

    request_no_g++;
    get_inputs_for_message_modify(&message);
#if 0
	printf("Task %s\n", message.task);
	printf("New task %s\n", message.new_task);
	printf("New date %s\n", message.new_date);
	printf("New status %d\n", message.new_task_status);
	printf("Flags %x", message.mod_flags);
#endif

    print_modify_request_to_console(&message);

    create_modify_message_to_server(buf, &message, client_id);

    status = send_and_get_response(buf, &response);
    if (status < 0) {
        printf("\nTask modify failed\n");
    } else {
        printf("\nTask modified successfully\n");
    }

    print_modify_response_to_console(&response);

    return;
}

int validate_input_from_user(int choice) {

    if ((choice >= 1) && (choice <= 4)) {
        return 0;
    } else {
        return -1;
    }
}

void parse_and_prepare_server_array(char *file) {
    FILE *fptr;
    char line[1024];
    char temp[1024];
    int i = 0;

    memset(line, 0, 1024);
    memset(temp, 0, 1024);

    fptr = fopen(file, "r");

    while (fgets(line, 1024, fptr)) {
        if (!strncmp(line, "###", strlen("###"))) {
            i = 0;
            continue;
        }

        if (!strncmp(line, "##", strlen("##"))) {
            i++;
            memset(line, 0, 1024);
            memset(temp, 0, 1024);
            server_arr.count = i + 1;
            continue;
        }

        if (!strncmp(line, "Server", strlen("Server"))) {
            sscanf(line, "Server: %s", temp);
            memcpy(server_arr.server[i].server_ip, temp, strlen(temp));
        }

        if (!strncmp(line, "Port", strlen("Port"))) {
            sscanf(line, "Port: %s", temp);
            server_arr.server[i].port = atoi(temp);
        }
    }

    fclose(fptr);
}

int parse_input_arguments(int argc, char *argv[]) {
    int c = 0;
    char filename[1024];

    memset(filename, 0, 1024);

    while ((c = getopt(argc, argv, "C:F:")) != -1) {
        switch (c) {
        case 'C':
            client_id = atoi(optarg);
            break;
        case 'F':
            memcpy(filename, optarg, strlen(optarg));
            parse_and_prepare_server_array(filename);
            break;
        case '?':
            printf("\nPlease check arguments passed\n");
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int choice = 0;
    int status = 0;
    char temp_choice;

    memset(&server_arr, 0, sizeof(struct all_servers));

    status = parse_input_arguments(argc, argv);
    if (status < 0) {
        exit(0);
    }

    printf("Server count: %d\n", server_arr.count);
    printf("Server 1 IP: %s\n", server_arr.server[0].server_ip);
    printf("Server 1 port: %d\n", server_arr.server[0].port);
    printf("Server 2 IP: %s\n", server_arr.server[1].server_ip);
    printf("Server 2 port: %d\n", server_arr.server[1].port);
    printf("Server 3 IP: %s\n", server_arr.server[2].server_ip);
    printf("Server 3 port: %d\n", server_arr.server[2].port);

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
