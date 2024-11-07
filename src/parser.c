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
#include <sys/sendfile.h>

int send_internal_server(const int client_fd)
{
    ssize_t buf_len = 0;
    char resp[128];
    static page_cache * page_500 = NULL;
    if(page_500 == NULL)
    {
        page_500 = get_page_cache("error_500.html");
        if(page_500 == NULL)
            return -1;
    }
    buf_len = snprintf(resp, 127, "HTTP/1.1 500 Internal Server Error\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n"
                        "Content-Length: %lu\r\nConnection: close\r\n\r\n", 
                        page_500->file_size);
    if(buf_len <= 0)
        return -1;
    
    send(client_fd, resp, (size_t)buf_len, 0);
    sendfile(client_fd, page_500->fd, NULL, page_500->file_size);
    lseek(page_500->fd, 0, SEEK_SET);
    return 0;
}

int send_not_found(const int client_fd)
{
    ssize_t buf_len = 0;
    char resp[128];
    static page_cache * page_404 = NULL;
    if(page_404 == NULL)
    {
        page_404 = get_page_cache("error_404.html");
        if(page_404 == NULL)
            return -1;
    }
    buf_len = snprintf(resp, 127, "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n"
                        "Content-Length: %lu\r\nConnection: close\r\n\r\n", 
                        page_404->file_size);
    if(buf_len <= 0)
        return -1;
    
    send(client_fd, resp, (size_t)buf_len, 0);
    sendfile(client_fd, page_404->fd, NULL, page_404->file_size);
    lseek(page_404->fd, 0, SEEK_SET);
    return 0;
}

int send_response(const int client_fd, const page_cache * page, bool is_head)
{
    ssize_t buf_len = 0;
    char resp[128];

    buf_len = snprintf(resp, 127, "HTTP/1.1 200 OK\r\nServer: legion\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n"
                        "Content-Length: %lu\r\nConnection: close\r\n\r\n", 
                        page->file_size);
    if(buf_len <= 0)
        return -1;
    
    send(client_fd, resp, (size_t)buf_len, 0);
    if(!is_head)
    {
        sendfile(client_fd, page->fd, NULL, page->file_size);
        lseek(page->fd, 0, SEEK_SET);
    }
    return 0;
}

int process_get_request(const int client_fd, char* buf, bool is_head)
{
    ssize_t len =0;
    char *file_end = NULL;
    page_cache * page_reqd = NULL;

    if(*buf == '/')
        buf++;

    file_end = strchr(buf, ' ');
    if(file_end == NULL)
        return send_internal_server(client_fd);

    len = file_end - buf;
    if(len < 0 || len >= PATH_MAX)
        return send_internal_server(client_fd);;
    
    (*file_end) = '\0';
    page_reqd = get_page_cache(buf);
    if(page_reqd == NULL)
    {
        return send_not_found(client_fd);
    }
    return send_response(client_fd, page_reqd, is_head);
}

int handle_http_request(const int client_fd)
{
    int ret = 0;
    ssize_t bytes_read = 0;
    char buffer[BUFFER_SIZE];
    bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
        return -1;
    buffer[bytes_read] = '\0';

    if(strncmp(buffer, "GET", 3) == 0)
        ret = process_get_request(client_fd, buffer + 4, false);

    else if(strncmp(buffer, "HEAD", 4) == 0)
        ret = process_get_request(client_fd, buffer + 5, true);

    return ret;
}