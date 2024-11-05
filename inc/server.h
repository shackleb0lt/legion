#ifndef _SERVER_H
#define _SERVER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

typedef struct 
{
    int fd_list[MAX_ALIVE_CONN];
    ssize_t last_free;
} client_list;

int handle_http_request(int client_fd);

int set_nonblocking(int fd);
int add_fd_to_list(client_list *clist, int fd);
int remove_fd_from_list(client_list *clist, int fd);

int initiate_server(const char *server_ip, const char *port);
int accept_connections(const int server_fd, const int epoll_fd, client_list* client_list);


#ifdef IPV6_SERVER
// char * get_internet_facing_ipv6();
#endif

#endif