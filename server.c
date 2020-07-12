#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
 #include <signal.h>

#include "c_s_iface.h"
#include "local_f_detector.h"
#include "server.h"
#include "storage.h"
#include "util.h"
#include "log.h"
#include "worker.h"

// Global variables
int verbose = 0;
int is_primary = 0;
server_log_t svr_log;
bsvr_ctx bckup_svr[2];
int server_id;
uint64_t chk_point_num;
int msg_count = 0;

static pthread_mutex_t storage_lock;

// Local help functions

void print_user_req(client_ctx_t *client_ctx, char *dir) {
    client_request_t *msg_ptr = &client_ctx->req;
    static int once = 0;
    if (once == 0) {
        char buffer[4096];
        int ch = snprintf(
            buffer, sizeof(buffer), "%-2s %3.3s%1s %7s%1s %7s%1s %-14s%1s "
                                    "%-24s%1s %-30s%1s %-10s%1s "
                                    "%-15s%1s %-4s%2s\n",
            "|", "DIR", "", "ClntID", "", "Seq No", "", "MSG Type", "", "Hash",
            "", "TASK", "", "Date", "", "Task Status", "", "Mod Flags", "|");
        ch = ch - 2;
        printf("%s", buffer);
        memset(buffer, 0, sizeof(buffer));
        memset(buffer, '=', ch);
        buffer[0] = '+';
        buffer[ch] = '+';
        printf("%s\n", buffer);
        once = 1;
        fflush(stdout);
    }
    char buffer[4096];

    int ch = snprintf(buffer, sizeof(buffer),
                      "%-2s %3.3s%1s %7d%2s %7lu%1s %-14s%2s %-24lu%2s "
                      "%-30s%2s %-10s%2s %-15s%2s %-8d%3s\n",
                      "|", dir, "", client_ctx->client_id, "", msg_ptr->req_no,
                      "", get_msg_type_str(msg_ptr->msg_type), "",
                      msg_ptr->hash_key, "", msg_ptr->task, "", msg_ptr->date,
                      "", get_task_status_str(msg_ptr->task_status), "",
                      msg_ptr->mod_flags, "|");
    printf("%s", buffer);
    memset(buffer, 0, sizeof(buffer));
    ch = ch - 2;
    if (!strcmp(dir, "Res"))
        memset(buffer, '=', ch);
    else
        memset(buffer, '-', ch);
    buffer[0] = '+';
    buffer[ch] = '+';
    printf("%s\n", buffer);
    fflush(stdout);
}

void write_client_responce(client_ctx_t *client_ctx, char *status, char *msg) {
    char resp_buf[MAX_LENGTH];
    int resp_len;
    if (client_ctx->req.msg_type == MSG_ADD) {
        if (client_ctx->req.hash_key == 0) {
            printf("ERROR: HASHKEY NULL\n");
        }
        resp_len =
            snprintf(resp_buf, sizeof(resp_buf), "Status: %s\r\n"
                                                 "Client ID: %d\r\n"
                                                 "Request No: %lu\r\n"
                                                 "Msg: %s\r\n"
                                                 "Key: %lu\r\n"
                                                 "\r\n",
                     status, client_ctx->client_id, client_ctx->req.req_no, msg,
                     client_ctx->req.hash_key);
    } else {
        resp_len = snprintf(resp_buf, sizeof(resp_buf), "Status: %s\r\n"
                                                        "Client ID: %d\r\n"
                                                        "Request No: %lu\r\n"
                                                        "Msg: %s\r\n"
                                                        "\r\n",
                            status, client_ctx->client_id,
                            client_ctx->req.req_no, msg);
    }
    if (resp_len > sizeof(resp_buf)) {
        fprintf(stderr, "server resp is greater than the resp_buffer!!");
        return;
    }
    if (sock_writen(client_ctx->fd, resp_buf, resp_len) == -1) {
        fprintf(stderr, "Error while writing responce to client\n");
    }
    return;
}

int parse_kv(client_ctx_t *client_ctx, char *key, char *value) {

    if (strcmp(key, "Client ID") == 0) {
        if (str_to_int(value, &client_ctx->client_id) != 0) {
            fprintf(stderr, "Client_id conversion failed; Malformed req!!!\n");
            write_client_responce(client_ctx, "FAIL",
                                  "Malformed Client ID received");
            return -1;
        }
    } else if (strcmp(key, "Message Type") == 0) {
        if (str_to_int(value, (int *)&client_ctx->req.msg_type) != 0) {
            write_client_responce(client_ctx, "FAIL",
                                  "Malformed Msg Type received");
            fprintf(stderr, "Msg Id Conversion failed!!!\n");
            return -1;
        }
    } else if (strcmp(key, "Task") == 0) {
        strncpy(client_ctx->req.task, value, sizeof(client_ctx->req.task));
        client_ctx->req.task_len = strlen(client_ctx->req.task);
    } else if (strcmp(key, "Task status") == 0) {
        if (str_to_int(value, (int *)&client_ctx->req.task_status) != 0) {
            write_client_responce(client_ctx, "FAIL", "Malformed Task Status");
            fprintf(stderr, "Msg Id Conversion failed!!!\n");
            return -1;
        }
    } else if (strcmp(key, "Due date") == 0) {
        strncpy(client_ctx->req.date, value, sizeof(client_ctx->req.date));
    } else if (strcmp(key, "Key") == 0) {
        client_ctx->req.hash_key = strtoul(value, NULL, 10);
        printf("Key: %lu\n", client_ctx->req.hash_key);
    } else if (strcmp(key, "New Task") == 0) {
        strncpy(client_ctx->req.task, value, sizeof(client_ctx->req.task));
        client_ctx->req.task_len = strlen(client_ctx->req.task);
    } else if (strcmp(key, "New Date") == 0) {
        strncpy(client_ctx->req.date, value, sizeof(client_ctx->req.date));
    } else if (strcmp(key, "New status") == 0) {
        if (str_to_int(value, (int *)&client_ctx->req.task_status) != 0) {
            write_client_responce(client_ctx, "FAIL", "Malformed Task Status");
            fprintf(stderr, "Msg Id Conversion failed!!!\n");
            return -1;
        }
    } else if (strcmp(key, "Request No") == 0) {
        if (str_to_int(value, (int *)&client_ctx->req.req_no) != 0) {
            write_client_responce(client_ctx, "FAIL", "Malformed Req No.");
            fprintf(stderr, "Req No Conversion failed!!!\n");
            return -1;
        }
    } else if(strcmp(key, "Data Length") == 0)
    {
        if (str_to_int(value, (int *)&client_ctx->req.payload.size) != 0) {
            write_client_responce(client_ctx, "FAIL", "Malformed Data Length.");
            fprintf(stderr, "Data Length Conversion failed!!!\n");
            return -1;
        }
        
    }

    return 0;
}

int enqueue_client_req(server_log_t *svr, client_ctx_t *client_ctx) {
    log_node_t *node = malloc(sizeof(log_node_t));
    node->val = client_ctx;
    log_msg_type m_type =
        (client_ctx->req.msg_type == MSG_HEARTBEAT) ? CONTROL : NORMAL;
    if(client_ctx->req.msg_type == MSG_CHK_PT)
    {
        // if checkpoint enquequ into both control and normal queues
        log_node_t *node1 = malloc(sizeof(log_node_t));
        node1->val = client_ctx;
        fprintf(stderr, "enqueue into ctrl queue\n");
        enqueue(svr, node1, CONTROL);
    }
    return enqueue(svr, node, m_type);
}
void *handle_connection(void *arg) {
    pthread_detach(pthread_self());
    client_ctx_t *client_ctx = arg;
    int msg_len = 0;
    char msg_buf[MAXMSGSIZE]; // define a MAXLINE macro.
    char key[MAXMSGSIZE], value[MAXMSGSIZE];
    unsigned int input_fields_counter = 0;
    sock_buf_read client_fd;
    init_buf_fd(&client_fd, client_ctx->fd);
    memset(msg_buf, 0, MAXMSGSIZE);
    memset(key, 0, MAXMSGSIZE);
    memset(value, 0, MAXMSGSIZE);

    while (1) {
        msg_len = sock_readline(&client_fd, msg_buf, MAXMSGSIZE);
        if (msg_len == 0) {
            // The client closed the connection we should to.
            fprintf(stderr, "Client close connection!!\n");
            goto _EXIT;
        }

        if (msg_len == 2 && msg_buf[0] == '\r' && msg_buf[1] == '\n') {
            // End of message.
            break;
        }
        if (input_fields_counter >= MAX_REQ_FIELDS) {
            fprintf(stderr, "Client sent too many input fields \n");
            write_client_responce(client_ctx, "FAIL",
                                  "req had more than req num of fields");
            goto _EXIT;
        }
        if (sscanf(msg_buf, "%[^:]: %[^\r\n]", key, value) != 2) {
            fprintf(stderr, "Malformed key value received in request!!!\n");
            write_client_responce(client_ctx, "FAIL",
                                  "Malformed Key-Value received in input");
            goto _EXIT;
        }
        if (parse_kv(client_ctx, key, value) == -1) {
            goto _EXIT;
        }
        ++input_fields_counter;
    }
    client_ctx->req.payload.data = malloc(client_ctx->req.payload.size);
    if(client_ctx->req.payload.data < 0)
    {
        fprintf(stderr, "run out of mem !!!\n");
        goto _EXIT;
    }
    if(client_ctx->req.payload.size != 0)
    {
        int n = 0; 
        int toread = client_ctx->req.payload.size;
        int nread = 0;
        while((n = sock_readline(&client_fd, msg_buf, MAXMSGSIZE)) > 0)
        {
             memcpy(client_ctx->req.payload.data + nread, msg_buf, 
                     (toread < n ? toread: n));
             nread += n;
             toread -= n;     
             if(toread == 0)
               break;  
        }
    }
    if(client_ctx->req.payload.size != 0)
    {
        char filename[64];
        snprintf(filename, sizeof(filename), "tmp-%d", (rand()*server_id));
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU  | S_IRUSR  
                | S_IWUSR 
                | S_IXUSR | S_IRWXG | S_IRGRP | S_IWGRP | S_IXGRP | S_IRWXO 
                | S_IROTH | S_IWOTH | S_IXOTH);
        write(fd, client_ctx->req.payload.data, client_ctx->req.payload.size);
        close(fd);
        memset(client_ctx->req.filename,0, sizeof(client_ctx->req.filename));
        strncpy(client_ctx->req.filename, filename, 
                sizeof(client_ctx->req.filename));
    }
        //import_db(client_ctx->req.filename);
// TODO: put a check to see if all the required fields are present.
//
    if (enqueue_client_req(&svr_log, client_ctx) != 0) {
        fprintf(stderr, "Enqueue failed, thats bad!!!\n");
        goto _EXIT;
    }
    if(!is_primary)
    {
        close(client_ctx->fd);
    }
    return (void *)0;
_EXIT:
    if (client_ctx->fd >= 0) {
        close(client_ctx->fd);
        client_ctx->fd = -1;
    }
    if (client_ctx) {
        free(client_ctx);
        client_ctx = NULL;
    }
    return (void *)-1;
}

void *execute_msg(void *arg) {
    fprintf(stderr, "Exec_msg called \n");
    client_ctx_t *client_ctx = arg;

    pthread_mutex_lock(&storage_lock);
    print_user_req(client_ctx, "Req");

    if (handle_storage(client_ctx) != 0) {
        printf("ERROR: handle storage failed\n");
        write_client_responce(client_ctx, "FAIL", "Check inputs");
    } else {
        // printf("handle storage success\n");
    }
    print_user_req(client_ctx, "Res");
    //if(client_ctx->req.msg_type != MSG_HEARTBEAT)
    msg_count++;
    pthread_mutex_unlock(&storage_lock);

    //
    // send responce.
    write_client_responce(client_ctx, "OK", "Success");
    //_EXIT:
    if (client_ctx->fd >= 0) {
        close(client_ctx->fd);
        client_ctx->fd = -1;
    }
    if(client_ctx->req.payload.size > 0)
    {
        free(client_ctx->req.payload.data);
    }
    if (client_ctx) {
        free(client_ctx);
        client_ctx = NULL;
    }
    return (void *)0;
}

void init_server_ctx(server_ctx_t *ctx) {
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    return;
}

void init_client_ctx(client_ctx_t *ctx) {
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    memset(&ctx->req, 0, sizeof(ctx->req));
    return;
}
void open_ports_secondary()
{
    init_bsvr_ctx(&bckup_svr[0]);
    init_bsvr_ctx(&bckup_svr[1]);
    // Hard coded for now;
    // take this info dynamically from somewhere else later
    memcpy(bckup_svr[0].info.server_ip, "127.0.0.1", strlen("127.0.0.1")+1);
    bckup_svr[0].info.port = 23457; 
    if( (bckup_svr[0].fd = connect_to_server(&bckup_svr[0].info))
            < 0)
    {
        fprintf(stderr, "failed open channel to backup server!!\n");
        exit(0);
    }

    memcpy(bckup_svr[1].info.server_ip, "127.0.0.1", strlen("127.0.0.1")+1);
    bckup_svr[1].info.port = 23458; 
    if( (bckup_svr[1].fd = connect_to_server(&bckup_svr[1].info))
            < 0)
    {
        fprintf(stderr, "failed open channel to backup server!!\n");
        exit(0);
    }
}
int write_check_point(bsvr_ctx *ctx, char *chk_file_name)
{
    char buffer[MAX_LENGTH];
        int fd = -1, n;
    fd = open(chk_file_name, O_RDONLY);
    if(fd < 0)
    {
        fprintf(stderr, "open of check point failed.\n");
        return -1;
    }
    char resp_buf[MAX_LENGTH];
    int resp_len =
        snprintf(resp_buf, sizeof(resp_buf), 
                "Client ID: %d\r\n"
                "Request No: %lu\r\n"
                "Message Type: %d\r\n"
                "Data Length: %lu\r\n"
                "\r\n",
                server_id, chk_point_num, MSG_CHK_PT,
                get_file_size(chk_file_name));
    if(write(ctx->fd, resp_buf, resp_len) < 0)
    {
            fprintf(stderr, "failed writing checkpoint\n");
        goto _END;
    }
    while((n = read(fd, buffer, MAX_LENGTH)) > 0)
    {
        if(write(ctx->fd, buffer, n) < 0)
        {
            fprintf(stderr, "failed writing checkpoint\n");
            goto _END; 
        }
    }
_END:
    close(fd);
    return 0;


}
void * send_checkpoint(void *argvp)
{
    while(1)
    {
        if(msg_count < 2)
        {
            sleep(1);
            continue;
        }
        msg_count = 0;
        open_ports_secondary();

        // Add code to get the checkpoint
        pthread_mutex_lock(&storage_lock);
        char checkpoint_file_name[MAX_LENGTH];
        export_db(checkpoint_file_name);
        pthread_mutex_unlock(&storage_lock);
        for(unsigned int i = 0; i < sizeof(bckup_svr)/sizeof(bsvr_ctx); i++)
        {
            write_check_point(&bckup_svr[i], checkpoint_file_name); 
            close(bckup_svr[i].fd);
        }
        chk_point_num++;
    }
    return (void *)0;
}
int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    int listen_fd = -1, optval = 1, accept_ret_val = -1;
    struct sockaddr_in server_addr;

    if (argc < 3) {
        // print usage
        fprintf(stderr, "Usage: %s <ip_address> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    is_primary = atoi(argv[3]);
    server_id = atoi(argv[4]);
    if (verbose > 5 || verbose < 0) {
        fprintf(stderr, "Warning: illegal verbose value ignoring it.\n");
    }
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr.s_addr) != 1) {
        fprintf(stderr, "Entered IP Address invalid!\n");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_port = htons(atoi(argv[2]));
    listen_fd = socket(AF_INET, SOCK_STREAM, 0); // create a TCP socket.
    if (listen_fd < 0) {
        fprintf(stderr, "Socket creation failed. Reason: %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
               sizeof(int));
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
        0) {
        fprintf(stderr, "Bind to address failed, Reason: %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 50) < 0) {
        fprintf(stderr, "listen() failed. Reason: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(is_primary)
    {
        pthread_t id;
        pthread_create(&id, NULL, send_checkpoint, NULL);
    }
    struct sockaddr client_addr;
    socklen_t client_addr_len;
    pthread_t th_id;
    memset(&client_addr, 0, sizeof(struct sockaddr));

    init_log_queue(&svr_log, 50, 50);

    if (start_worker_threads(&svr_log, execute_msg, execute_msg, !is_primary) 
            != 0) {
        fprintf(stderr, "Failed to start the worker thread\n");
        exit(-1);
    }

    storage_init(); // init database for storage

    pthread_mutex_init(&storage_lock, NULL);

    while (1) {
        accept_ret_val = accept(listen_fd, &client_addr, &client_addr_len);
        if (verbose >= 3) {
            printf("Connection accepted !!!\n");
        }
        if (accept_ret_val < 0) {
            if (errno == ECONNABORTED) {
                continue;
            } else {
                fprintf(stderr, "Error on accept exiting. Reason: %s\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        client_ctx_t *conn_client_ctx = malloc(sizeof(client_ctx_t));
        init_client_ctx(conn_client_ctx);
        conn_client_ctx->fd = accept_ret_val;
        memcpy(&conn_client_ctx->addr, &client_addr,
               sizeof(struct sockaddr_in));
        if (pthread_create(&th_id, NULL, handle_connection,
                           (void *)conn_client_ctx) != 0)
#if 0
        if(enqueue_client_req(&svr_log,conn_client_ctx) != 0)
#endif
        {
            fprintf(stderr, "!!!!!!!! Pthread Create Failed !!!!!!!");
            if (conn_client_ctx->fd != -1) {
                close(conn_client_ctx->fd);
                free(conn_client_ctx);
            }
        }
    }
    return 0;
}
