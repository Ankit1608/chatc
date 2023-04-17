#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>

#define MAXLINE 1000
#define MAX_TOKENS 1000
#define LISTENQ 1024

typedef struct
{
    int connectionFileDescriptor;
    char name[50];
    int signnedIn;
} User;

User clients[FD_SETSIZE];
int listenfd, connectionFileDescriptor, maxfd, maxi = -1;


void handleTerminationSignal(int sig)
{
    printf("SIGINT! Closing all the connections\n");
    int i = 0;
    while (i <= maxi) {
        if (clients[i].connectionFileDescriptor > 0) {
            close(clients[i].connectionFileDescriptor);
        }
        i++;
    }
    close(listenfd);
    exit(0);
}

char **createTokenBuffer(void)
{
    return malloc(MAX_TOKENS * sizeof(char *));
}

char **parseCommand(char *str, char *delimiter, int *num_tokens)
{
    char **tokens = createTokenBuffer();
    char *token;
    int i = 0;

    token = strtok(str, delimiter);
    while (token != NULL && i < MAX_TOKENS)
    {
        tokens[i++] = strdup(token);
        token = strtok(NULL, delimiter);
    }
    *num_tokens = i;
    return tokens;
}


char *shiftLeft(char *str)
{
    size_t len = strlen(str);
    char *new_str = malloc(len);
    if (new_str != NULL)
    {
        memcpy(new_str, str + 1, len);
        new_str[len - 1] = '\0';
    }
    return new_str;
}

void sendBroadCast(char *message) {
    int i;
    fflush(stdout);
    for (i = 0; i <= maxi; i++) {
        if (clients[i].connectionFileDescriptor >= 0 && clients[i].signnedIn) {
            int bytes_sent = send(clients[i].connectionFileDescriptor, message, strlen(message), 0);
            if (bytes_sent == -1) {
                perror("Error sending broadcast message");
            } else {
                printf("%d bytes sent to client %s\n", bytes_sent, clients[i].name);
                fflush(stdout);
            }
        }
    }
}

void sendDM(char *client_name, char *message) {
    int i;
    fflush(stdout);
    for (i = 0; i <= maxi; i++) {
        if (clients[i].connectionFileDescriptor >= 0 && strcmp(clients[i].name, client_name) == 0) {
            int bytes_sent = send(clients[i].connectionFileDescriptor, message, strlen(message), 0);
            if (bytes_sent == -1) {
                perror("Error sending direct message");
            } else {
                printf("%d bytes sent to client %s\n", bytes_sent, clients[i].name);
                fflush(stdout);
            }
        }
    }
}

void sendM(char *client_name, char *message, int dest_fd) {
    if (strcmp(client_name, "-1") == 0) {
        sendBroadCast(message);
    } else if (dest_fd > 0) {
        int bytes_sent = send(dest_fd, message, strlen(message), 0);
        if (bytes_sent == -1) {
            perror("Error sending direct message");
        } else {
            printf("%d bytes sent to client with %d\n", bytes_sent, dest_fd);
            fflush(stdout);
        }
    } else {
        sendDM(client_name, message);
    }
}

void login(int ind, char **tokens) {
    if (clients[ind].signnedIn == 1) {
        // do nothing if already logged in
    } else {
        strcpy(clients[ind].name, tokens[1]);
        clients[ind].signnedIn = 1;
    }
    printf("Signedin\n");
}

void logout(int ind) {
    if (clients[ind].signnedIn == 0) {
        // do nothing if not logged in
    } else {
        clients[ind].signnedIn = 0;
    }
    printf("Signedin\n");
}

void processReq(int ind, char *message) {
    char delimiter[] = " ";
    int num_tokens;
    char *new_message = (char *)malloc(strlen(message) + 1);
    strcpy(new_message, message);
    char **tokens = parseCommand(message, delimiter, &num_tokens);

    if (strcmp(tokens[0], "login") == 0) {
        login(ind, tokens);
    } else if (strcmp(tokens[0], "logout") == 0) {
        logout(ind);
    } else if (strcmp(tokens[0], "chat") == 0) {
        if (clients[ind].signnedIn == 0) {
            printf("Not Signned in\n");
        } else {
            if (tokens[1][0] == '@')
                sendM(shiftLeft(tokens[1]), new_message, -1);
            else
                sendM("-1", new_message, -1);
        }
    } else {
        printf("Invalid Command\n");
    }
}


void configSocket(int sockfd)
{
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }
}

int setupServer(int port)
{
    int listenfd;
    struct sockaddr_in servaddr, local_addr;
    char hostname[1024];
    struct hostent *he;
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    
    configSocket(listenfd);
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(listenfd, LISTENQ) < 0)
    {
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }
    
    gethostname(hostname, 1024);
    printf("host is : %s",hostname);
    he = gethostbyname(hostname);

    socklen_t local_addr_len = sizeof(local_addr);
    getsockname(listenfd, (struct sockaddr *)&local_addr, &local_addr_len);
    printf("Host: %s\n", he->h_name);
    printf("IP: %s\n", inet_ntoa(*((struct in_addr *)he->h_addr)));
    printf("Port number: %d\n", ntohs(local_addr.sin_port));
    
    return listenfd;
}
int main(int argc, char **argv)
{

    int i;
    struct sockaddr_in cliaddr;
    socklen_t clilen;

    fd_set allset, rset;
    FD_ZERO(&allset);
    signal(SIGINT, handleTerminationSignal);

    listenfd = setupServer(1234);

    FD_SET(listenfd, &allset);
    maxfd = listenfd;

    for (i = 0; i < FD_SETSIZE; i++)
    {
        clients[i].connectionFileDescriptor = -1;
        clients[i].signnedIn = 0;
    }

    for (;;)
    {
        rset = allset;
        if (select(maxfd + 1, &rset, NULL, NULL, NULL) < 0) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(listenfd, &rset))
        {
            clilen = sizeof(cliaddr);
            connectionFileDescriptor = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

            for (i = 0; i < FD_SETSIZE; i++)
            {
                if (clients[i].connectionFileDescriptor < 0)
                {
                    clients[i].connectionFileDescriptor = connectionFileDescriptor;
                    break;
                }
            }

            if (i == FD_SETSIZE)
            {
                fprintf(stderr, "Maximum client limit reached.\n");
                exit(EXIT_FAILURE);
            }

            FD_SET(connectionFileDescriptor, &allset);
            if (connectionFileDescriptor > maxfd)
            {
                maxfd = connectionFileDescriptor;
            }

            if (i > maxi)
            {
                maxi = i;
            }

            printf("new client: %s, port %d, maxi is %d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), maxi);
        }

        for (i = 0; i <= maxi; i++)
        {

            if ((connectionFileDescriptor = clients[i].connectionFileDescriptor) < 0)
            {
                continue;
            }

            if (FD_ISSET(connectionFileDescriptor, &rset))
            {
                ssize_t n;
                char buff[MAXLINE];
                memset(buff, 0, MAXLINE);

                if ((n = read(connectionFileDescriptor, buff, MAXLINE)) == 0)
                {
                    printf("user closed connection: %s, port %d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

                    FD_CLR(connectionFileDescriptor, &allset);
                    clients[i].connectionFileDescriptor = -1;
                    clients[i].signnedIn = 0;
                    strcpy(clients[i].name, "");
                    close(connectionFileDescriptor);
                }
                else
                {
                    processReq(i, buff);
                    fflush(stdout);
                }
            }
        }
    }
}
