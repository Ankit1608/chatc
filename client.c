#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAXLINE 1000
#define LISTENQ 1024

void exitClientFromCode(int sockfd)
{
    close(sockfd);
    printf("exiting..\n");
    exit(0);
}

void tcpConnectionForClient(int sockfd, char *ip_add, int port) {
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_add, &servaddr.sin_addr) != 1) {
        fprintf(stderr, "Error: Invalid IP address\n");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("connect error");
        exit(EXIT_FAILURE);
    }
}
void *readClinetAndWriteInServer(void *arg)
{
    int sockfd = *(int *)arg;
    char input[MAXLINE];

    X:
    {
        
        if (fgets(input, MAXLINE, stdin) != NULL)
        {
            
            input[strcspn(input, "\n")] = '\0';

            
            if (write(sockfd, input, strlen(input)) < 0) {
                perror("write error");
                exit(1);
            }

            
            if (strcmp(input, "exit") == 0)
            {
                exitClientFromCode(sockfd);
            }
        }
        goto X;
    }

}

void *readFromServerWriteToUser(void *arg)
{
    int sockfd = *(int *)arg;
    ssize_t nread;
    char buf[MAXLINE];

    Y:
    {
        nread = recv(sockfd, buf, MAXLINE, 0);

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
            buf[nread] = '\0';
            printf("%s\n", buf);
            fflush(stdout);
        }
        goto Y;
    }
}



int createSocket(int domain, int type, int protocol)
{
    int sockfd;
    if ((sockfd = socket(domain, type, protocol)) < 0)
        fprintf(stderr, "socket error");
    return sockfd;
}

void connectToServer(int sockfd, char *ip_address, int port)
{
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_address, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "gethostbyname error for %s", ip_address);
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        exit(1);
    }
}

void createClientThread(pthread_t *thread, int sockfd)
{
    if (pthread_create(thread, NULL, readClinetAndWriteInServer, &sockfd) != 0)
        fprintf(stderr, "pthread_create error");
}

void createSocketThread(pthread_t *thread, int sockfd)
{
    if (pthread_create(thread, NULL, readFromServerWriteToUser, &sockfd) != 0)
        fprintf(stderr, "pthread_create error");
}

void joinThread(pthread_t thread)
{
    if (pthread_join(thread, NULL) != 0)
        fprintf(stderr, "pthread_join error");
}

void closeSocket(int sockfd)
{
    if (close(sockfd) != 0)
        fprintf(stderr, "close error");
}


int main(int argc, char **argv)
{
    int sockfd;
    pthread_t user_thread, socket_thread;

    
    sockfd = createSocket(AF_INET, SOCK_STREAM, 0);
    
    connectToServer(sockfd, "127.0.0.1", 1234);
    createClientThread(&user_thread, sockfd);
    createSocketThread(&socket_thread, sockfd);
    joinThread(user_thread);
    joinThread(socket_thread);

    closeSocket(sockfd);
    return 0;
}

