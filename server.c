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
    char name[50];
    int connfd;
    int logged_in;
} Client;

Client clients[FD_SETSIZE];
int listenfd, connfd, maxfd, maxi = -1;


void handle_sigint(int sig)
{
    printf("SIGINT! Closing all the connections\n");
     for (int i = 0; i <= maxi; i++)
    {
        if (clients[i].connfd > 0)
        {
            close(clients[i].connfd);
        }
    }

    
    close(listenfd);

   
    exit(0);
}



char **split_string(char *str, char *delimiter, int *num_tokens)
{
    char **tokens = malloc(MAX_TOKENS * sizeof(char *));
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

char *remove_first_char(char *str)
{
    size_t len = strlen(str);
    char *new_str = (char *)malloc(len);
    if (new_str != NULL)
    {
        memcpy(new_str, str + 1, len);
    }
    return new_str;
}

void send_message(char *client_name, char *message, int dest_fd)
{
    int i, is_broadcast = 0;
    if (strcmp(client_name, "-1") == 0)
        is_broadcast = 1;
    
    fflush(stdout);

    for (i = 0; i <= maxi; i++)
    {
        if (clients[i].connfd >= 0 && ((is_broadcast && clients[i].logged_in) || strcmp(clients[i].name, client_name) == 0 || clients[i].connfd == dest_fd))
        {
            int bytes_sent = send(clients[i].connfd, message, strlen(message), 0);
            if (bytes_sent == -1)
            {
                perror("Error sending message");
                
            }
            else
            {
                printf("Sent %d bytes to client %s\n", bytes_sent, clients[i].name);
                fflush(stdout);
            }
        }
    }
}

void interpret_request(int ind, char *message)
{

    char delimiter[] = " ";
    int num_tokens;
    char *new_message = (char *)malloc(strlen(message) + 1);
    strcpy(new_message, message);
    char **tokens = split_string(message, delimiter, &num_tokens);
    

    if (strcmp(tokens[0], "login") == 0)
    {
        if (clients[ind].logged_in == 1)
        {
            
        }
        else
        {
            strcpy(clients[ind].name, tokens[1]);
            clients[ind].logged_in = 1;
            
        }
        printf("Loggedin\n");
       
       
    }
    else if (strcmp(tokens[0], "logout") == 0)
    {

        if (clients[ind].logged_in == 0)
        {
           
        }
        else
        {
            clients[ind].logged_in = 0;
           
        }
        printf("Loggedout\n");
    }
    else if (strcmp(tokens[0], "chat") == 0)
    {

        if (clients[ind].logged_in == 0)
        {
            printf("Not logged in\n");
            
        }
        else
        {

            if (tokens[1][0] == '@')
                send_message(remove_first_char(tokens[1]), new_message, -1);
            else
                send_message("-1", new_message, -1);
        }
    }
    else
    {
       
       printf("wrong command\n");
       
    }
}

int bind_and_listen(int port)
{
    int listenfd;
    struct sockaddr_in servaddr, local_addr;
    char hostname[1024];
    struct hostent *he;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(listenfd, LISTENQ);
    gethostname(hostname, 1024);
    printf("here ir is : %s",hostname);
    he = gethostbyname(hostname);

    socklen_t local_addr_len = sizeof(local_addr);
    getsockname(listenfd, (struct sockaddr *)&local_addr, &local_addr_len);
    printf("Hostname: %s\n", he->h_name);
    printf("IP address: %s\n", inet_ntoa(*((struct in_addr *)he->h_addr)));
    printf("Assigned port number: %d\n", ntohs(local_addr.sin_port));
    return listenfd;
}

int main(int argc, char **argv)
{

    int i;
    struct sockaddr_in cliaddr;
    socklen_t clilen;

    fd_set allset, rset;
    FD_ZERO(&allset);
    signal(SIGINT, handle_sigint);

    

    listenfd = bind_and_listen(1234);

    FD_SET(listenfd, &allset);
    maxfd = listenfd;

    for (i = 0; i < FD_SETSIZE; i++)
    {
        clients[i].connfd = -1;
        clients[i].logged_in = 0;
    }

    for (;;)
    {
        rset = allset;
        // should return something
        select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &rset))
        {
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

            for (i = 0; i < FD_SETSIZE; i++)
            {
                if (clients[i].connfd < 0)
                {
                    clients[i].connfd = connfd;
                    break;
                }
            }

            if (i == FD_SETSIZE)
            {
                fprintf(stderr, "too many clients\n");
                exit(1);
            }

            FD_SET(connfd, &allset);
            if (connfd > maxfd)
            {
                maxfd = connfd;
            }

            if (i > maxi)
            {
                maxi = i;
            }

            printf("new client: %s, port %d, maxi is %d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), maxi);
        }

        for (i = 0; i <= maxi; i++)
        {

            if ((connfd = clients[i].connfd) < 0)
            {
                continue;
            }

            if (FD_ISSET(connfd, &rset))
            {
                ssize_t n;
                char buff[MAXLINE];
                memset(buff, 0, MAXLINE);

                if ((n = read(connfd, buff, MAXLINE)) == 0)
                {
                    printf("client closed connection: %s, port %d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

                    FD_CLR(connfd, &allset);
                    clients[i].connfd = -1;
                    clients[i].logged_in = 0;
                    strcpy(clients[i].name, "");
                    close(connfd);
                }
                else
                {

                    interpret_request(i, buff);
                    fflush(stdout);
                }
            }
        }
    }
    return 0;
}