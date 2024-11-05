#include "server.h"

const char * get_string()
{
    return "Hello World\n";
}

int handle_http_request(int client_fd)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    return 0;
}