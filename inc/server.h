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

#include "logger.h"

#include <stdbool.h>
#include <limits.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_QUEUE_CONN 64
#define MAX_ALIVE_CONN 256
#define BUFFER_SIZE    8192

#define MAX_FD_COUNT    4096
#define TLS_TIMEOUT_SEC 4

#define DEFAULT_PAGE_SIZE 4096

#define DEFAULT_ASSET_PATH "assets/"
#define DEFAULT_ASSET_LEN  sizeof(DEFAULT_ASSET_PATH)

#define DEFAULT_MIME_T  "application/octet-stream"
#define INDEX_PAGE      "index.html"
#define ERROR_404_PAGE  "error_404.html"
#define ERROR_500_PAGE  "error_500.html"


#define SERVER_IP_ADDR "127.0.0.1"
#define SERVER_PORT_NO 8080

typedef struct sockaddr s_addr;

typedef struct
{
    int fd;
    char *buffer;
    size_t buf_len;
    bool keep_alive;
} client_info;

typedef struct
{
    int fd;
    const char *file_name;
    const char *mime_type;
    off_t file_size;
} page_cache;

const page_cache *get_page_cache(const char *path);
size_t initiate_cache(const char *root_path);
void release_cache();

int set_fd_limit();
void init_client_list();
void cleanup_client_list();
void remove_client_info_fd(const int fd);
void remove_client_info(client_info * cinfo);
int add_client_info(const int client_fd);
client_info *get_client_info(const int client_fd);

int set_non_blocking(const int fd, bool is_non_block);

void handle_http_request(void *arg);

socklen_t check_ip_and_port(const char *ip_str, const char *port_str, s_addr *server_addr);
int initiate_server(s_addr *server_addr, socklen_t addr_len);
int accept_connections(const int server_fd, const int epoll_fd);

#ifdef IPV6_SERVER
// char * get_internet_facing_ipv6();
#endif

#endif