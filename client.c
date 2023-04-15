#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAXLINE 1000
#define LISTENQ 1024

void exit_me(int sockfd) {
    if (close(sockfd) == -1) {
        perror("Failed to close socket");
    }
    printf("Exiting...\n");
    exit(EXIT_SUCCESS);
}
struct sockaddr_in set_sockaddr(char *ip_add, int port)
{
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_add, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "gethostbyname error for %s", ip_add);
    }

    return servaddr;
}

void tcp_connect(int sockfd, char *ip_add, int port)
{
    struct sockaddr_in servaddr = set_sockaddr(ip_add, port);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        exit(1);
    }
}

void read_user_input(char *input)
{
    if (fgets(input, MAXLINE, stdin) != NULL)
    {
        input[strcspn(input, "\n")] = '\0';
    }
}

void *read_user_write_socket(void *arg)
{
    int sockfd = *(int *)arg;
    char input[MAXLINE];

    while (1)
    {
        read_user_input(input);
        char *new_input = (char*) malloc(strlen(input) + 1);
        strcpy(new_input, input);
        char *token = strtok(new_input, " ");
        if (token != NULL)
        {
            if (strcmp(token, "exit") == 0)
            {
                exit_me(sockfd);
            }
            else
            {
                write(sockfd, input, strlen(input));
            }
        }
        free(new_input);
    }
}

void print_received_message(char *message) {
    fputs(message, stdout);
    fputs("\n", stdout);
    fflush(stdout);
}

void *read_socket_write_user(void *arg)
{
    int sockfd = *(int *)arg;
    int n;
    char recvline[MAXLINE + 1];

    while ((n = read(sockfd, recvline, MAXLINE)) > 0)
    {
        recvline[n] = '\0';
        print_received_message(recvline);
    }

    if (n == 0)
    {
        printf("Server closed the connection\n");
        exit(0);
    }
    else
    {
        perror("read error");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    int sockfd;
    pthread_t tuser, tsocket;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    // Connect to server
    tcp_connect(sockfd, "127.0.0.1", 1234);

    // Create threads for user input and server output
    if (pthread_create(&tuser, NULL, read_user_write_socket, &sockfd) != 0) {
        perror("pthread_create error");
        exit(1);
    }
    if (pthread_create(&tsocket, NULL, read_socket_write_user, &sockfd) != 0) {
        perror("pthread_create error");
        exit(1);
    }

    // Wait for threads to finish
    if (pthread_join(tuser, NULL) != 0) {
        perror("pthread_join error");
        exit(1);
    }
    if (pthread_join(tsocket, NULL) != 0) {
        perror("pthread_join error");
        exit(1);
    }

    // Close socket and exit program
    exit_me(sockfd);
}
