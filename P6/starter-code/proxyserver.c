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

#include "safequeue.h"
#include "proxyserver.h"


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

/**
 * Global priority queue and thread variables
 */
priority_queue* pq;
pthread_t* listener_threads;
pthread_t* worker_threads;
int* thread_idx;
int* server_fds;


void send_error_response(int client_fd, status_code_t err_code, char *err_msg) {
    http_start_response(client_fd, err_code);
    http_send_header(client_fd, "Content-Type", "text/html");
    http_end_headers(client_fd);
    char *buf = malloc(strlen(err_msg) + 2);
    sprintf(buf, "%s\n", err_msg);
    http_send_string(client_fd, buf);
    shutdown(client_fd, SHUT_WR);
    close(client_fd);
    free(buf);  // ORIGNAL CODE DIDN'T FREE
    buf = NULL;  // No dangling pointers
    return;
}

/*
 * forward the client request to the fileserver and
 * forward the fileserver response to the client
 */
void serve_request(struct proxy_request* pr) {
    if (!pr || !pr->request)
        return;

    if (pr->request->delay) // Sleep if delay is specified
        sleep(pr->request->delay);

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

    if (!buffer) {
        perror("malloc failed in serve_request\n");
        goto end_op;
    }

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

    end_op:
    // close the connection to the fileserver
    shutdown(fileserver_fd, SHUT_WR);
    close(fileserver_fd);

    // close the connection to the client
    shutdown(client_fd, SHUT_WR);
    close(client_fd);

    // Free resources and exit
    if (buffer)
        free(buffer);
    buffer = NULL;  // No dangling pointers

    http_request_destroy(pr->request);

    if (pr)
        free(pr);
    pr = NULL;  // No dangling pointers
}

/**
 * Parse the priority of a request given the path
 * @param  path The path of the requested resource
 * @return      The priority of the resource
 */
static uint parse_priority(char* path) {
    // We parse the priority using some properties of the path
    // The pattern of the path is
    //      "/<num>/<file>"
    // The <num>
    // To parse the <num>, we read numeric characters starting from
    // index 1, (i.e. after the first forward slash),
    // till there are no more numeric chars,
    // (i.e. till we hit the next forward slash)

    int len = strlen(path);

    int i = 0;
    while (++i < len && path[i] >= '0' && path[i] <= '9');

    // Null terminate the string to read using atoi
    char orig = path[i];
    path[i] = '\0';

    // Read the number
    uint num = (uint) atoi(&path[1]);

    // Revert the null termination
    path[i] = orig;

    return num;
}

/**
 * Routine for a worker thread. Serves requests forever
 * @param args Unused
 */
void* do_work(void*) {
    while (!EXIT_FLAG) { // Loop forever
        // get_work blocks till there is a request in the queue
        struct proxy_request* pr = (struct proxy_request*) get_work(pq);

        // Serve the request
        if (pr)
            serve_request(pr);
    }
    return NULL;
}

/*
 * opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 *
 * @param args Takes the index into the listener_ports and server_fds arrays
 */
void* serve_forever(void* args) {
    // The index into the global arrays
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

    // printf("Listening on port %d...\n", proxy_port);

    struct sockaddr_in client_address;
    size_t client_address_length = sizeof(client_address);
    int client_fd;
    while (!EXIT_FLAG) {
        client_fd = accept(*server_fd,
                           (struct sockaddr *)&client_address,
                           (socklen_t *)&client_address_length);
        if (client_fd < 0) {
            perror("Error accepting socket");
            continue;
        }

        /* printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port); */

        // Parse the http request headers
        struct http_request* req = http_request_parse(client_fd);

        // If parsing failed, do not server
        if (!req) {
            perror("Failed to parse http_request");
            continue;
        }

        struct proxy_request* pr;

        // Check if the request was a GetJob request
        if (strcmp(req->path, GETJOBCMD) == 0) {
            // Get a job from the queue if there is one, without blocking
            pr = (struct proxy_request*) get_work_nonblocking(pq);

            // If no request, return an error response
            if (!pr) {
                char buf[40];
                sprintf(buf, "Elem: %p | NO JOBS IN QUEUE!", pr);
                send_error_response(client_fd, QUEUE_EMPTY, buf);
            } else { // Otherwise return the path
                send_error_response(client_fd, OK, pr->request->path);
                http_request_destroy(pr->request);
                free(pr);
                pr = NULL;  // No dangling pointers
            }

            http_request_destroy(req);
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
            pr = NULL;  // No dangling pointers
            free(elem);
            elem = NULL;  // No dangling pointers
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
    EXIT_FLAG = 1;
    pthread_cond_broadcast(&pq->pq_cond_fill);
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
    //print_settings();

    pq = create_queue(max_queue_size);

    listener_threads = malloc(sizeof(pthread_t) * num_listener);

    if (!listener_threads) {
        perror("FAILED TO MALLOC THREADS!\n");
        exit(0);
    }

    thread_idx = malloc(sizeof(int) * num_listener);

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

    // SIGINT occured, so threads exited
    // Cleanup
    for (int i = 0; i < num_listener; i++) {
        if (close(server_fds[i]) < 0) {
            perror("Failed to close server_fd (ignoring)\n");
        }
    }

    // Shouldn't have any running threads here
    // so pq->pq_mutex lock wouldn't be held
    for (int i = 0; i < pq->size; i++) {
         if (!pq->queue[i]) {
             continue;
         }

         struct proxy_request* pr = (struct proxy_request*) pq->queue[i]->value;
         if (!pr) {
             continue;
         }

         shutdown(pr->client_fd, SHUT_WR);
         close(pr->client_fd);
    }

    free(listener_ports);
    free(server_fds);
    free(listener_threads);
    free(worker_threads);
    free(thread_idx);
    destroy_queue(pq);

    return EXIT_SUCCESS;
}
