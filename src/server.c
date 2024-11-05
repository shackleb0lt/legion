#include "server.h"

bool is_run = true;

void signal_handler(int sig)
{
    printf("Received %s signal\nInitiating server shutdown...", strsignal(sig));
    is_run = false;
}

int signal_setup()
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    // Set up signal handling
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

    if (argc == 1)
        server_fd = initiate_server(NULL, SERVER_PORT);
    else if (argc == 2)
        server_fd = initiate_server(argv[1], SERVER_PORT);
    else if (argc == 3)
        server_fd = initiate_server(argv[1], argv[2]);
    else
    {
        printf("Usage: %s [<ip_address>] [<port_num>]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (server_fd < 0)
        return EXIT_FAILURE;

    memset(clist.fd_list, -1, MAX_ALIVE_CONN * sizeof(int));
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("epoll_create1");
        close(server_fd);
        return EXIT_FAILURE;
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl: server_fd");
        close(epoll_fd);
        close(server_fd);
        return EXIT_FAILURE;
    }

    while (is_run)
    {
        nfds = epoll_wait(epoll_fd, events, MAX_ALIVE_CONN, -1);
        if (nfds == -1)
        {
            perror("epoll_wait");
            is_run = false;
            break;
        }

        for (curr = 0; curr < nfds; curr++)
        {
            curr_event = events[curr].events;
            curr_fd = events[curr].data.fd;

            if (curr_fd == server_fd)
            {
                ret = accept_connections(server_fd, epoll_fd, &clist);
                if (ret == -1)
                {
                    is_run = false;
                    break;
                }
            }
            else if (curr_event & EPOLLIN)
            {
                ret = handle_http_request(curr_fd);
                if (ret == 0)
                    continue;
            }
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, curr_fd, NULL);
            remove_fd_from_list(&clist, curr_fd);
            close(curr_fd);
        }
    }

    sleep(2);

    for (curr = 0; curr < MAX_ALIVE_CONN; curr++)
    {
        if (clist.fd_list[curr] < 0)
            continue;
        close(clist.fd_list[curr]);
    }

    close(epoll_fd);
    close(server_fd);
    return 0;
}

/**
 * To do list,
 *
 * Spawn separate thread for accepting clients,
 * keep a lock for client list if queue is full sleep until socket is freed up.
 *
 * Implement hashtable based cache for get request of files.
 *
 */