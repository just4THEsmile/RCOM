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
#define SERVER_ADDR "193.137.29.15"

typedef struct {
    char username[50];
    char password[50];
    char host[100];
    char path[100];
    char filename[100];
    char ip[100];
} ftp_url;

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

        if ((h = gethostbyname(result->host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    strncpy(result->ip, inet_ntoa(*((struct in_addr *) h->h_addr)),sizeof(result->ip) - 1);

    return 0;
}






int main(int argc, char **argv) {
    if (argc > 1)
        printf("**** No arguments needed. They will be ignored. Carrying ON.\n");
    int sockfd;
    struct sockaddr_in server_addr;
    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    size_t bytes;
    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(SERVER_PORT);        /*server TCP port must be network byte ordered */
    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    printf("dca\n");
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    printf("dca\n");
    /*send a string to the server*/
    bytes = write(sockfd, buf, strlen(buf));
    

    if (bytes > 0)
        printf("Bytes escritos %ld\n", bytes);
    else {
        perror("write()");
        exit(-1);
    }

    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }

        const char* url1 = "ftp://username:password@netlab1.fe.up.pt/file.txt";
    const char* url2 = "ftp://netlab1.fe.up.pt/file.txt";
    ftp_url result;

    parse_ftp_url(url1, &result);
    printf("URL 1:\n");
    printf("username: %s\n", result.username);
    printf("password: %s\n", result.password);
    printf("host: %s\n", result.host);
    printf("path: %s\n", result.path);
    printf("filename: %s\n", result.filename);
    printf("ip: %s\n", result.ip);

    parse_ftp_url(url2, &result);
    printf("\nURL 2:\n");
    printf("username: %s\n", result.username);
    printf("password: %s\n", result.password);
    printf("host: %s\n", result.host);
    printf("path: %s\n", result.path);
    printf("filename: %s\n", result.filename);
    printf("ip: %s\n", result.ip);

    return 0;
}


