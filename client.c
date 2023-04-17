#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>

#define MAXLINE 1000
#define LISTENQ 1024

void exitClientFromCode(int socketFdConnection)
{
    close(socketFdConnection);
    printf("exiting..\n");
    exit(0);
}

void tcpConnectionForClient(int socketFdConnection, char *ipAddress, int port) {
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ipAddress, &servaddr.sin_addr) != 1) {
        fprintf(stderr, "Error: Invalid IP address\n");
        exit(EXIT_FAILURE);
    }

    if (connect(socketFdConnection, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("connect error");
        exit(EXIT_FAILURE);
    }
}
void *readClinetAndWriteInServer(void *arg)
{
    int socketFdConnection = *(int *)arg;
    char input[MAXLINE];

    X:
    {
        
        if (fgets(input, MAXLINE, stdin) != NULL)
        {
            
            input[strcspn(input, "\n")] = '\0';

            
            if (strcmp(input, "exit") == 0)
            {
                exitClientFromCode(socketFdConnection);
            }
            else if (write(socketFdConnection, input, strlen(input)) < 0) {
                perror("write error");
                exit(1);
            }

            
            
        }
        goto X;
    }

}

void *readFromServerWriteToUser(void *arg)
{
    int socketFdConnection = *(int *)arg;
    ssize_t nread;
    char bufferValue[MAXLINE];

    Y:
    {
        nread = recv(socketFdConnection, bufferValue, MAXLINE, 0);

        if (nread < 0)
        {
            perror("recv error");
            exit(1);
        }
        else if (nread == 0)
        {
            printf("Server closed the connection\n");
            exit(0);
        }
        else
        {
            bufferValue[nread] = '\0';
            printf("%s\n", bufferValue);
            fflush(stdout);
        }
        goto Y;
    }
}



int createSocket(int domain, int type, int protocol)
{
    int socketFdConnection;
    if ((socketFdConnection = socket(domain, type, protocol)) < 0)
        fprintf(stderr, "socket error");
    return socketFdConnection;
}

void connectToServer(int socketFdConnection, char *ip_address, int port)
{
    struct addrinfo hints, *res, *p;
    int status;
    char port_str[6];
    sprintf(port_str, "%d", port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(ip_address, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if (connect(socketFdConnection, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect error");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "failed to connect\n");
        exit(1);
    }

    freeaddrinfo(res);
}


void createClientThread(pthread_t *thread, int socketFdConnection)
{
    if (pthread_create(thread, NULL, readClinetAndWriteInServer, &socketFdConnection) != 0)
        fprintf(stderr, "pthread_create error");
}

void createSocketThread(pthread_t *thread, int socketFdConnection)
{
    if (pthread_create(thread, NULL, readFromServerWriteToUser, &socketFdConnection) != 0)
        fprintf(stderr, "pthread_create error");
}

void joinThread(pthread_t thread)
{
    if (pthread_join(thread, NULL) != 0)
        fprintf(stderr, "pthread_join error");
}

void closeSocket(int socketFdConnection)
{
    if (close(socketFdConnection) != 0)
        fprintf(stderr, "close error");
}


int main(int argc, char **argv)
{
    int socketFdConnection;
    pthread_t user_thread, socket_thread;

    
    socketFdConnection = createSocket(AF_INET, SOCK_STREAM, 0);
    
    connectToServer(socketFdConnection, "127.0.0.1", 1234);
    createClientThread(&user_thread, socketFdConnection);
    createSocketThread(&socket_thread, socketFdConnection);
    joinThread(user_thread);
    joinThread(socket_thread);

    closeSocket(socketFdConnection);
    return 0;
}

