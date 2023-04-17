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
    int connFdConnection;
    int authenticated;
} Client;

typedef enum {
    LOGIN,
    LOGOUT,
    CHAT,
    INVALID_COMMAND
} Command_type;

Client clients[FD_SETSIZE];
int listeningFdConnection, connFdConnection, maxFdConnections;
int max = -1;


void handlingSigintClosingConnections(int sig)
{
    printf("trying to close connections\n");
     for (int i = 0; i <= max; i++)
    {
        if (clients[i].connFdConnection > 0)
        {
            close(clients[i].connFdConnection);
        }
    }
    close(listeningFdConnection);
    exit(0);
}




char *RemoveFirstCharacterFromString(char *str)
{
    if (str == NULL || str[0] == '\0') {
        return str;
    }
    int len = strlen(str);
    memmove(str, str+1, len); 
    str[len-1] = '\0';
    return str;
}

int checkClientMessageValidations(int broadcast,int pointer,char *clientName, int destFdConnection)
{
    
    if(broadcast==1 && clients[pointer].authenticated)
    {
        return 1;
    }
    if(strcmp(clients[pointer].name, clientName) == 0)
    {
        return 1;
    }
    if(clients[pointer].connFdConnection == destFdConnection)
    {
        return 1;
    }
    return 0;

}

void sendMessageToSpecificClient(char *clientName, char *message, int dest_fd)
{
     int broadCast = 0;
    
   
    if (strcmp(clientName, "broadcast") == 0)
        broadCast = 1;
    
    fflush(stdout);

    int pointer =0;

    Y:
    {
        
        if (clients[pointer].connFdConnection >= 0 && checkClientMessageValidations(broadCast,pointer,clientName,dest_fd) ==1)
        {
            int bytesSent = send(clients[pointer].connFdConnection, message, strlen(message), 0);
            if (bytesSent == -1)
            {
                perror("Error sending message");
                
            }
            else
            {
                fflush(stdout);
            }
        }
        if(pointer<=max)
        {
            pointer=pointer+1;
            goto Y;
        }
        
    }
}

int getCommandType(char* command) {
    if (strcmp(command, "login") == 0) {
        return LOGIN;
    }
    else if (strcmp(command, "logout") == 0) {
        return LOGOUT;
    }
    else if (strcmp(command, "chat") == 0) {
        return CHAT;
    }
    else {
        return INVALID_COMMAND;
    }
}






void decodeRequestRecieved(int ind, char *message)
{
    char *newMessage = (char *)malloc(strlen(message) + 1);
    int numTokens;
    
    strcpy(newMessage, message);
    

    char** tokens = NULL;
    char* token = strtok(message, " "); 
    int count = 0;
    while (token != NULL) {
        tokens = realloc(tokens, sizeof(char*) * (count + 1)); 
        tokens[count] = token; 
        count++;
        token = strtok(NULL, " "); 
    }
    numTokens = count;


    switch (getCommandType(tokens[0])) {
        case LOGIN:
            if (clients[ind].authenticated == 1)
            {
               
                sendMessageToSpecificClient(clients[ind].name,"Already Logged in",-1);
            }
            else
            {
                strcpy(clients[ind].name, tokens[1]);
                clients[ind].authenticated = 1;
                printf("Loggedin\n");
            }
            
            break;
        case LOGOUT:
            if (clients[ind].authenticated == 0)
            {
                
                sendMessageToSpecificClient(clients[ind].name,"Should login to logout",-1);
            }
            else
            {
                clients[ind].authenticated = 0;
                printf("Loggedout\n");
            }
            
            break;
        case CHAT:
            if (clients[ind].authenticated == 0)
            {
                printf("Not logged in\n");
            }
            else
            {
                if (tokens[1][0] == '@')
                    sendMessageToSpecificClient(RemoveFirstCharacterFromString(tokens[1]), newMessage, -1);
                else
                    sendMessageToSpecificClient("broadcast", newMessage, -1);
            }
            break;
        default:
            printf("wrong command\n");
    }
}


int bindAndListenServer(int port) {
    int listeningFdConnection = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningFdConnection < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(listeningFdConnection, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        close(listeningFdConnection);
        return -1;
    }

    if (listen(listeningFdConnection, LISTENQ) < 0) {
        perror("listen");
        close(listeningFdConnection);
        return -1;
    }

    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        perror("gethostname");
        close(listeningFdConnection);
        return -1;
    }

    struct hostent *he = gethostbyname("localhost");
    if (!he) {
        perror("gethostbyname");
        close(listeningFdConnection);
        return -1;
    }

    struct sockaddr_in local_addr = {0};
    socklen_t local_addr_len = sizeof(local_addr);
    if (getsockname(listeningFdConnection, (struct sockaddr *)&local_addr, &local_addr_len) < 0) {
        perror("getsockname");
        close(listeningFdConnection);
        return -1;
    }

    printf("Hostname: %s\n", he->h_name);
    printf("IP address: %s\n", inet_ntoa(*((struct in_addr *)he->h_addr)));
    printf("Assigned port number: %d\n", ntohs(local_addr.sin_port));

    return listeningFdConnection;
}




int main()
{

    int i;
    struct sockaddr_in cliaddr;
    socklen_t clilen;

    fd_set allset, rset;
    FD_ZERO(&allset);
    signal(SIGINT, handlingSigintClosingConnections);

    char line[MAXLINE];
    char *key, *value;
    char *delim = ":";
    FILE *file = fopen("chat_server_configuration_file", "r");
    if (file == NULL) {
        printf("Failed to open file.\n");
        exit(1);
    }
    char *keys[1];
    char *values[1];
    int count = 0;
    int servPort;
    while (fgets(line, MAXLINE, file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        key = strtok(line, delim);
        value = strtok(NULL, delim);
        
        
        if (key != NULL && value != NULL) {
            keys[count] = strdup(key);
            values[count] = strdup(value);
            count++;
        }
    }

    servPort = (int)strtol(values[0], (char **)NULL, 10);
    

    listeningFdConnection = bindAndListenServer(servPort);

    FD_SET(listeningFdConnection, &allset);
    maxFdConnections = listeningFdConnection;

    for (i = 0; i < FD_SETSIZE; i++)
    {
        clients[i].connFdConnection = -1;
        clients[i].authenticated = 0;
    }

    Z:
    {
        int one=1;
        
        rset = allset;
        // should return something
        select(maxFdConnections + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listeningFdConnection, &rset)*one)
        {
            clilen = sizeof(cliaddr);
            connFdConnection = accept(listeningFdConnection, (struct sockaddr *)&cliaddr, &clilen);

            for (i = 0; i < (FD_SETSIZE * one); i++)
            {
                if (clients[i].connFdConnection < 0)
                {
                    clients[i].connFdConnection = connFdConnection;
                    break;
                }
            }

            if (i == (FD_SETSIZE*one))
            {
                fprintf(stderr, "too many clients\n");
                exit(1);
            }

            FD_SET(connFdConnection, &allset);
            if (connFdConnection > maxFdConnections)
            {
                maxFdConnections = connFdConnection;
            }

            if (i > max)
            {
                max = i;
            }

            printf("new client has been connected: %s, port %d, clientNumber is %d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), max);
        }

        for (i = 0; i <= (max*one); i++)
        {

            if ((connFdConnection = clients[i].connFdConnection) < 0 && one)
            {
                continue;
            }

            if (FD_ISSET(connFdConnection, &rset) && one)
            {
                ssize_t n;
                char buff[MAXLINE];
                memset(buff, 0, MAXLINE);

                if ((n = read(connFdConnection, buff, MAXLINE)) == 0)
                {
                    printf("client connection has been closed: %s, port %d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

                    FD_CLR(connFdConnection, &allset);
                    clients[i].connFdConnection = -1;
                    clients[i].authenticated = 0;
                    strcpy(clients[i].name, "");
                    close(connFdConnection);
                }
                else
                {

                    decodeRequestRecieved(i, buff);
                    fflush(stdout);
                }
            }
        }
        goto Z;
    }

    return 0;
}
