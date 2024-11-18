#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE 4096


void cleanup_openssl()
{
    EVP_cleanup();
}

void ssl_connect(SSL *ssl)
{
    if (SSL_connect(ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void send_https_request(SSL *ssl, const char *hostname)
{
    const char * request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n";
    SSL_write(ssl, request, strlen(request));
}

void receive_https_response(SSL *ssl)
{
    char buffer[BUFFER_SIZE];
    int bytes;

    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
}

SSL_CTX * init_client()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    method = SSLv23_client_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL)
    {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

int connect_server(const char *ip, const int port, SSL_CTX *ctx)
{
    struct sockaddr_in server_addr = {0};

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

int main(int argc, char **argv)
{
    int port = 8080;
    int server_fd = 0;
    const char *ip = "127.0.0.1";
    SSL *ssl = NULL;
    SSL_CTX *ctx = init_client();
    int curr = 0;
    server_fd = connect_server(ip, port, ctx);
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, server_fd);
    ssl_connect(ssl);

    for(; curr < 10; curr++)
    {
        printf("%d\n", curr);
        send_https_request(ssl, ip);
        receive_https_response(ssl);
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(server_fd);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    return 0;
}


// Generate a test cache to load all of the files

// Test 1
// Connect to server and send a HEAD request
// ret = run_test_1(conn_fd, server_addr);
// Test 2
// Connect to server and send GET request for all file
// Test 3
// Send repeated get requests for a file without closing the socket
// Test 4
// Send repeated get requests over new connection
// Test 5
// Spawn multiple child processes and send GET requests