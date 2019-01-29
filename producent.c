/*
 * server.c
 * Version 20161003
 * Written by Harry Wong (RedAndBlueEraser)
 */
#include "ringbuff.h"

#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLOG 10
#define BUFF_SIZE (125 * 1024 * 1024 / 100)

typedef struct pthread_arg_t {
    int new_socket_fd;
    struct sockaddr_in client_address;
    /* TODO: Put arguments passed to threads here. See lines 116 and 139. */
} pthread_arg_t;

typedef struct producer_arg_t {
    char *path;
    float tempo;
} producer_arg_t;

// Stworzenie struktury magazynu w postaci bufora cyklicznego
/*typedef struct circ_bbuf_t {
    char buffer[BUFF_SIZE];
    int head;
    int tail;
} circ_bbuf_t;

circ_bbuf_t circ_buf = {
    .buffer = {0},
    .head = 0,
    .tail = 0,
};*/

pthread_mutex_t circ_buf_lock;

/* Thread routine to serve connection to client. */
void *pthread_routine(void *arg);

/* Thread routine to serve producer function. */
void *producer_routine(void *arg);

/* Signal handler to handle SIGTERM and SIGINT signals. */
void signal_handler(int signal_number);

/*int circ_bbuf_push(circ_bbuf_t *c, char *data, int step)
{
    int next;

    next = c->head + step;
    if (next >= BUFF_SIZE)
        next = 0;

    if (next == c->tail)
        return -1;

    for (int i=0; i<step; ++i)
        c->buffer[c->head + i] = data[i];

    c->head = next;
    return 0;
}

int circ_bbuf_pop(circ_bbuf_t *c, char *data, int step)
{
    int next;

    if (c->head == c->tail)
        return -1;

    if (step > c->head)
        return -1;

    next = c->tail + step;
    if (next >= BUFF_SIZE)
        next = 0;

    for (int i=0; i<step; ++i)
        data[i] = c->buffer[c->tail + i];
    c->tail = next;
    return 0;
}*/
ringbuff_t ring_buffer;
char ring_buff_data[BUFF_SIZE];

int main(int argc, char *argv[]) {
    int port, socket_fd, new_socket_fd;
    struct sockaddr_in address;
    pthread_attr_t pthread_attr;
    producer_arg_t producer_arg = {
        .path = NULL,
        .tempo = 0
    };
    pthread_arg_t *pthread_arg;
    pthread_t pthread;
    socklen_t client_address_len;
    int c;
    char *localhost = "127.0.0.1";
    ringbuff_init(&ring_buffer, ring_buff_data, sizeof(ring_buff_data));


    while ((c = getopt(argc, argv, "r:t:")) != -1) {
        switch (c) {
            case 'r':
                producer_arg.path = optarg;
                break;
            case 't':
                producer_arg.tempo = atof(optarg);
                if (producer_arg.tempo < 1 && producer_arg.tempo > 8)
                    abort();
                break;
            case ':':
                printf("-%c without argument\n", optopt);
                break;
            case '?':
                printf("unknown arg %c\n", optopt);
                break;
            default:
                abort();
       }
    }

    /* Get port from command line arguments or stdin. */
    char *addr_port = NULL;
    char *addr = NULL;
    char *Port = NULL;
    addr_port = argv[argc - 1];
    if (addr_port == NULL) {
        printf("Invalid option: addres:port\n");
        abort();
    }
    addr = strtok(addr_port, ":");
    if (addr == NULL) {
        printf("No address provided, using localhost\n");
    }
    Port = strtok(NULL, ":");
    if (Port == NULL && addr != NULL) {
        Port = addr;
        addr = localhost;
    }
    else if (Port == NULL) {
        printf("Invalid port\n");
        abort();
    }

    printf("path: %s, tempo: %f, addr: %s, port: %s\n", producer_arg.path, producer_arg.tempo, addr, Port);
    port = atoi(Port);

    /* Initialise IPv4 address. */
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    /* Create TCP socket. */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* Bind address to socket. */
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof address) == -1) {
        perror("bind");
        exit(1);
    }

    /* Listen on socket. */
    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    /* Assign signal handlers to signals. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    /* Initialise pthread attribute to create detached threads. */
    if (pthread_attr_init(&pthread_attr) != 0) {
        perror("pthread_attr_init");
        exit(1);
    }
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate");
        exit(1);
    }

    if (pthread_mutex_init(&circ_buf_lock, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(1);
    }
    /* Create thread to serve connection to client. */
    if (pthread_create(&pthread, &pthread_attr, producer_routine, (void *)&producer_arg) != 0) {
        perror("pthread_create");
        exit(1);
    }

    while (1) {
        /* Create pthread argument for each connection to client. */
        /* TODO: malloc'ing before accepting a connection causes only one small
         * memory when the program exits. It can be safely ignored.
         */
        pthread_arg = (pthread_arg_t *)malloc(sizeof *pthread_arg);
        if (!pthread_arg) {
            perror("malloc");
            continue;
        }

        /* Accept connection to client. */
        client_address_len = sizeof pthread_arg->client_address;
        new_socket_fd = accept(socket_fd, (struct sockaddr *)&pthread_arg->client_address, &client_address_len);
        if (new_socket_fd == -1) {
            perror("accept");
            free(pthread_arg);
            continue;
        }

        /* Initialise pthread argument. */
        pthread_arg->new_socket_fd = new_socket_fd;
        /* TODO: Initialise arguments passed to threads here. See lines 22 and
         * 139.
         */

        /* Create thread to serve connection to client. */
        if (pthread_create(&pthread, &pthread_attr, pthread_routine, (void *)pthread_arg) != 0) {
            perror("pthread_create");
            free(pthread_arg);
            continue;
        }
    }
    /* close(socket_fd);
     * TODO: If you really want to close the socket, you would do it in
     * signal_handler(), meaning socket_fd would need to be a global variable.
     */
    free(producer_arg.path);

    return 0;
}

void *pthread_routine(void *arg) {
    pthread_arg_t *pthread_arg = (pthread_arg_t *)arg;
    int new_socket_fd = pthread_arg->new_socket_fd;
    struct sockaddr_in client_address = pthread_arg->client_address;
    char rxBuff[4];
    char txBuff[112 * 1024];
    size_t len;

    while (1) {
        if (read(new_socket_fd, rxBuff, sizeof(rxBuff)) < 0)
            break;
        pthread_mutex_lock(&circ_buf_lock);
        while ((len = ringbuff_read(&ring_buffer, txBuff, sizeof(txBuff))));
        pthread_mutex_unlock(&circ_buf_lock);

        write(new_socket_fd, txBuff, sizeof(txBuff));
    }

    close(new_socket_fd);
    return NULL;
}

void *producer_routine(void *arg) {
    producer_arg_t *producer_arg = (producer_arg_t *)arg;
    int time_us = (int)(producer_arg->tempo * 60 / 96 * 1000000);
    char c = 'a';
    char buf[640];
    int status = 0;

    while (1) {

        if (status == 0) {
            for (int i=0; i<640; ++i) {
                buf[i] = c;
            }
            if (c == 'z')
                c = 'A';
            else if (c == 'Z')
                c = 'a';
            else
                c++;
        }

        pthread_mutex_lock(&circ_buf_lock);
        //status = circ_bbuf_push(&circ_buf, buf, sizeof(buf));
        ringbuff_write(&ring_buffer, buf, sizeof(buf));

        pthread_mutex_unlock(&circ_buf_lock);
        printf("Buf: %s\n", buf);
        //printf("Tempo[us]: %d\n", time_us);
        usleep(time_us / 100);
    }

    return NULL;
}

void signal_handler(int signal_number) {
    /* TODO: Put exit cleanup code here. */
    exit(0);
}
