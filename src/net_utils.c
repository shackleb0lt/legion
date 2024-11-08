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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SOCKADDR_4_SIZE sizeof(struct sockaddr_in)
#define SOCKADDR_6_SIZE sizeof(struct sockaddr_in6)

/**
 * Function to get an IPv4 address of the machine which
 * is used to communicate with internet
 * Returns a string strring numeric IPv4 notation on success,
 * NULL otherwise
 */
const char *get_internet_facing_ipv4()
{
    int dns_fd = 0;
    struct sockaddr_in serv = {0};
    struct sockaddr_in local_addr = {0};
    static char ip_addr[INET_ADDRSTRLEN];

    // Fill struct as if we plan to connect
    // to Google's DNS Server
    dns_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (dns_fd < 0)
    {
        perror("socket: ");
        return NULL;
    }
    serv.sin_family = AF_INET;
    serv.sin_port = htons(53);
    memset(ip_addr, 0, INET_ADDRSTRLEN);
    inet_pton(AF_INET, "8.8.8.8", &serv.sin_addr);

    // Connect the socket to the remote address
    // This doesn’t establish a true connection because its UDP
    // but sets up the socket with an appropriate route.
    if (connect(dns_fd, (struct sockaddr *)&serv, SOCKADDR_4_SIZE) < 0)
    {
        perror("connect");
        close(dns_fd);
        return NULL;
    }

    // Get the local address of the socket
    socklen_t addr_len = SOCKADDR_4_SIZE;
    if (getsockname(dns_fd, (struct sockaddr *)&local_addr, &addr_len) == -1)
    {
        perror("getsockname");
        close(dns_fd);
        return NULL;
    }

    // Convert the IP address to a string
    inet_ntop(AF_INET, &local_addr.sin_addr, ip_addr, INET_ADDRSTRLEN);
    LOG("Internet facing IP is %d", ip_addr);
    close(dns_fd);
    return ip_addr;
}

/**
 * Converts ip version agnostic address to string 
 */
const char *get_ip_address(struct sockaddr *addr)
{
    void *ip_addr = NULL;
    short unsigned port = 0; 
    struct sockaddr_in6 *ipv6 = NULL;
    struct sockaddr_in *ipv4 = NULL;
    static char ipstr[INET6_ADDRSTRLEN + 8] = {0};

    if (addr->sa_family == AF_INET)
    {
        ipv4 = (struct sockaddr_in *)addr;
        ip_addr = &(ipv4->sin_addr);
        port = ntohs(ipv4->sin_port);
    }
    else
    {
        ipv6 = (struct sockaddr_in6 *)addr;
        ip_addr = &(ipv6->sin6_addr);
        port = ntohs(ipv6->sin6_port);
    }

    // convert the IP to a string and return
    inet_ntop(addr->sa_family, ip_addr, ipstr, INET6_ADDRSTRLEN);
    snprintf(ipstr + strlen(ipstr), 8, ":%d", port);
    return ipstr;
}

/**
 * Function to intiate a server socket on the machine,
 * and bind it to provided IP and port for listening to
 * incoming client connections.
 */
int initiate_server(const char *server_ip, const char *port)
{
    int server_fd = 0, ret = 0;
    struct addrinfo hint = {0};
    struct addrinfo *res = NULL;

    if (server_ip == NULL || port == NULL)
    {
        fprintf(stderr, "No IP address or port number available for host\n");
        return -1;
    }

    // Fill the hint struct for desired connection
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_NUMERICHOST;

    ret = getaddrinfo(server_ip, port, &hint, &res);
    if (ret != 0 || res == NULL)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
        return -1;
    }

    // Open a TCP socket that can communicate using IPv4 or IPv6
    server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd < 0)
    {
        perror("socket");
        goto err_cleanup;
    }

    // Set to non blocking to avoid waiting for incoming connection
    ret = set_nonblocking(server_fd);
    if (ret == -1)
        goto err_cleanup;

    // Allow process to reuse a socket that's
    // not entirely freed up by kernel
    ret = 1;
    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &ret, sizeof(int));
    if (ret == -1)
    {
        perror("setsockopt");
        goto err_cleanup;
    }

    // Bind the socket to specified IP address and port
    // All new incoming connections will be redirected to this port
    ret = bind(server_fd, res->ai_addr, res->ai_addrlen);
    if (ret < 0)
    {
        perror("bind");
        goto err_cleanup;
    }

    // Use this socket for accepting new connections not for requests
    ret = listen(server_fd, MAX_QUEUE_CONN);
    if (ret < 0)
    {
        perror("listen");
        goto err_cleanup;
    }

    printf("Server is active on %s\n", get_ip_address(res->ai_addr));
    fflush(stdout);
    LOG("Server is active on %s, server_fd %d",
        get_ip_address(res->ai_addr), server_fd);
    freeaddrinfo(res);
    return server_fd;

err_cleanup:
    close(server_fd);
    freeaddrinfo(res);
    return ret;
}

/**
 * Function that accepts all new incoming connections
 * on the server socket and adds them to epoll event listener
 */
int accept_connections(const int server_fd, const int epoll_fd, client_list *clist)
{
    int ret = 0, client_fd = 0;
    struct sockaddr client_addr = {0};
    socklen_t client_addr_size = sizeof(struct sockaddr);
    struct epoll_event ev = {0};

    // Loop until all incoming connections have been accepted
    // or rejected them if queue is full
    while (1)
    {
        client_fd = accept(server_fd, &client_addr, &client_addr_size);
        if (client_fd == -1)
            break;

        LOG("Incoming Connection from %s", get_ip_address(&client_addr));
        // Set non blocking to avoid waiting for incoming requests
        ret = set_nonblocking(client_fd);
        if (ret == -1)
            break;

        // Add new connection to list of open connections
        ret = add_fd_to_list(clist, client_fd);
        if (ret == -1)
        {
            close(client_fd);
            continue;
        }

        // Add new connection to epoll event listener
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
        if (ret == -1)
        {
            perror("epoll_ctl: conn_sock");
            remove_fd_from_list(clist, client_fd);
            break;
        }
        LOG("Connection accepted and bound to client_fd: %d", client_fd);
    }

    // Report error only if accept call failed
    if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        perror("accept");
        return ret;
    }

    if (ret == -1 && client_fd > 0)
    {
        close(client_fd);
    }
    return ret;
}

#ifdef IPV6_SERVER
char *get_internet_facing_ipv6()
{
    int dns_fd = 0;
    struct sockaddr_in6 serv = {0};
    struct sockaddr_in6 local_addr = {0};
    static char ip_addr[INET6_ADDRSTRLEN];

    dns_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (dns_fd < 0)
    {
        perror("socket: ");
        return NULL;
    }
    serv.sin6_family = AF_INET6;
    serv.sin6_port = htons(53);
    memset(ip_addr, 0, INET_ADDRSTRLEN);
    inet_pton(AF_INET, "2001:4860:4860:0:0:0:0:6464", &serv.sin6_addr);

    // Connect the socket to the remote address
    // This doesn’t establish a true connection
    // but sets up the socket with an appropriate route.
    if (connect(dns_fd, (struct sockaddr *)&serv, SOCKADDR_6_SIZE) < 0)
    {
        perror("connect: ");
        close(dns_fd);
        return NULL;
    }

    // Get the local address of the socket
    socklen_t addr_len = SOCKADDR_6_SIZE;
    if (getsockname(dns_fd, (struct sockaddr *)&local_addr, &addr_len) == -1)
    {
        perror("getsockname");
        close(dns_fd);
        return NULL;
    }

    // Convert the IP address to a string
    inet_ntop(AF_INET6, &local_addr.sin6_addr, ip_addr, INET6_ADDRSTRLEN);

    close(dns_fd);
    return ip_addr;
}
#endif