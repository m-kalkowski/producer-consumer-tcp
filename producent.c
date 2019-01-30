#include "ringbuff.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
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

pthread_mutex_t circ_buf_lock;
pthread_mutex_t file_lock;
FILE *fp;
double wall0;
double cpu0;
int numOfConnectedClients = 0;

/* Thread routine to serve connection to client. */
void *pthread_routine(void *arg);

/* Thread routine to serve producer function. */
void *producer_routine(void *arg);

void *timer_routine(void *arg);

/* Signal handler to handle SIGTERM and SIGINT signals. */
void signal_handler(int signal_number);

ringbuff_t ring_buffer;
char ring_buff_data[BUFF_SIZE];

double get_wall_time() {
    struct timeval time;
    if (gettimeofday(&time, NULL))
        return 0;
    return (double)time.tv_sec + (double)time.tv_usec * 0.000001;
}

double get_cpu_time() {
    return (double)clock() / CLOCKS_PER_SEC;
}

int main(int argc, char *argv[]) {
    int port, socket_fd, new_socket_fd;
    struct sockaddr_in address;
    pthread_attr_t pthread_attr;
    producer_arg_t producer_arg = {
        .path = NULL,
        .tempo = 0,
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

    if ((fp=fopen(producer_arg.path, "r+")) == NULL) {
        printf("Cannot open file for writing\n");
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

    if (pthread_mutex_init(&file_lock, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(1);
    }
    /* Create thread to serve connection to client. */
    if (pthread_create(&pthread, &pthread_attr, producer_routine, (void *)&producer_arg) != 0) {
        perror("pthread_create");
        exit(1);
    }

    /* Create thread to serve connection to client. */
    if (pthread_create(&pthread, &pthread_attr, timer_routine, (void *)&producer_arg) != 0) {
        perror("pthread_create");
        exit(1);
    }

    wall0 = get_wall_time();
    cpu0 = get_cpu_time();

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

        numOfConnectedClients++;

        pthread_mutex_lock(&file_lock);
        fprintf(fp, "[%f:%f] %s\n", get_wall_time() - wall0, get_cpu_time() - cpu0, inet_ntoa(pthread_arg->client_address.sin_addr));
        pthread_mutex_unlock(&file_lock);
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

void *timer_routine(void *arg) {

    float occupancy;

    while (1) {

        pthread_mutex_lock(&circ_buf_lock);
        occupancy = abs(ring_buffer.r - ring_buffer.w);
        pthread_mutex_unlock(&circ_buf_lock);

        pthread_mutex_lock(&file_lock);
        fprintf(fp, "[%f:%f] %f, %f\n", get_wall_time() - wall0, get_cpu_time() - cpu0, occupancy, occupancy / BUFF_SIZE * 100);
        pthread_mutex_unlock(&file_lock);
        sleep(5);
    }
}

void *pthread_routine(void *arg) {
    pthread_arg_t *pthread_arg = (pthread_arg_t *)arg;
    int new_socket_fd = pthread_arg->new_socket_fd;
    char rxBuff[4];
    char txBuff[112 * 1024];
    size_t len;
    int counter = 0;

    while (1) {
        if (read(new_socket_fd, rxBuff, sizeof(rxBuff)) <= 0)
            break;

        pthread_mutex_lock(&circ_buf_lock);
        while ((len = ringbuff_read(&ring_buffer, txBuff, sizeof(txBuff))));
        pthread_mutex_unlock(&circ_buf_lock);

        if (write(new_socket_fd, txBuff, sizeof(txBuff)) <= 0)
            break;
        counter++;
    }

    close(new_socket_fd);
    pthread_mutex_lock(&file_lock);
    fprintf(fp, "[%f:%f] %s, %d\n", get_wall_time() - wall0, get_cpu_time() - cpu0, inet_ntoa(pthread_arg->client_address.sin_addr), counter);
    pthread_mutex_unlock(&file_lock);

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
        ringbuff_write(&ring_buffer, buf, sizeof(buf));
        pthread_mutex_unlock(&circ_buf_lock);
        //printf("Buf: %s\n", buf);
        //printf("Tempo[us]: %d\n", time_us);
        usleep(time_us);
    }

    return NULL;
}

void signal_handler(int signal_number) {
    /* TODO: Put exit cleanup code here. */
    fclose(fp);
    exit(0);
}
