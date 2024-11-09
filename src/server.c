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

/**
 * Signal handler to catch signals and
 * shutdown the server gracefully
 */
void signal_handler(int sig)
{
    printf("Received %s signal\n Initiating server shutdown...\n", strsignal(sig));
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

int main(int argc, char *argv[])
{
    ssize_t nfds = 0, curr = 0;
    int server_fd = 0, epoll_fd = 0;
    int ret = 0, curr_fd = 0;
    client_list clist;
    unsigned int curr_event = 0;
    struct epoll_event ev = {0};
    struct epoll_event events[MAX_ALIVE_CONN] = {{0}};

    if (signal_setup() != 0)
        return EXIT_FAILURE;

    ret = initiate_logging();
    if (ret != 0)
        return EXIT_FAILURE;

    LOG("Signal handler has been registered");

    // Build the cache from all files in assets folder
    // For fast access while sending response
    g_cache_size = initiate_cache(DEFAULT_ASSET_PATH);
    if (g_cache_size == 0)
        goto close_logs;

    // Struct to keeep track of active client connections
    memset(clist.fd_list, -1, MAX_ALIVE_CONN * sizeof(int));
    clist.last_free = 0;

    // Initiate the server from default or
    // User provided IP address annd Port
    if (argc == 1)
        server_fd = initiate_server(SERVER_IP_ADDR, SERVER_PORT);
    else if (argc == 2)
        server_fd = initiate_server(argv[1], SERVER_PORT);
    else if (argc == 3)
        server_fd = initiate_server(argv[1], argv[2]);
    else
    {
        printf("Usage: %s [<ip_address>] [<port_num>]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Check if server launch succeeded
    if (server_fd < 0)
    {
        goto cleanup_cache;
    }

    // Setup epoll to track incoming connection on server port
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("epoll_create1");
        goto cleanup_server;
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl: server_fd");
        goto cleanup_epoll;
    }

    LOG("epoll is ready to listen for incoming connetion on epoll_fd: %d", epoll_fd);
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
                ret = accept_connections(server_fd, epoll_fd, &clist);
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
            remove_fd_from_list(&clist, curr_fd);
            close(curr_fd);
            LOG("Connection from client_fd:%d is now closed", curr_fd);
        }
    }

    sleep(1);
    // Close all open client connections
    for (curr = 0; curr < MAX_ALIVE_CONN; curr++)
    {
        if (clist.fd_list[curr] < 0)
            continue;
        close(clist.fd_list[curr]);
    }
cleanup_epoll:
    // Close epoll socket
    close(epoll_fd);
cleanup_server:
    // Close Server socket
    close(server_fd);
cleanup_cache:
    // Release the cache
    free_cache();
close_logs:
    shutdown_loggging();
    return 0;
}

/**
 * To do list,
 *
 * Write tests
 * Create a web portfolio
 * Support sending of non html files
 * Support sending compressed files
 * Maybe implement other HTTP methods.
 * Use libssl to support https request.
 * Spawn a separate thread for accepting clients.
 * Add rate limiting, close client sockets after timeout,
 * Implement hashtable based g_cache for get request of files.
 *
 */