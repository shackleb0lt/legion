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

const char *get_internet_facing_ipv4()
{
    int dns_fd = 0;
    struct sockaddr_in serv = {0};
    struct sockaddr_in local_addr = {0};
    static char ip_addr[INET_ADDRSTRLEN];

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
    // This doesn’t establish a true connection
    // but sets up the socket with an appropriate route.
    if (connect(dns_fd, (struct sockaddr *)&serv, SOCKADDR_4_SIZE) < 0)
    {
        perror("connect");
        close(dns_fd);
        return NULL;
        ;
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

    close(dns_fd);
    return ip_addr;
}

int initiate_server(const char *server_ip, const char *port)
{
    int server_fd = 0, ret = 0;
    struct addrinfo hint = {0};
    struct addrinfo *res = NULL;

    if (server_ip == NULL)
    {
        server_ip = get_internet_facing_ipv4();
        if (server_ip == NULL)
        {
            fprintf(stderr, "No IP address available for host\n");
            return -1;
        }
    }

    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_NUMERICHOST;

    ret = getaddrinfo(server_ip, port, &hint, &res);
    if (ret != 0 || res == NULL)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
        return -1;
    }

    server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd < 0)
    {
        perror("socket");
        goto err_cleanup;
    }

    ret = set_nonblocking(server_fd);
    if (ret == -1)
        goto err_cleanup;

    ret = 1;
    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &ret, sizeof(int));
    if (ret == -1)
    {
        perror("setsockopt");
        goto err_cleanup;
    }

    ret = bind(server_fd, res->ai_addr, res->ai_addrlen);
    if (ret < 0)
    {
        perror("bind");
        goto err_cleanup;
    }

    ret = listen(server_fd, MAX_QUEUE_CONN);
    if (ret < 0)
    {
        perror("listen");
        goto err_cleanup;
    }

    freeaddrinfo(res);
    return server_fd;

err_cleanup:
    close(server_fd);
    freeaddrinfo(res);
    return ret;
}

int accept_connections(const int server_fd, const int epoll_fd, client_list* clist)
{
    int ret = 0, client_fd = 0;
    struct sockaddr client_addr = {0};
    socklen_t client_addr_size = sizeof(struct sockaddr);
    struct epoll_event ev = {0};
    // struct sockaddr_in *temp = NULL;
    // char ipstr[INET6_ADDRSTRLEN];

    while(1)
    {
        client_fd = accept(server_fd, &client_addr, &client_addr_size);
        if(client_fd == -1)
            break;

        // temp = (struct sockaddr_in *) &client_addr;
        // inet_ntop(temp->sin_family, &(temp->sin_addr), ipstr, sizeof(ipstr));
        // debug_log("%s : %d \n", ipstr, ntohs(temp->sin_port));

        ret = set_nonblocking(client_fd);
        if(ret == -1)
            break;

        ret = add_fd_to_list(clist, client_fd);
        if(ret == -1)
        {
            close(client_fd);
            continue;
        }

        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
        if (ret == -1)
        {
            perror("epoll_ctl: conn_sock");
            remove_fd_from_list(clist, client_fd);
            break;
        }
    }

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
        ;
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
    printf("Public-facing IP address: %s\n", ip_addr);

    close(dns_fd);
    return ip_addr;
}
#endif