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
#include "threadpool.h"
#include <signal.h>

// Flag to maintain running status of the server
bool server_run = true;

// Store the global ssl context
SSL_CTX *g_ssl_ctx = NULL;

// List to store all active client connections
client_list clist;

/**
 * Signal handler to catch signals and
 * shutdown the server gracefully
 */
void signal_handler(int sig)
{
    LOG_INFO("Received %s signal. Initiating server shutdown...", strsignal(sig));
    server_run = false;
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
        LOG_ERROR("sigaction: SIGTERM");
        return -1;
    }
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        LOG_ERROR("sigaction: SIGINT");
        return -1;
    }
    if (sigaction(SIGHUP, &sa, NULL) == -1)
    {
        LOG_ERROR("sigaction: SIGHUP");
        return -1;
    }
    if (sigaction(SIGQUIT, &sa, NULL) == -1)
    {
        LOG_ERROR("sigaction: SIGHUP");
        return -1;
    }

    LOG_INFO("Signal Handler Registration complete");
    return 0;
}

/**
 * Function to be called at exit to perform cleanup
 */
void cleanup_server()
{
    release_cache();
    stop_logging();
    stop_threadpool();
    cleanup_client_list();

    if (g_ssl_ctx != NULL)
        SSL_CTX_free(g_ssl_ctx);
}

/**
 * Initialize structs for secured socket communication
 * Setup the cert and key files for authentication
 */
int init_openssl_context(const char *cert_file, const char *key_file)
{
    if (access(cert_file, F_OK | R_OK) != 0)
    {
        LOG_ERROR("ssl_cert_file: %s cannot be accessed\n", cert_file);
        return -1;
    }

    if (access(key_file, F_OK | R_OK) != 0)
    {
        LOG_ERROR("ssl_key_file: %s cannot be accessed\n", key_file);
        return -1;
    }

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    g_ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (g_ssl_ctx == NULL)
    {
        ERR_print_errors_cb(ssl_log_err, NULL);
        return -1;
    }

    // Load certificate and private key for authentication
    LOG_INFO("SSL using cert file %s", cert_file);
    if (SSL_CTX_use_certificate_file(g_ssl_ctx, cert_file, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_cb(ssl_log_err, NULL);
        return -1;
    }

    LOG_INFO("SSL using private key file %s", key_file);
    if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, key_file, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_cb(ssl_log_err, NULL);
        return -1;
    }

    LOG_INFO("SSL Initialisation Complete");
    return 0;
}


/**
 * Worker function to accept and serve HTTPS Requests 
 */
void run_https_server(int server_fd)
{
    ssize_t nfds = 0;
    ssize_t curr = 0;
    int client_fd = 0;
    int epoll_fd = 0;
    SSL *client_ssl = NULL;
    unsigned int curr_event = 0;
    struct epoll_event ev = {0};
    struct epoll_event events[MAX_ALIVE_CONN] = {{0}};

    // Setup epoll to track incoming connection on server port
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        LOG_ERROR("%s epoll_create1", __func__);
        return;
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1)
    {
        LOG_ERROR("%s epoll_ctl server_fd", __func__);
        close(epoll_fd);
        return;
    }

    LOG_INFO("epoll listening for edge triggers on %d", epoll_fd);

    // Keep server alive until running status is true
    while (server_run)
    {
        // Wait for incoming activity on all the ports being tracked
        nfds = epoll_wait(epoll_fd, events, MAX_ALIVE_CONN, -1);
        if (nfds == -1)
        {
            // If wait didn't exit due to interrupt signal
            // Then error happened, in any case exit the loop
            if (errno != EINTR)
                LOG_ERROR("%s epoll_wait", __func__);
            server_run = false;
            break;
        }

        // Iterate through the list of sockets which triggered an event
        for (curr = 0; curr < nfds; curr++)
        {
            // New event on server_fd means incoming connection
            if (events[curr].data.fd == server_fd)
            {
                // Accept new connection and add it to epoll
                if (accept_connections(server_fd, epoll_fd) == -1)
                {
                    server_run = false;
                    break;
                }
                continue;
            }

            curr_event = events[curr].events;
            client_ssl = (SSL *)events[curr].data.ptr;
            client_fd = SSL_get_fd(client_ssl);
            if (curr_event && POLL_IN)
            {
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                add_task_to_queue(handle_http_request, client_ssl);
            }
        }
    }
    // Close epoll file descriptor and exit
    close(epoll_fd);
}


int main(int argc, char *argv[])
{
    int opt = 0;
    int server_fd = 0;
    bool is_daemon_mode = false;
    char *server_ip = SERVER_IP_ADDR;
    char *server_port = SERVER_PORT;
    char *assets_dir = DEFAULT_ASSET_PATH;
    char *ssl_key_file = DEFAULT_SSL_KEY_FILE;
    char *ssl_cert_file = DEFAULT_SSL_CERT_FILE;

    while ((opt = getopt(argc, argv, "c:k:i:p:a:d:")) != -1)
    {
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
        case 'a':
            assets_dir = optarg;
            break;
        case 'd':
            is_daemon_mode = true;
            break;
        default:
            fprintf(stderr, "Usage: %s [-c cert.pem] [-k key.pem] [-i <ip addr>] [-p <port>] [-a <asset folder>]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (is_daemon_mode && daemon(1, 0) != 0)
    {
        fprintf(stderr, "Switch to daemon mode failed. Exiting ...\n");
        return EXIT_FAILURE;
    }

    if (signal_setup() != 0)
        return EXIT_FAILURE;

    if (atexit(cleanup_server) != 0)
        return EXIT_FAILURE;

    if (init_logging() != 0)
        return EXIT_FAILURE;

    if (init_openssl_context(ssl_cert_file, ssl_key_file) != 0)
        return EXIT_FAILURE;

    if (initiate_cache(assets_dir) == 0)
        return EXIT_FAILURE;

    if(init_threadpool() != 0)
        return EXIT_FAILURE;

    init_client_list();
    // Initiate the server using the parsed input
    server_fd = initiate_server(server_ip, server_port);
    if (server_fd < 0)
        return EXIT_FAILURE;

    run_https_server(server_fd);

    sleep(1);
    close(server_fd);

    return EXIT_SUCCESS;
}

/**
 * To do list,

 * Create a web portfolio 
 * Support sending compressed files
 * Support sending of non html files
 * Add rate limiting, close client sockets after timeout,

 */
