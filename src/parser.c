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

#define RTT_TIMEOUT_US 200000

typedef enum
{
    DATA_READY,
    RECV_ERROR,
    PARTIAL_READ,
} read_state;

extern int g_epoll_fd;

extern const page_cache *page_404;
extern const page_cache *page_500;

bool is_request_complete(char *buf, size_t len)
{
    if (len < 4)
        return false;

    if (strncmp(buf + len - 4, "\r\n\r\n", 4) == 0)
        return true;

    return false;
}

ssize_t nb_recv(int fd, char *buf, size_t buf_size)
{
    char *curr = buf;
    ssize_t total_read = 0;
    ssize_t bytes_read = 0;

    buf_size--;
    while (1)
    {
        bytes_read = recv(fd, curr, buf_size, 0);
        if (bytes_read < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else if (errno == EINTR)
                continue;
            LOG_ERROR("recv");
            return -1;
        }

        if (bytes_read == 0)
            break;

        curr += bytes_read;
        total_read += bytes_read;
        buf_size -= (size_t)bytes_read;
    }

    buf[total_read] = '\0';
    return total_read;
}

int sendfile_to_client(int fd, const page_cache *cache_ptr)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = 0;
    ssize_t bytes_written = 0;
    ssize_t total_bytes_written = 0;
    off_t total_bytes_read = 0;

    while ((bytes_read = pread(cache_ptr->fd, buffer, BUFFER_SIZE, total_bytes_read)) > 0)
    {
        total_bytes_read += bytes_read;
        total_bytes_written = 0;

        while (total_bytes_written < bytes_read)
        {
            bytes_written = send(fd, buffer + total_bytes_written, (size_t)(bytes_read - total_bytes_written), 0);
            if (bytes_written <= 0)
            {
                LOG_ERROR("%s send error: %s", __func__, strerror(errno));
                return -1;
            }
            total_bytes_written += bytes_written;
        }
    }

    if (bytes_read < 0)
    {
        LOG_ERROR("%s Error reading file: %s", __func__, strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * Sends back 500 response code to client and
 * Returns -1 to instruct closing of this connection
 */
int send_server_error(int fd)
{
    int resp_len = 0;
    char resp[256];
    resp_len = snprintf(resp, 255, "HTTP/1.1 500 Internal Server Error\r\n"
                                    "Content-Type: %s; charset=UTF-8\r\n"
                                    "Content-Length: %lu\r\nConnection: close\r\n\r\n",
                                    page_500->mime_type, page_500->file_size);

    send(fd, resp, (size_t)resp_len, 0);
    sendfile_to_client(fd, page_500);
    return -1;
}

/**
 * Sends back 404 response code to client
 * Returns -1 to instruct closing of this connection
 */
int send_not_found(int fd)
{
    int buf_len = 0;
    char resp[256];
    buf_len = snprintf(resp, 255, "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: %s charset=UTF-8\r\n"
                                "Content-Length: %lu\r\nConnection: close\r\n\r\n",
                                page_404->mime_type, page_404->file_size);
    if (buf_len <= 0)
        return -1;

    send(fd, resp, (size_t)buf_len, 0);
    sendfile_to_client(fd, page_404);
    return -1;
}

/**
 * Construct appropriate header and send back the requested file
 * Returns 0 on success, -1 otherwise
 */
int send_response(int fd, const page_cache *page, bool is_head)
{
    int buf_len = 0;
    char resp[256];

    buf_len = snprintf(resp, 255, "HTTP/1.1 200 OK\r\nServer: legion\r\n"
                                "Content-Type: %s; charset=UTF-8\r\n"
                                "Content-Length: %lu\r\nConnection: keep-alive\r\n\r\n",
                                page->mime_type, page->file_size);

    if (buf_len <= 0)
        return -1;

    send(fd, resp, (size_t)buf_len, 0);
    if (!is_head)
        sendfile_to_client(fd, page);

    return 0;
}

/**
 * Parse the incoming message for the requested webpage
 * And send back the page if it's found
 * Returns 0 on success, -1 otherwise
 */
int process_get_request(int fd, char *buf, bool is_head)
{
    ssize_t len = 0;
    char *file_end = NULL;
    const page_cache *page_reqd = NULL;

    if (*buf == '/')
        buf++;

    file_end = strchr(buf, ' ');
    if (file_end == NULL)
        return send_server_error(fd);

    len = file_end - buf;
    if (len < 0 || len >= PATH_MAX)
        return send_server_error(fd);

    (*file_end) = '\0';
    page_reqd = get_page_cache(buf);
    if (page_reqd == NULL)
    {
        LOG_ERROR("%s Requested page %s not found", __func__, buf);
        return send_not_found(fd);
    }
    return send_response(fd, page_reqd, is_head);
}

void handle_http_request(void *arg)
{
    client_info *cinfo = (client_info *)arg;
    ssize_t bytes_read = 0, ret = 0;

    bytes_read = nb_recv(cinfo->fd, cinfo->buffer - cinfo->buf_len, BUFFER_SIZE - cinfo->buf_len);
    if (bytes_read <= 0)
        goto close_conn;

    LOG_INFO("%ld %s", bytes_read, cinfo->buffer);

    cinfo->buf_len += (size_t)bytes_read;
    if (is_request_complete(cinfo->buffer, cinfo->buf_len) == false)
    {
        if (cinfo->buf_len == BUFFER_SIZE - 1)
            goto close_conn;
        return;
    }

    if (strstr(cinfo->buffer, "Connection: close") != NULL)
        cinfo->keep_alive = false;
    else
        cinfo->keep_alive = true;

    if (strncmp(cinfo->buffer, "GET", 3) == 0)
        ret = process_get_request(cinfo->fd, cinfo->buffer + 4, false);
    else if (strncmp(cinfo->buffer, "HEAD", 4) == 0)
        ret = process_get_request(cinfo->fd, cinfo->buffer + 5, true);
    else
        ret = send_server_error(cinfo->fd);

    if (ret < 0)
        goto close_conn;

    cinfo->buf_len = 0;

    if (cinfo->keep_alive)
        return;

close_conn:
    epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, cinfo->fd, NULL);
    remove_client_info(cinfo);
}