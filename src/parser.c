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

extern const int g_epoll_fd;
extern const page_cache *page_404;
extern const page_cache *page_500;

int sendfile_to_client(SSL *client_ssl, const page_cache * cache_ptr)
{
    char buffer[BUFFER_SIZE];
    int bytes_read = 0, bytes_written = 0;
    int ssl_ret = 0;
    off_t total_bytes_read = 0;
    if(cache_ptr->file_map == NULL)
    {
        // Read file and send in chunks
        bytes_read = (int) pread(cache_ptr->fd, buffer, BUFFER_SIZE, total_bytes_read);
        total_bytes_read += bytes_read;
        while (bytes_read > 0)
        {
            bytes_written = 0;
            while (bytes_written < bytes_read)
            {
                ssl_ret = SSL_write(client_ssl, buffer + bytes_written, bytes_read - bytes_written);
                if (ssl_ret <= 0)
                {
                    LOG_ERROR(" %s SSL_write error: %d", __func__, SSL_get_error(client_ssl, ssl_ret));
                    return -1;
                }
                bytes_written += ssl_ret;
            }
            bytes_read = (int) pread(cache_ptr->fd, buffer, BUFFER_SIZE, total_bytes_read);
            total_bytes_read += bytes_read;
        }

        // lseek(cache_ptr->fd, 0, SEEK_SET);
        if (total_bytes_read < 0)
        {
            LOG_ERROR("%s Error in reading file", __func__);
            return -1;
        }
    }
    else
    {
        bytes_written = 0;
        while(bytes_written < cache_ptr->file_size)
        {
            ssl_ret = SSL_write(client_ssl, cache_ptr->file_map + bytes_written, (int)cache_ptr->file_size - bytes_written);
            if (ssl_ret <= 0)
            {
                LOG_ERROR(" %s SSL_write error: %d", __func__, SSL_get_error(client_ssl, ssl_ret));
                ERR_print_errors_fp(stderr);
                fflush(stderr);
                return -1;
            }
            bytes_written += ssl_ret;
        }
    }

    return 0;
}

/**
 * Sends back 500 response code to client and
 * Returns -1 to instruct closing of this connection
 */
int send_server_error(SSL *client_ssl)
{
    int buf_len = 0;
    char resp[256];
    buf_len = (int)snprintf(resp, 255, "HTTP/1.1 500 Internal Server Error\r\n"
                                       "Content-Type: %s; charset=UTF-8\r\n"
                                       "Content-Length: %lu\r\nConnection: close\r\n\r\n",
                                        page_500->mime_type, page_500->file_size);
    if (buf_len <= 0)
        return -1;

    SSL_write(client_ssl, resp, buf_len);
    sendfile_to_client(client_ssl, page_500);
    return -1;
}

/**
 * Sends back 404 response code to client
 * Returns -1 to instruct closing of this connection
 */
int send_not_found(SSL *client_ssl)
{
    int buf_len = 0;
    char resp[256];
    buf_len = (int)snprintf(resp, 255, "HTTP/1.1 404 Not Found\r\n"
                                       "Content-Type: %s charset=UTF-8\r\n"
                                       "Content-Length: %lu\r\nConnection: close\r\n\r\n",
                                        page_404->mime_type, page_404->file_size);
    if (buf_len <= 0)
        return -1;
    SSL_write(client_ssl, resp, buf_len);
    sendfile_to_client(client_ssl, page_404);
    return -1;
}

/**
 * Construct appropriate header and send back the requested file
 * Returns 0 on success, -1 otherwise
 */
int send_response(SSL *client_ssl, const page_cache *page, bool is_head)
{
    int buf_len = 0;
    char resp[256];

    buf_len = (int)snprintf(resp, 255, "HTTP/1.1 200 OK\r\nServer: legion\r\n"
                                       "Content-Type: %s; charset=UTF-8\r\n"
                                       "Content-Length: %lu\r\nConnection: keep-alive\r\n\r\n",
                                        page->mime_type, page->file_size);
    if (buf_len <= 0)
        return -1;

    SSL_write(client_ssl, resp, buf_len);
    if (!is_head)
    {
        sendfile_to_client(client_ssl, page);
    }
    return 0;
}

/**
 * Parse the incoming message for the requested webpage
 * And send back the page if it's found
 * Returns 0 on success, -1 otherwise
 */
int process_get_request(SSL *client_ssl, char *buf, bool is_head)
{
    ssize_t len = 0;
    char *file_end = NULL;
    const page_cache *page_reqd = NULL;

    if (*buf == '/')
        buf++;

    file_end = strchr(buf, ' ');
    if (file_end == NULL)
        return send_server_error(client_ssl);

    len = file_end - buf;
    if (len < 0 || len >= PATH_MAX)
        return send_server_error(client_ssl);

    (*file_end) = '\0';
    page_reqd = get_page_cache(buf);
    if (page_reqd == NULL)
    {
        LOG_ERROR("%s Requested page %s not found", __func__, buf);
        return send_not_found(client_ssl);
    }
    return send_response(client_ssl, page_reqd, is_head);
}

int parse_header(const char *buffer, client_info *cinfo)
{
    char *needle = NULL;
    cinfo->keep_alive = false;
    needle = strstr(buffer, "keep-alive");
    if (needle != NULL)
        cinfo->keep_alive = true;

    return 0;
}

void handle_http_request(void *arg)
{
    client_info *cinfo = (client_info *)arg;
    struct epoll_event ev = {0};
    int bytes_read = 0, ret = 0;
    char buffer[BUFFER_SIZE];

    set_non_blocking(cinfo->fd, false);
    set_socket_timeout(cinfo->fd, 0, RTT_TIMEOUT_US);
    bytes_read = SSL_read(cinfo->ssl, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0)
    {
        remove_client_info(cinfo);
        return;
    }

    buffer[bytes_read] = '\0';
    while (1)
    {
        // LOG_INFO("\n%s", buffer);
        ret = parse_header(buffer, cinfo);
        if (strncmp(buffer, "GET", 3) == 0)
            ret = process_get_request(cinfo->ssl, buffer + 4, false);
        else if (strncmp(buffer, "HEAD", 4) == 0)
            ret = process_get_request(cinfo->ssl, buffer + 5, true);
        else
            ret = send_server_error(cinfo->ssl);

        if (ret != 0 || cinfo->keep_alive == false)
            break;

        bytes_read = SSL_read(cinfo->ssl, buffer, BUFFER_SIZE - 1);
        if (bytes_read > 0)
            continue;

        else if (bytes_read == 0)
            break;

        ret = SSL_get_error(cinfo->ssl, bytes_read);
        if (ret == SSL_ERROR_WANT_READ)
        {
            set_non_blocking(cinfo->fd, true);
            ev.events = EPOLLIN;
            ev.data.fd = cinfo->fd;
            ret = epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, cinfo->fd, &ev);
            if (ret == -1)
            {
                remove_client_info(cinfo);
                return;
            }
        }
        else
            break;
    }
    remove_client_info(cinfo);
}

/**
void handle_http_request(void *arg)
{
    client_info * cinfo = (client_info *) arg;
    ssize_t bytes_read = 0;
    char buffer[BUFFER_SIZE];

    // Below part needs better handling
    bytes_read = SSL_read(cinfo->ssl, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0)
    {
        return;
    }
    buffer[bytes_read] = '\0';

    set_non_blocking(cinfo->fd, false);
    if (strncmp(buffer, "GET", 3) == 0)
        process_get_request(cinfo->ssl, buffer + 4, false);

    else if (strncmp(buffer, "HEAD", 4) == 0)
        process_get_request(cinfo->ssl, buffer + 5, true);

    else
        send_server_error(cinfo->ssl);

    remove_client_info(cinfo);
}
 */