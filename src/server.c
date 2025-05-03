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

#define EPOLL_TIMEOUT_MS 1000

// Flag to maintain running status of the server
bool server_run = true;
int g_epoll_fd = -1;

/**
 * Signal handler to catch signals and
 * shutdown the server gracefully
 */
void signal_handler(int sig)
{
#ifndef DEBUG
    (void)sig;
#endif
    LOG_INFO("Received %s signal. Shutdown initiated.", strsignal(sig));
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
    stop_threadpool();
    release_cache();
    cleanup_client_list();
    stop_logging();
}

/**
 * Worker function to accept and serve HTTPS Requests
 */
void run_https_server(int server_fd)
{
    ssize_t nfds = 0;
    ssize_t curr = 0;
    client_info * cinfo = NULL;
    unsigned int curr_event = 0;
    struct epoll_event ev = {0};
    struct epoll_event events[MAX_ALIVE_CONN] = {{0}};

    // Setup epoll to track incoming connection on server port
    g_epoll_fd = epoll_create1(0);
    if (g_epoll_fd == -1)
    {
        LOG_ERROR("%s epoll_create1", __func__);
        return;
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1)
    {
        LOG_ERROR("%s epoll_ctl server_fd", __func__);
        close(g_epoll_fd);
        return;
    }

    LOG_INFO("epoll listening for edge triggers on %d", g_epoll_fd);

    // Keep server alive until running status is true
    while (server_run)
    {
        // Wait for incoming activity on all the ports being tracked
        nfds = epoll_wait(g_epoll_fd, events, MAX_ALIVE_CONN, EPOLL_TIMEOUT_MS);
        if (nfds == -1)
        {
            // If wait didn't exit due to interrupt signal
            // Then error happened, in any case exit the loop
            if (errno != EINTR)
            {
                LOG_ERROR("%s epoll_wait", __func__);
            }
            server_run = false;
            break;
        }

        else if(nfds == 0)
            continue;

        // Iterate through the list of sockets which triggered an event
        for (curr = 0; curr < nfds; curr++)
        {
            // New event on server_fd means incoming connection
            if (events[curr].data.fd == server_fd)
            {
                // Accept new connections then add them to epoll
                if (accept_connections(server_fd, g_epoll_fd) == -1)
                {
                    server_run = false;
                    break;
                }
                continue;
            }

            curr_event = events[curr].events;
            // epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, events[curr].data.fd, NULL);

            cinfo = get_client_info(events[curr].data.fd);
            if (cinfo == NULL)
            {
                continue;
            }
            else if (curr_event && POLL_IN)
            {
                add_task_to_queue(handle_http_request, cinfo);
            }
            else if (curr_event & (EPOLLHUP | EPOLLERR))
            {
                epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, events[curr].data.fd, NULL);
                remove_client_info(cinfo);
            }
        }
    }

    close(g_epoll_fd);
}

int main(int argc, char *argv[])
{
    int opt = 0;
    int server_fd = 0;
    bool is_daemon_mode = false;

    char *server_ip = NULL;
    char *server_port = NULL;
    char *assets_dir = DEFAULT_ASSET_PATH;

    socklen_t server_addr_len = 0;
    struct sockaddr_storage server_addr = {0};

    while ((opt = getopt(argc, argv, "c:k:i:p:a:d:")) != -1)
    {
        switch (opt) 
        {
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
                fprintf(stderr, "Usage: %s [-d] [-i <ip addr>] [-p <port>] [-a <asset folder>]\n", argv[0]);
                return EXIT_FAILURE;
            }
    }

    server_addr_len = check_ip_and_port(server_ip, server_port, (s_addr *)&server_addr);
    if (server_addr_len == 0)
        return EXIT_FAILURE;

    if (is_daemon_mode && daemon(1, 0) != 0)
    {
        fprintf(stderr, "Switch to daemon mode failed. Exiting ...\n");
        return EXIT_FAILURE;
    }

    if (signal_setup() != 0)
        return EXIT_FAILURE;

    if (set_fd_limit() != 0)
        return EXIT_FAILURE;

    if (atexit(cleanup_server) != 0)
        return EXIT_FAILURE;

    if (init_logging() != 0)
        return EXIT_FAILURE;

    if (initiate_cache(assets_dir) == 0)
        return EXIT_FAILURE;

    if (init_threadpool() != 0)
        return EXIT_FAILURE;

    init_client_list();

    server_fd = initiate_server((s_addr *)&server_addr, server_addr_len);
    if (server_fd < 0)
        return EXIT_FAILURE;

    run_https_server(server_fd);

    sleep(1);
    close(server_fd);

    return EXIT_SUCCESS;
}
