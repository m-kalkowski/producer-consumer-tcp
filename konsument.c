#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SERVER_NAME_LEN_MAX 255

int main(int argc, char *argv[]) {
    //char server_name[SERVER_NAME_LEN_MAX + 1] = { 0 };
    int server_port, socket_fd;
    struct hostent *server_host;
    struct sockaddr_in server_address;
    char *stringNumberOfMessages = NULL;
    int numberOfMessages = 0;
    char *stringDelayR = NULL;
    char *stringDelayS = NULL;
    float delayR = 0;
    float delayS = 0;
    bool rSelected = false;
    bool sSelected = false;
    bool numberOfMessagesSelected = false;
    int c;
    char *localhost = "127.0.0.1";

    srand(time(NULL));

    while ((c = getopt(argc, argv, "#:r:s:")) != -1) {
        switch (c) {
            case '#': {
                stringNumberOfMessages = optarg;
                char *stringL1 = NULL;
                char *stringL2 = NULL;
                stringL1 = strtok(stringNumberOfMessages, ":");
                stringL2 = strtok(NULL, ":");
                if (stringL2 == NULL) {
                    numberOfMessages = atoi(stringL1);
                    numberOfMessagesSelected = true;
                    break;
                }
                int L1 = atoi(stringL1);
                int L2 = atoi(stringL2);
                if (L2 < L1)
                    abort();
                numberOfMessages = rand() % (L2 + 1 - L1) + L1;
                numberOfMessagesSelected = true;
                break;
            }
            case 'r': {
                stringDelayR = optarg;
                char *stringL1 = NULL;
                char *stringL2 = NULL;
                stringL1 = strtok(stringDelayR, ":");
                stringL2 = strtok(NULL, ":");
                if (stringL2 == NULL) {
                    delayR = atof(stringL1);
                    rSelected = true;
                    break;
                }
                int L1 = atoi(stringL1);
                int L2 = atoi(stringL2);
                if (L2 < L1)
                    abort();
                delayR = rand() % (L2 + 1 - L1) + L1;
                rSelected = true;
                break;
            }
            case 's': {
                stringDelayS = optarg;
                char *stringL1 = NULL;
                char *stringL2 = NULL;
                stringL1 = strtok(stringDelayS, ":");
                stringL2 = strtok(NULL, ":");
                if (stringL2 == NULL) {
                    delayS = atof(stringL1);
                    sSelected = true;
                    break;
                }
                int L1 = atoi(stringL1);
                int L2 = atoi(stringL2);
                if (L2 < L1)
                    abort();
                delayS = rand() % (L2 + 1 - L1) + L1;
                sSelected = true;
                break;
            }
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

    if (rSelected == true && sSelected == true) {
        printf("Please select only one delay\n");
        abort();
    }

    if (rSelected == false && sSelected == false) {
        printf("No delay selected\n");
        abort();
    }

    if (!numberOfMessagesSelected) {
        printf("No number of messages selected\n");
        abort();
    }

    /* Get port from command line arguments or stdin. */
    char *addr_port = NULL;
    char *server_name = NULL;
    char *Port = NULL;
    addr_port = argv[argc - 1];
    if (addr_port == NULL) {
        printf("Invalid option: addres:port\n");
        abort();
    }
    server_name = strtok(addr_port, ":");
    if (server_name == NULL) {
        printf("No address provided, using localhost\n");
    }
    Port = strtok(NULL, ":");
    if (Port == NULL && server_name != NULL) {
        Port = server_name;
        server_name = localhost;
    }
    else if (Port == NULL) {
        printf("Invalid port\n");
        abort();
    }

    server_port = atoi(Port);

    printf("server name: %s, server port: %d, delay r: %f, delay s: %f, number of messages: %d\n", server_name, server_port, delayR, delayS, numberOfMessages);
    /* Get server host from server name. */
    server_host = gethostbyname(server_name);

    /* Initialise IPv4 server address with server host. */
    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    memcpy(&server_address.sin_addr.s_addr, server_host->h_addr, server_host->h_length);

    /* Create TCP socket. */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* Connect to socket with server address. */
    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof server_address) == -1) {
		perror("connect");
        exit(1);
	}

    char txBuff[4];
    char rxBuff[112 * 1024];

    for (int i=0; i<numberOfMessages; ++i) {
        write(socket_fd, txBuff, sizeof(txBuff));
    }

    for (int i=0; i<numberOfMessages; ++i) {
        read(socket_fd, rxBuff, sizeof(rxBuff));
        printf("%d\n", i);
    }

    /* TODO: Put server interaction code here. For example, use
     * write(socket_fd,,) and read(socket_fd,,) to send and receive messages
     * with the client.
     */

    close(socket_fd);
    return 0;
}
