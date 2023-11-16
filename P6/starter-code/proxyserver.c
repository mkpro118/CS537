#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "proxyserver.h"
#include "safequeue.h"


/*
 * Constants
 */
#define RESPONSE_BUFSIZE 10000

static const char* template_resp = "%s %s HTTP/1.1\r\n"
                                   "Host: localhost:%d\r\n"
                                   "User-Agent: proxy_server/0.1\r\n"
                                   "Accept: */*\r\n\r\n";

/*
 * Global configuration variables.
 * Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int num_listener;
int *listener_ports;
int num_workers;
char *fileserver_ipaddr;
int fileserver_port;
int max_queue_size;

priority_queue* pq;

void send_error_response(int client_fd, status_code_t err_code, char *err_msg) {
    http_start_response(client_fd, err_code);
    http_send_header(client_fd, "Content-Type", "text/html");
    http_end_headers(client_fd);
    char *buf = malloc(strlen(err_msg) + 2);
    sprintf(buf, "%s\n", err_msg);
    http_send_string(client_fd, buf);
    shutdown(client_fd, SHUT_WR);
    close(client_fd);
    free(buf);
    return;
}

/*
 * forward the client request to the fileserver and
 * forward the fileserver response to the client
 */
void serve_request(struct proxy_request* pr) {
    int delay = pr->request->delay ? atoi(pr->request->delay) : 0;

    if (delay > 0)
        sleep(delay);

    printf("Sleep Delay: %s | %d\n", pr->request->delay, delay);

    int client_fd = pr->client_fd;

    // create a fileserver socket
    int fileserver_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fileserver_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        exit(errno);
    }

    // create the full fileserver address
    struct sockaddr_in fileserver_address;
    fileserver_address.sin_addr.s_addr = inet_addr(fileserver_ipaddr);
    fileserver_address.sin_family = AF_INET;
    fileserver_address.sin_port = htons(fileserver_port);

    // connect to the fileserver
    int connection_status = connect(fileserver_fd, (struct sockaddr *)&fileserver_address,
                                    sizeof(fileserver_address));
    if (connection_status < 0) {
        // failed to connect to the fileserver
        printf("Failed to connect to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");
        return;
    }

    // successfully connected to the file server
    char *buffer = (char *)malloc(RESPONSE_BUFSIZE * sizeof(char));

    // forward the client request to the fileserver
    // int bytes_read = read(client_fd, buffer, RESPONSE_BUFSIZE);
    ssize_t bytes_read = sprintf(buffer, template_resp, pr->request->method, pr->request->path, pr->port);
    int ret = http_send_data(fileserver_fd, buffer, bytes_read);
    if (ret < 0) {
        printf("Failed to send request to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");

    } else {
        // forward the fileserver response to the client
        while (1) {
            int bytes_read = recv(fileserver_fd, buffer, RESPONSE_BUFSIZE - 1, 0);
            if (bytes_read <= 0) // fileserver_fd has been closed, break
                break;
            ret = http_send_data(client_fd, buffer, bytes_read);
            if (ret < 0) { // write failed, client_fd has been closed
                break;
            }
        }
    }

    // close the connection to the fileserver
    shutdown(fileserver_fd, SHUT_WR);
    close(fileserver_fd);

    // close the connection to the client
    shutdown(client_fd, SHUT_WR);
    close(client_fd);

    // Free resources and exit
    free(buffer);
    free(pr->request->method);
    free(pr->request->path);
    free(pr->request->delay);
    free(pr->request);
    free(pr);
}

static uint parse_priority(char* path) {
    int len = strlen(path);

    int i = 1;
    while (i < len && path[i] >= 'A' && path[i] <= 'Z')
        i++;

    char orig = path[i];
    path[i] = '\0';
    uint num = (uint) atoi(&path[1]);
    path[i] = orig;

    return num;
}

void* do_work(void* args) {
    while (1) {
        struct proxy_request* pr = (struct proxy_request*) get_work(pq);
        serve_request(pr);
    }
}


int* server_fds;
/*
 * opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void* serve_forever(void* args) {

    int idx = *((int*) args);

    int* server_fd = &server_fds[idx];

    // create a socket to listen
    *server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (*server_fd == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }

    // manipulate options for the socket
    int socket_option = 1;
    if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                   sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        exit(errno);
    }


    int proxy_port = listener_ports[idx];
    // create the full address of this proxyserver
    struct sockaddr_in proxy_address;
    memset(&proxy_address, 0, sizeof(proxy_address));
    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = INADDR_ANY;
    proxy_address.sin_port = htons(proxy_port); // listening port

    // bind the socket to the address and port number specified in
    if (bind(*server_fd, (struct sockaddr *)&proxy_address,
             sizeof(proxy_address)) == -1) {
        perror("Failed to bind on socket");
        exit(errno);
    }

    // starts waiting for the client to request a connection
    if (listen(*server_fd, 1024) == -1) {
        perror("Failed to listen on socket");
        exit(errno);
    }

    printf("Listening on port %d...\n", proxy_port);

    struct sockaddr_in client_address;
    size_t client_address_length = sizeof(client_address);
    int client_fd;
    while (1) {
        client_fd = accept(*server_fd,
                           (struct sockaddr *)&client_address,
                           (socklen_t *)&client_address_length);
        if (client_fd < 0) {
            perror("Error accepting socket");
            continue;
        }

        printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port);

        struct http_request* req = http_request_parse(client_fd);

        if (!req) {
            perror("Failed to parse http_request");
            continue;
        }

        struct proxy_request* pr;

        if (strcmp(req->path, GETJOBCMD) == 0) {
            pr = (struct proxy_request*) get_work_nonblocking(pq);
            if (!pr) {
                char buf[40];
                sprintf(buf, "Elem: %p | NO JOBS IN QUEUE!", pr);
                send_error_response(client_fd, QUEUE_EMPTY, buf);
                http_request_destroy(req);
            } else {
                send_error_response(client_fd, OK, pr->request->path);
            }
            continue;
        }

        pr = malloc(sizeof(struct proxy_request));
        pr->request = req;
        pr->client_fd = client_fd;
        pr->port = proxy_port;

        pq_element* elem = malloc(sizeof(pq_element));
        if (!pr || !elem) {
            perror("malloc failed in serve forever");
            exit(0);
        }


        elem->priority = parse_priority(req->path);
        elem->value = (void*) pr;

        if(add_work(pq, elem) < 0) {
            send_error_response(client_fd, QUEUE_FULL, "QUEUE IS FULL!");
            http_request_destroy(req);
            free(pr);
            free(elem);
        }
    }

    shutdown(*server_fd, SHUT_RDWR);
    close(*server_fd);
    return NULL;
}

/*
 * Default settings for in the global configuration variables
 */
void default_settings() {
    num_listener = 1;
    listener_ports = (int *)malloc(num_listener * sizeof(int));
    listener_ports[0] = 8000;

    num_workers = 1;

    fileserver_ipaddr = "127.0.0.1";
    fileserver_port = 3333;

    max_queue_size = 100;
}

void print_settings() {
    printf("\t---- Setting ----\n");
    printf("\t%d listeners [", num_listener);
    for (int i = 0; i < num_listener; i++)
        printf(" %d", listener_ports[i]);
    printf(" ]\n");
    printf("\t%d workers\n", num_listener);
    printf("\tfileserver ipaddr %s port %d\n", fileserver_ipaddr, fileserver_port);
    printf("\tmax queue size  %d\n", max_queue_size);
    printf("\t  ----\t----\t\n");
}

void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    for (int i = 0; i < num_listener; i++) {
        if (close(server_fds[i]) < 0) perror("Failed to close server_fd (ignoring)\n");
    }
    free(listener_ports);
//    destroy_queue(pq);
    exit(0);
}

char *USAGE =
    "Usage: ./proxyserver [-l 1 8000] [-n 1] [-i 127.0.0.1 -p 3333] [-q 100]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);

    /* Default settings */
    default_settings();

    for (int i = 1; i < argc; i++) {
        if (strcmp("-l", argv[i]) == 0) {
            num_listener = atoi(argv[++i]);
            free(listener_ports);
            listener_ports = (int *)malloc(num_listener * sizeof(int));
            for (int j = 0; j < num_listener; j++) {
                listener_ports[j] = atoi(argv[++i]);
            }

            server_fds = (int*) malloc(num_listener * sizeof(int));
        } else if (strcmp("-w", argv[i]) == 0) {
            num_workers = atoi(argv[++i]);
        } else if (strcmp("-q", argv[i]) == 0) {
            max_queue_size = atoi(argv[++i]);
        } else if (strcmp("-i", argv[i]) == 0) {
            fileserver_ipaddr = argv[++i];
        } else if (strcmp("-p", argv[i]) == 0) {
            fileserver_port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }
    print_settings();

    pq = create_queue(max_queue_size);

    pthread_t* listener_threads;
    listener_threads = malloc(sizeof(pthread_t) * num_listener);

    if (!listener_threads) {
        perror("FAILED TO MALLOC THREADS!\n");
        exit(0);
    }

    int* thread_idx = malloc(sizeof(int) * num_listener);

    if (!thread_idx) {
        perror("MALLOC FAILED!\n");
        exit(0);
    }

    for (int i = 0; i < num_listener; i++)
        thread_idx[i] = i;

    for (int i = 0; i < num_listener; i++) {
        if (pthread_create(&listener_threads[i], NULL, serve_forever, (void*) &thread_idx[i])) {
            perror("FAILED TO CREATE LISTENER THREADS\n");
            exit(0);
        }
    }

    pthread_t* worker_threads;
    worker_threads = malloc(sizeof(pthread_t) * num_workers);

    if (!worker_threads) {
        perror("FAILED TO MALLOC THREADS!\n");
        exit(0);
    }

    for (int i = 0; i < num_workers; i++) {
        if (pthread_create(&worker_threads[i], NULL, do_work, NULL)) {
            perror("FAILED TO CREATE LISTENER THREADS\n");
            exit(0);
        }
    }

    for (int i = 0; i < num_listener; i++) {
        pthread_join(listener_threads[i], NULL);
    }

    for (int i = 0; i < num_workers; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    return EXIT_SUCCESS;
}
