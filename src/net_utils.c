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

#include <sys/resource.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define S_ADDR4_SIZE sizeof(struct sockaddr_in)
#define S_ADDR6_SIZE sizeof(struct sockaddr_in6)

typedef struct sockaddr_in s_addr4;
typedef struct sockaddr_in6 s_addr6;

/**
 * Function that limits the total open file descriptor
 * for this process to MAX_FD_COUNT, we do not want
 * socket or file desscriptor numbers to exceed this num
 * because of the way we are storing the client connections
 */
int set_fd_limit()
{
    int ret = 0;
    struct rlimit rl;

    // Get the current limit
    ret = getrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: Retrieval getrlimit", __func__);
        return -1;
    }

    if (rl.rlim_cur == MAX_FD_COUNT && rl.rlim_max == MAX_FD_COUNT)
        return 0;

    // Set a new soft and hard limit for open files
    rl.rlim_cur = MAX_FD_COUNT;
    rl.rlim_max = MAX_FD_COUNT;

    // Set the new limit
    ret = setrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: setrlimit", __func__);
        return -1;
    }

    // Verify the change
    ret = getrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: Verify getrlimit", __func__);
        return -1;
    }

    if (rl.rlim_cur != MAX_FD_COUNT || rl.rlim_max != MAX_FD_COUNT)
    {
        LOG_ERROR("%s: Verification failed soft %ld, hard = %ld\n",
                  __func__, rl.rlim_cur, rl.rlim_max);
        return -1;
    }
    return 0;
}

/**
 * Convert a normal socket to a non-blocking socket
 * Returns 0 on success, -1 otherwise
 */
int set_non_blocking(const int fd, bool is_non_block)
{
    int ret = 0;
    ret = fcntl(fd, F_GETFL, 0);
    if (ret == -1)
    {
        LOG_ERROR("%s fcntl get", __func__);
        return -1;
    }

    if(is_non_block)
        ret |= O_NONBLOCK;
    else
        ret &= ~O_NONBLOCK;

    ret = fcntl(fd, F_SETFL, ret);
    if (ret == -1)
    {
        LOG_ERROR("%s fcntl set", __func__);
        return -1;
    }
    return 0;
}

/**
 * Converts ip version agnostic address to string
 */
const char *get_ip_address(s_addr *addr)
{
    void *ip_addr = NULL;
    uint16_t port = 0;
    s_addr6 *ipv6 = NULL;
    s_addr4 *ipv4 = NULL;
    static char ipstr[INET6_ADDRSTRLEN + 8] = {0};

    if (addr->sa_family == AF_INET)
    {
        ipv4 = (s_addr4 *)addr;
        ip_addr = &(ipv4->sin_addr);
        port = ntohs(ipv4->sin_port);
    }
    else
    {
        ipv6 = (s_addr6 *)addr;
        ip_addr = &(ipv6->sin6_addr);
        port = ntohs(ipv6->sin6_port);
    }

    // convert the IP to a string and return
    inet_ntop(addr->sa_family, ip_addr, ipstr, INET6_ADDRSTRLEN);
    snprintf(ipstr + strlen(ipstr), 8, ":%d", port);
    return ipstr;
}

/**
 * 
 */
socklen_t check_ip_and_port(const char *ip_str, const char *port_str, s_addr *server_addr)
{
    char *end_ptr = NULL;
    s_addr4 * addr4 = (s_addr4 *) server_addr;
    s_addr6 * addr6 = (s_addr6 *) server_addr;
    uint16_t port_no = SERVER_PORT_NO;

    if (server_addr == NULL)
        return 0;
    
    if (port_str != NULL)
    {
        errno = 0;
        long port_long = strtol(port_str, &end_ptr, 10);

        if (errno != 0)
        {
            fprintf(stderr, "%s: strtol: %s", __func__, strerror(errno));
            return 0;
        }
        else if (end_ptr == port_str || *end_ptr != '\0')
        {
            fprintf(stderr, "Invalid port number provided: %s\n", port_str);
            return 0;
        }
        else if (port_long < 0 || port_long > 65535)
        {
            fprintf(stderr, "Port number: %ld out of range (0-65535)\n", port_long);
            return 0;
        }
        port_no = (uint16_t) port_long;
    }
    
    if (ip_str == NULL)
    {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port_no);
        // addr6->sin6_addr  = in6addr_loopback;
        addr6->sin6_addr  = in6addr_any;
        return S_ADDR6_SIZE;
    }

    if (inet_pton(AF_INET, ip_str, &(addr4->sin_addr)) == 1)
    {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port_no);
        return S_ADDR4_SIZE;
    }
    else if (inet_pton(AF_INET, ip_str, &(addr4->sin_addr)) == 1)
    {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port_no);
        return S_ADDR6_SIZE;   
    }

    fprintf(stderr, "Invalid ip address provided: %s\n", ip_str);
    return 0;
}

/**
 * Function to intiate a server socket on the machine,
 * and bind it to provided IP and port for listening to
 * incoming client connections.
 */
int initiate_server(s_addr *server_addr, socklen_t addr_len)
{
    int opt = 0;
    int ret = 0;
    int server_fd = 0;

    server_fd = socket(server_addr->sa_family, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        LOG_ERROR("socket");
        return -1;
    }

    // Set to non blocking to avoid waiting for incoming connection
    ret = set_non_blocking(server_fd, true);
    if (ret != 0)
        goto err_cleanup;

    // Allow process to reuse a socket that's not entirely freed up by kernel
    ret = 1;
    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &ret, sizeof(int));
    if (ret != 0)
    {
        LOG_ERROR("%s setsockopt SO_REUSEADDR", __func__);
        goto err_cleanup;
    }

    opt = 0;
    if (server_addr->sa_family == AF_INET6 &&
        setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0)
    {
        LOG_ERROR("%s setsockopt IPV6_V6ONLY", __func__);
        goto err_cleanup;
    }

    // Bind the socket to specified IP address and port
    // All new incoming connections will be redirected to this port
    ret = bind(server_fd, server_addr, addr_len);
    if (ret < 0)
    {
        LOG_ERROR("%s bind", __func__);
        goto err_cleanup;
    }

    // Use this socket for accepting new connections not for requests
    ret = listen(server_fd, MAX_QUEUE_CONN);
    if (ret < 0)
    {
        LOG_ERROR("%s listen", __func__);
        goto err_cleanup;
    }

    LOG_INFO("Server listening on [%s] sockfd [%d]", get_ip_address(server_addr), server_fd);
    return server_fd;

err_cleanup:
    close(server_fd);
    return -1;
}

/**
 * 
 */
static int accept_client(int server_fd, int *client_fd_ptr)
{
    int client_fd = 0, ret = 0;
    struct sockaddr_storage client_addr = {0};
    socklen_t addr_size = sizeof(struct sockaddr_storage);
    
    (*client_fd_ptr) = -1; 
    client_fd = accept(server_fd, (s_addr *)&client_addr, &addr_size);
    if (client_fd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        LOG_ERROR("%s accept", __func__);
        return -1;
    }

    LOG_INFO("Incoming Connection from %s", get_ip_address((s_addr *)&client_addr));
    ret = set_non_blocking(client_fd, true);
    if (ret != 0)
    {
        close(client_fd);
        return -1;
    }

    ret = add_client_info(client_fd);
    if (ret == -1)
    {
        close(client_fd);
    }
    (*client_fd_ptr) = client_fd;
    return 1;
}

/**
 * Function that accepts all new incoming connections
 * on the server socket and adds them to epoll event listener
 */
int accept_connections(const int server_fd, const int epoll_fd)
{
    int ret = 0, client_fd = 0;
    struct epoll_event ev = {0};

    // Loop until all incoming connections have been accepted
    while (1)
    {
        ret = accept_client(server_fd, &client_fd);

        if(ret == 0)
            break;

        else if(ret == -1)
            continue;

        // Add new connection to epoll event listener
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
        if (ret != 0)
        {
            LOG_ERROR("%s epoll_ctl", __func__);
            remove_client_info_fd(client_fd);
            continue;
        }
        LOG_INFO("Connection accepted and bound to client_fd: %d", client_fd);
    }

    return 0;
}