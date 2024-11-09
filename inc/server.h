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
#include <stdbool.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <limits.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_QUEUE_CONN 256
#define MAX_ALIVE_CONN 4096
#define BUFFER_SIZE 4096

#define DEFAULT_ASSET_PATH "assets/"
#define DEFAULT_ASSET_LEN  sizeof(DEFAULT_ASSET_PATH)
#define INDEX_PAGE      "index.html"
#define ERROR_404_PAGE  "error_404.html"
#define ERROR_500_PAGE  "error_500.html"

#define DEBUG_LOG_FILE "/tmp/legion.log"
#define DEBUG_LOG_OLD "/tmp/legion_old.log"
#define SERVER_IP_ADDR "127.0.0.1"
#define SERVER_PORT "8080"

typedef struct
{
    SSL* ssl[MAX_ALIVE_CONN];
    ssize_t last_free;
} client_list;

typedef struct
{
    int fd;
    char *file_name;
    size_t file_size;
} page_cache;

// parser.c
int handle_http_request(int client_fd);

// utils.c
int initiate_logging();
void shutdown_loggging();
// Do not call below function directly, use the LOG macro instead
void debug_log(const char *fmt, ...);

int set_nonblocking(const int fd);
void init_client_list();
void cleanup_client_list();
int add_client_ssl_to_list(SSL * client_ssl);
int remove_client_ssl_from_list(SSL * client_ssl);

void free_cache();
size_t initiate_cache(const char *root_path);
page_cache *get_page_cache(const char *path);

// net_utils.c
int initiate_server(const char *server_ip, const char *port);
int accept_connections(const int server_fd, const int epoll_fd);
const char *get_internet_facing_ipv4();

#ifdef DEBUG
#define LOG(fmt, ...) debug_log("[DEBUG] " fmt "\n", ##__VA_ARGS__);
#else
#define LOG(fmt, ...)
#endif

#ifdef IPV6_SERVER
// char * get_internet_facing_ipv6();
#endif

#endif