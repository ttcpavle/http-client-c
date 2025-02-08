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

typedef struct url_info{
    char* url;
    char* protocol;
    char* hostname;
    char* path;
}url_info;

// extraktovanje hostname i path iz url-a
url_info* extract_hostname(const char* url) {
    url_info* info = (url_info*)malloc(sizeof(url_info));
    info->url = (char*)malloc(sizeof(char) * 1024);
    info->protocol = (char*)malloc(sizeof(char) * 10);
    info->hostname = (char*)malloc(sizeof(char) * 256);
    info->path = (char*)malloc(sizeof(char) * 1024);

    strcpy(info->url, url);
    char temp[2048];  
    strcpy(temp, url);
    strcpy(info->protocol, "http"); // default

    if (strncmp(temp, "https://", 8) == 0) {
        strcpy(info->protocol, "https");
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

// konvertovanje hostname u IP adresu
int resolve_hostname(const char* hostname, char* ip_address) {
    struct addrinfo hints, *result, *p; // result pokazuje na lancanu listu, p je za iteraciju

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family = AF_INET;

    int s = getaddrinfo(hostname, NULL, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return 1;
    }

    // iteracija kroz listu ip adresa (p pokazuje na trenutni node u listi)
    for (p = result; p != NULL; p = p->ai_next) {
        // zaustavljanje na prvu ipv4 adresu u listi
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* addr = (struct sockaddr_in*)p->ai_addr;
            // upisati pronadjenu ip adresu u ip_address string
            inet_ntop(AF_INET, &addr->sin_addr, ip_address, IPSTRLEN);
            break;
        } 
    }

    freeaddrinfo(result); // osloboditi memoriju
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ./client <url>\n");
        exit(EXIT_FAILURE);
    }

    char resolved_ip[IPSTRLEN];

    url_info* info = extract_hostname(argv[1]);
    if (resolve_hostname(info->hostname, resolved_ip) != 0) {
        perror("Couldn't resolve hostname\n");
        exit(EXIT_FAILURE);
    }

    printf("Connecting to: %s\nPath:    %s\nHostname: %s\n\n", resolved_ip, info->path, info->hostname);

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in remote_address;
    remote_address.sin_family = AF_INET; // ipv4
    remote_address.sin_port = htons(80); // port soketa
    inet_pton(AF_INET, resolved_ip, &remote_address.sin_addr); // upisi resolved ip u adresu soketa
    
    if (connect(client_socket, (struct sockaddr*)&remote_address, sizeof(remote_address)) == -1) {
        perror("Connection fail\n");
        exit(EXIT_FAILURE);
    }

    char request[4096];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n", info->path, info->hostname);
    char response[8192];

    send(client_socket, request, sizeof(request) - 1, 0);

    // dokle god se fetch-uju bajtovi, nastavlja se printovanje
    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, response, sizeof(response) - 1, 0)) > 0) {
        response[bytes_received] = '\0';
        printf("%s", response);
    }
    printf("\n");
    close(client_socket);
    return 0;
}
