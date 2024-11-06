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

#ifndef _SERVER_H
#define _SERVER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_PORT "8080"
#define MAX_QUEUE_CONN 256
#define MAX_ALIVE_CONN 4096
#define BUFFER_SIZE 4096
#define INDEX_FILE "/var/www/html/index.nginx-debian.html"
#define DEBUG_LOG_FILE "/tmp/legion.log"

typedef struct 
{
    int fd_list[MAX_ALIVE_CONN];
    ssize_t last_free;
} client_list;

// parser.c
int handle_http_request(int client_fd);

// utils.c
int debug_log(const char *fmt, ...);
int set_nonblocking(const int fd);
int add_fd_to_list(client_list *clist,const int fd);
int remove_fd_from_list(client_list *clist,const int fd);

// net_utils.c
int initiate_server(const char *server_ip, const char *port);
int accept_connections(const int server_fd, const int epoll_fd, client_list* client_list);
const char *get_internet_facing_ipv4();


#ifdef IPV6_SERVER
// char * get_internet_facing_ipv6();
#endif

#endif