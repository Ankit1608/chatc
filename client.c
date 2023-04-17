#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>
#include <errno.h>

#define MAXLINE 1000
#define LISTENQ 1024

void quitApplication(int sockfd)
{
    if (close(sockfd) == -1)
    {
        perror("Failed to close socket");
    }
    printf("Exiting...\n");
    exit(EXIT_SUCCESS);
}

struct sockaddr_in configureSockAddr(char *ip_add, int port)
{
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    struct hostent *he;
    if ((he = gethostbyname(ip_add)) == NULL)
    {
        fprintf(stderr, "gethostbyname error for %s", ip_add);
        exit(1);
    }

    memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);

    return servaddr;
}

void connectToTcpServer(int sockfd, char *ip_add, int port)
{
    struct sockaddr_in servaddr = configureSockAddr(ip_add, port);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect error");
        exit(1);
    }
}

void getUserInput(char *input)
{
    if (fgets(input, MAXLINE, stdin) != NULL)
    {
        input[strcspn(input, "\n")] = '\0';
    }
}

void *readUserWriteSock(void *arg)
{
    int sockfd = *(int *)arg;
    char input[MAXLINE];

    while (1)
    {
        getUserInput(input);
        char *new_input = (char *)malloc(strlen(input) + 1);
        strcpy(new_input, input);
        char *token = strtok(new_input, " ");
        if (token != NULL)
        {
            if (strcmp(token, "exit") == 0)
            {
                quitApplication(sockfd);
            }
            else
            {
                write(sockfd, input, strlen(input));
            }
        }
        free(new_input);
    }
}

void logMessage(char *message)
{
    fputs(message, stdout);
    fputs("\n", stdout);
    fflush(stdout);
}

void *readSockWriteUser(void *arg)
{
    int sockfd = *(int *)arg;
    int n;
    char recvline[MAXLINE + 1];

    while ((n = read(sockfd, recvline, MAXLINE)) > 0)
    {
        recvline[n] = '\0';
        logMessage(recvline);
    }

    switch (n)
    {
    case 0:
        printf("Server closed the connection\n");
        exit(0);
        break;
    default:
        perror("read error");
        exit(1);
        break;
    }
}

int main(int argc, char **argv)
{
    int sockfd;
    pthread_t tuser, tsocket;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockfd < 0 ? (perror("socket error"), exit(1)) : connectToTcpServer(sockfd, "127.0.0.1", 1234);
    pthread_create(&tuser, NULL, readUserWriteSock, &sockfd) != 0 ? (perror("pthread_create error"), exit(1)) : 0;
    pthread_create(&tsocket, NULL, readSockWriteUser, &sockfd) != 0 ? (perror("pthread_create error"), exit(1)) : 0;
    pthread_join(tuser, NULL) != 0 ? (perror("pthread_join error"), exit(1)) : 0;
    pthread_join(tsocket, NULL) != 0 ? (perror("pthread_join error"), exit(1)) : 0;

    // Close socket and exit program
    quitApplication(sockfd);
}
