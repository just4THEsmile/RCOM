/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>

#include <string.h>

#define DEFAULT_USER "rcom"
#define DEFAULT_PASS "rcom"

#define FTP_DEFAULT_PORT    21

//server response codes
#define FTP_SERVICE_GOING_TO_OPEN_DC 150
#define FTP_SERVICE_READY_FOR_USER 220
#define FTP_USER_SUCCESS_LOGGED_IN 230
#define FTP_CLOSE 221
#define FTP_PASSIVE_MODE 227
#define FTP_TRANSFER_COMPLETE 226
#define FTP_SERVICE_READY_FOR_PASS 331

#define SERVER_PORT 21


typedef struct {
    char username[50];
    char password[50];
    char host[100];
    char path[100];
    char filename[100];
    char ip[100];
} ftp_url;

enum state {
    START,
    SINGLE,
    MULTI,
    END
};

int connect_to_server(const char* ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}
int read_response(int socket,char* buf){

    memset(buf, 0, 300);
    enum state s = START;
    char byte;
    int i = 0,responseCode;

    while( s != END){
        read(socket, &byte, 1);
        switch (s)
        {
        case START:
            if(byte == ' '){
                s = SINGLE;
            }
            else if(byte == '-'){
                s = MULTI;
            }
            else if(byte == '\n'){
                s = END;
            }else{
                buf[i] = byte;
                i++;
            }
            break;
        case SINGLE:
            if(byte == '\n'){
                s = END;
            }
            else {
                buf[i] = byte;
                i++;
            }

            break;
        case MULTI:
            if(byte == '\n'){
                i=0;
                memset(buf, 0, 300);
                s = START;
            }
            break;
        default:
            break;
        }
    }

    sscanf( buf,"%d", &responseCode);
    printf("%d\n",responseCode);
    return responseCode;

}

int passive(int socket,int *port,char *ip){
    int ip1,ip2,ip3,ip4,p1,p2;
    char buf[300];
    write(socket, "PASV\n", strlen("PASV\n"));
    if(read_response(socket,buf) != FTP_PASSIVE_MODE){
        printf("Error entering passive mode\n");
        return -1;
    }
    sscanf(buf, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1,&ip2,&ip3,&ip4,&p1,&p2);
    sprintf(ip, "%d.%d.%d.%d", ip1,ip2,ip3,ip4);
    *port=p1*256+p2;
    return 0;
}

int Auth(int socket, char* user,char* pass){
    char buf[300];
    char username_command[strlen(user) + 6];
    char password_command[strlen(pass) + 6];
    sprintf(username_command, "USER %s\n", user);
    sprintf(password_command, "PASS %s\n", pass);

    write(socket, username_command, strlen(username_command));
    if(read_response(socket,buf) != FTP_SERVICE_READY_FOR_PASS){
        printf("Error sending username\n");
        return -1;
    }

    write(socket, password_command, strlen(password_command));

    if(read_response(socket,buf) != FTP_USER_SUCCESS_LOGGED_IN){
        printf("Error sending password\n");
        return -1;
    }
    return 0;

}

int close_connec(int socket){
    char buf[300];
    write(socket, "QUIT\n", strlen("QUIT\n"));
    if(read_response(socket,buf) != FTP_CLOSE){
        printf("Error closing connection\n");
        return -1;
    }
    return 0;

}

int request_file(int socket,char* filename){
    char buf[300];
    char retr_command[strlen(filename) + 6];
    sprintf(retr_command, "RETR %s\n", filename);

    write(socket, retr_command, strlen(retr_command));

    if(read_response(socket,buf) != FTP_SERVICE_GOING_TO_OPEN_DC){
        printf("Error requesting file\n");
        return -1;
    }
    return 0;
}

int get_file(int socket, char *filename){
    printf("%s\n",filename);
    FILE *file=fopen(filename,"wb");
    if(file==NULL){
        printf("Error opening file\n");
        return -1;
    }
    char buf[300];
    int bytes=1;
    while(bytes>0){
        bytes=read(socket,buf,300);
        if(fwrite(buf,1,bytes,file)<0){
            printf("Error writing to file\n");
            return -1;
        }
    }
    fclose(file);
    return 0;

}

int parse_ftp_url(const char* url, ftp_url* result) {
    struct hostent *h;
    // Initialize the result struct
    memset(result, 0, sizeof(ftp_url));

    // Check if the URL contains a username and password
    if (strstr(url, "@") != NULL) {
        // The format of the URL is: ftp://username:password@host/path
        sscanf(url, "ftp://%49[^:]:%49[^@]@%99[^/]/%99[^\n]", 
            result->username, result->password, result->host, result->path);
    } else {
        // The format of the URL is: ftp://host/path
        sscanf(url, "ftp://%99[^/]/%99[^\n]", result->host, result->path);

        strncpy(result->username, DEFAULT_USER ,sizeof(result->username) - 1);
        strncpy(result->password, DEFAULT_PASS ,sizeof(result->password) - 1);

    }
        // Check if the path is the filename
    if (strchr(result->path, '/') == NULL) {
        strncpy(result->filename, result->path, sizeof(result->filename) - 1);

    }else{
        // Get the filename
        char *filename = strrchr(result->path, '/') + 1;
        strncpy(result->filename, filename, sizeof(result->filename) - 1);
    }

    if ((h = gethostbyname(result->host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    strncpy(result->ip, inet_ntoa(*((struct in_addr *) h->h_addr)),sizeof(result->ip) - 1);

    printf("Username: %s\n", result->username);
    printf("ip: %s\n", result->ip);
    printf("filename: %s\n", result->filename);
    printf("path: %s\n", result->path);

    return 0;
}






int main(int argc, char **argv) {
    if (argc > 2)
        printf("**** wrong usage try ./download ftp://[<user>:<password>@]<host>/<url-path> \n");

    int sockfd;
    ftp_url result;


    if(parse_ftp_url(argv[1], &result) != 0){
        printf("Error parsing url\n");
        return -1;
    };
    if((sockfd = connect_to_server(result.ip, FTP_DEFAULT_PORT)) < 0){
        printf("Error connecting to server\n");
        return -1;
    }
    char buf[300];
    if(read_response(sockfd,buf) != FTP_SERVICE_READY_FOR_USER){
        printf("Error connecting to server\n");
        return -1;
    }
    if(Auth(sockfd,result.username,result.password) != 0){
        printf("Error authenticating\n");
        return -1;
    }
    int port;
    char ip[100];
    if(passive(sockfd,&port,ip) != 0){
        printf("Error entering passive mode\n");
        return -1;
    }

    int sockfd2;
    if((sockfd2 = connect_to_server(ip, port)) < 0){
        printf("Error connecting to server\n");
        return -1;
    }

    printf("%s\n",result.filename);
    if(request_file(sockfd,result.path) != 0){
        printf("Error requesting file\n");
        return -1;
    }


    if(get_file(sockfd2,result.filename) != 0){
        printf("Error getting file\n");
        return -1;
    }
    if(read_response(sockfd,buf) != FTP_TRANSFER_COMPLETE){
        printf("Error getting file\n");
        return -1;
    }

    if(close_connec(sockfd) != 0){
        printf("Error closing connection\n");
        return -1;
    }
    
    return 0;

}


