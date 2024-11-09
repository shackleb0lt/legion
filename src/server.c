/**
 * MIT License
 *
 * Copyright (c) 2024 Aniruddha Kawade
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "server.h"
#include <signal.h>

// Flag to maintain running status of the server
bool is_run = true;

// Stores the web assets in a readily accessible cache
size_t g_cache_size = 0;
page_cache *g_cache = NULL;

// Store the global ssl context
SSL_CTX * g_ssl_ctx = NULL;

// List to store all active client connections
client_list clist;

/**
 * Signal handler to chanage stop and
 * shutdown the server gracefully
 */
void signal_handler(int sig)
{
    LOG("Received %s signal\nInitiating server shutdown...", strsignal(sig));
    is_run = false;
}

/**
 * Function to register above signal handler
 * Above function will be triggered whenever any
 * one of below signals is received
 */
int signal_setup()
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction: SIGTERM");
        return -1;
    }
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction: SIGINT");
        return -1;
    }
    if (sigaction(SIGHUP, &sa, NULL) == -1)
    {
        perror("sigaction: SIGHUP");
        return -1;
    }
    if (sigaction(SIGQUIT, &sa, NULL) == -1)
    {
        perror("sigaction: SIGHUP");
        return -1;
    }
    return 0;
}

/**
 * Initialize structs for secured socket communication
 * Setup the cert and key files for authentication
 */
int init_openssl_context()
{
    int ret = 0;
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD *method = TLS_server_method();
    g_ssl_ctx = SSL_CTX_new(method);
    if (g_ssl_ctx == NULL)
    {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // Load certificate and private key
    ret = SSL_CTX_use_certificate_file(g_ssl_ctx, "/home/qubit/cf_cert.pem", SSL_FILETYPE_PEM);
    if(ret != 1)
        goto openssl_error;

    ret = SSL_CTX_use_PrivateKey_file(g_ssl_ctx, "/home/qubit/cf_key.pem", SSL_FILETYPE_PEM);  
    if (ret != 1)
        goto openssl_error;

    return 0;
openssl_error:
    ERR_print_errors_fp(stderr);
    SSL_CTX_free(g_ssl_ctx);
    g_ssl_ctx = NULL;
    return -1;
}

void run_https_server(int server_fd)
{
    ssize_t nfds = 0;
    ssize_t curr = 0;
    int epoll_fd = 0;
    int ret = 0, curr_fd = 0;
    unsigned int curr_event = 0;
    struct epoll_event ev = {0};
    struct epoll_event events[MAX_ALIVE_CONN] = {{0}};

    // Setup epoll to track incoming connection on server port
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("epoll_create1");
        return;
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl: server_fd");
        close(epoll_fd);
        return;
    }

    LOG("epoll listening for edge triggers on %d", epoll_fd);
    // Keep server alive until running status is true
    while (is_run)
    {
        // Wait for incoming activity on all the ports being tracked
        nfds = epoll_wait(epoll_fd, events, MAX_ALIVE_CONN, -1);
        if (nfds == -1)
        {
            // If wait didn't exit due to interrupt signal
            // Then error happened, in any case exit the loop
            if (errno != EINTR)
                perror("epoll_wait");
            is_run = false;
            break;
        }
        LOG("epoll_wait returned nfds:%d", nfds);
        // If wait exited due to incoming connection or request
        // Then handle it below
        for (curr = 0; curr < nfds; curr++)
        {
            curr_event = events[curr].events;
            curr_fd = events[curr].data.fd;
            // New event on server_fd means incoming connection
            if (curr_fd == server_fd)
            {
                // Accept new connection and add it to epoll
                ret = accept_connections(server_fd, epoll_fd);
                if (ret == -1)
                    is_run = false;
                continue;
            }
            // On a client socket only if there is incoming request
            // then parse it and send response
            else if (curr_event & EPOLLIN)
            {
                ret = handle_http_request(curr_fd);
                if (ret == 0)
                    continue;
            }
            // Close the socket otherwise
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, curr_fd, NULL);
            remove_fd_from_list(curr_fd);
            LOG("Connection from client_fd:%d is now closed", curr_fd);
        }
    }
    close(epoll_fd);
}

int main(int argc, char *argv[])
{
    int opt = 0;
    size_t curr = 0;
    int server_fd = 0, cfd = 0;
    char * ssl_key_file = NULL;
    char * ssl_cert_file = NULL;
    char * server_ip = SERVER_IP_ADDR;
    char * server_port = SERVER_PORT;
    
    if (signal_setup() != 0)
        return EXIT_FAILURE;
    
    while (opt != -1)
    {
        opt = getopt(argc, argv, "c:k:i:p:");
        switch (opt)
        {
            case 'c':
                ssl_cert_file = optarg;
                break;
            case 'k':
                ssl_key_file = optarg;
                break;
            case 'i':
                server_ip = optarg;
                break;
            case 'p':
                server_port = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -c cert.pem -k key.pem [-i <ip addr>] [-p <port>]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Check that required arguments are provided
    if (ssl_cert_file == NULL || ssl_key_file == NULL) 
    {
        fprintf(stderr, "Error: -c and -k options are required\n");
        fprintf(stderr, "Usage: %s -c cert.pem -k key.pem [-i <ip addr>] [-p <port>]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if(access(ssl_cert_file, F_OK | R_OK) != 0)
    {
        fprintf(stderr, "Error: ssl cert file %s cannot be accessed\n", ssl_cert_file);
        return EXIT_FAILURE;
    }

    if(access(ssl_key_file, F_OK | R_OK) != 0)
    {
        fprintf(stderr, "Error: ssl key file %s cannot be accessed\n", ssl_key_file);
        return EXIT_FAILURE;
    }

    if (initiate_logging() != 0)
        return EXIT_FAILURE;

    LOG("Signal handler has been registered");
    LOG("Using cert file %s", ssl_cert_file);
    LOG("Using key file %s", ssl_key_file);

    // Initialise clist array with default values
    init_client_list();

    if(init_openssl_context() != 0)
        goto close_logs;

    // Build the cache from all files in assets folder
    // For fast access while sending response
    g_cache_size = initiate_cache(DEFAULT_ASSET_PATH);
    if (g_cache_size == 0)
        goto cleanup_ssl;
    LOG("File cache has been initialised");

    // Initiate the server using the parsed input
    server_fd = initiate_server(server_ip, server_port);
    if (server_fd < 0)
        goto cleanup_cache;

    run_https_server(server_fd);

    sleep(1);
    // Close all open client connections
    cleanup_client_list();
cleanup_server:
    // Close Server socket
    close(server_fd);
cleanup_cache:
    // Release the cache
    free_cache();
cleanup_ssl:
    SSL_CTX_free(g_ssl_ctx);
close_logs:
    shutdown_loggging();
    return 0;
}

/**
 * To do list,

 * Create a web portfolio
 * 
 * Support sending of non html files
 * 
 * Support sending compressed files
 * 
 * Implement hashtable based g_cache for get request of files.
 * 
 * Use libssl to support https request.
 * Spawn a separate thread for accepting clients.
 * Add rate limiting, close client sockets after timeout,

 */