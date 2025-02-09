#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h> // za getaddrinfo()

// duzina stringa za ip adresu
#define IPSTRLEN INET_ADDRSTRLEN

enum Protocol { HTTP, HTTPS };

typedef struct url_info{
    char* url;
    enum Protocol protocol;
    char* hostname;
    char* path;
}url_info;

url_info* parse_url(const char* url);
int establish_connection(url_info* info);
void http_get(int client_socket, url_info* info);

// extraktovanje hostname i path iz url-a
url_info* parse_url(const char* url) {
    url_info* info = (url_info*)malloc(sizeof(url_info));
    info->url = (char*)malloc(sizeof(char) * 1024);
    info->protocol = HTTP;
    info->hostname = (char*)malloc(sizeof(char) * 256);
    info->path = (char*)malloc(sizeof(char) * 516);

    strcpy(info->url, url);
    char temp[2048];  
    strcpy(temp, url);

    if (strncmp(temp, "https://", 8) == 0) {
        info->protocol = HTTPS;
        memmove(temp, temp + 8, strlen(temp) - 7);
    } else if (strncmp(temp, "http://", 7) == 0) {        
        memmove(temp, temp + 7, strlen(temp) - 6);
    }

    // zamena prvog '/' sa 0. Sve pre toga je hostname, sve posle je path
    char* slash = strchr(temp, '/');
    if (slash) {
        *slash = '\0';
        strcpy(info->hostname, temp);
        *slash = '/';
        strcpy(info->path, slash);
    } else {
        strcpy(info->hostname, temp);
        strcpy(info->path, "/"); // ako nema path
    }

    return info;
}

int establish_connection(url_info* info){

    // result pokazuje na lancanu listu, p je za iteraciju
    struct addrinfo hints, *result, *p; 
    int client_socket; // file deskriptor za socket

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family = AF_INET;

    int s = getaddrinfo(info->hostname, "80", &hints, &result); // todo: promeniti da ne bude nuzno port 80
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // iteracija kroz listu ip adresa (p pokazuje na trenutni node u listi)
    for (p = result; p != NULL; p = p->ai_next) {

        client_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (client_socket == -1)
            continue;

        if (connect(client_socket, p->ai_addr, p->ai_addrlen) != -1)
            break;

    }

    if(p == NULL){
        freeaddrinfo(result); // osloboditi listu
        close(client_socket); // zatvaranje socketa
        perror("Could not connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);
    return client_socket;
}

void http_get(int client_socket, url_info* info) {
    char request[1024] = {0};
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n", info->path, info->hostname);

    send(client_socket, request, sizeof(request) - 1, 0);
}

// 1. ocita se url i njegovi delovi
// 2. kreira se socket (file descriptor) i uspostavi konekcija sa serverom
// 3. posalje se http GET zahtev sa odgovarajucim podacima
// 4. primljeni podaci se ocitavaju pomocu funkcije recv() iz socket buffera

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ./client <url>\n");
        exit(EXIT_FAILURE);
    }

    url_info* info = parse_url(argv[1]);
    printf("path: %s\nhostname: %s\n", info->path, info->hostname);
    int client_socket = establish_connection(info);
    http_get(client_socket, info);
    
    // dokle god se fetch-uju bajtovi, nastavlja se printovanje
    char response[1024];
    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, response, sizeof(response) - 1, 0)) > 0) {
        response[bytes_received] = '\0';
        printf("%s", response);
        memset(response, 0, sizeof(response));
    }
    printf("\n");
    close(client_socket);
    return 0;
}
