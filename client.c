#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

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
    info->protocol = HTTP; // default ako se ne specificira da li je http ili https
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
    
    struct addrinfo hints, *result, *p; // result pokazuje na lancanu listu, p je za iteraciju, hints daje specifikacije za hostname resolving
    int client_socket; // file deskriptor za socket

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family = AF_INET; // ipv4 tip adrese

    int s = getaddrinfo(info->hostname, info->protocol == HTTPS ? "443" : "80", &hints, &result); // koristi se port 443 za https i 80 za http
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
    // ako je lista iscrpljena
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
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n"
        "Accept-Language: en-US,en;q=0.9\r\n"
        "Connection: close\r\n\r\n", info->path, info->hostname);

    char response[1024] = {0};
    ssize_t bytes_received;

    // ============================== HTTP ========================================
    if(info->protocol == HTTP){
        // slanje http GET request-a
        send(client_socket, request, strlen(request), 0);
        // citanje odgovora
        while ((bytes_received = recv(client_socket, response, sizeof(response) - 1, 0)) > 0) {
            response[bytes_received] = '\0';
            printf("%s", response);
            memset(response, 0, sizeof(response));
        }
        printf("\n");
        close(client_socket);
        return;
    // ============================== HTTPS ========================================
    }else if(info->protocol == HTTPS){
        // inicijalizacija
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();

        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_socket);
        SSL_set_tlsext_host_name(ssl, info->hostname);

        // ssl handshake
        if (SSL_connect(ssl) != 1) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(client_socket);
            exit(EXIT_FAILURE);
        }
        // slanje http GET request-a
        if (SSL_write(ssl, request, strlen(request)) <= 0) {
            ERR_print_errors_fp(stderr);
        }
        // citanje odgovora
        while ((bytes_received = SSL_read(ssl, response, sizeof(response) - 1)) > 0) {
            response[bytes_received] = '\0';
            printf("%s", response);
        }
        printf("\n");

        // cleanup
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(client_socket);
    }
    
}

// ==========================================
// 1. ocita se url i njegovi delovi
// 2. kreira se socket (file descriptor) i uspostavi konekcija sa serverom
// 3. posalje se http(https) GET zahtev sa odgovarajucim podacima
// 4. primljeni podaci se ocitavaju iz socket buffera pomocu recv() za http odnosno SSL_read() za https
// ==========================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ./client <url>\n");
        exit(EXIT_FAILURE);
    }

    url_info* info = parse_url(argv[1]);
    // printf("path: %s\nhostname: %s\n", info->path, info->hostname);
    int client_socket = establish_connection(info);
    http_get(client_socket, info);

    return 0;
}
