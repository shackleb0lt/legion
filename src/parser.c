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

int sendfile_to_client(SSL *client_ssl, int file_fd, size_t count)
{
    char buffer[BUFFER_SIZE];
    int bytes_read = 0, bytes_written = 0;
    int ssl_ret = 0;
    (void)count;
    // Read file and send in chunks
    bytes_read = (int)read(file_fd, buffer, BUFFER_SIZE);
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
        bytes_read = (int)read(file_fd, buffer, BUFFER_SIZE);
    }

    lseek(file_fd, 0, SEEK_SET);
    if (bytes_read < 0)
    {
        LOG_ERROR("%s Error in reading file", __func__);
        return -1;
    }
    return 0;
}

/**
 * Sends back 500 response code to client and
 * Returns -1 to instruct closing of this connection
 */
int send_internal_server_err(SSL *client_ssl)
{
    int buf_len = 0;
    char resp[256];
    static page_cache *page_500 = NULL;
    if (page_500 == NULL)
    {
        page_500 = get_page_cache(ERROR_500_PAGE);
        if (page_500 == NULL)
            return -1;
    }
    buf_len = (int)snprintf(resp, 255, "HTTP/1.1 500 Internal Server Error\r\n"
                                       "Content-Type: text/html; charset=UTF-8\r\n"
                                       "Content-Length: %lu\r\nConnection: close\r\n\r\n",
                            page_500->file_size);
    if (buf_len <= 0)
        return -1;

    SSL_write(client_ssl, resp, buf_len);
    sendfile_to_client(client_ssl, page_500->fd, page_500->file_size);
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
    static page_cache *page_404 = NULL;
    if (page_404 == NULL)
    {
        page_404 = get_page_cache(ERROR_404_PAGE);
        if (page_404 == NULL)
            return -1;
    }
    buf_len = (int) snprintf(resp, 255, "HTTP/1.1 404 Not Found\r\n"
                                  "Content-Type: text/html; charset=UTF-8\r\n"
                                  "Content-Length: %lu\r\nConnection: close\r\n\r\n",
                       page_404->file_size);
    if (buf_len <= 0)
        return -1;
    SSL_write(client_ssl, resp, buf_len);
    sendfile_to_client(client_ssl, page_404->fd, page_404->file_size);
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

    buf_len = (int) snprintf(resp, 255, "HTTP/1.1 200 OK\r\nServer: legion\r\n"
                                  "Content-Type: text/html; charset=UTF-8\r\n"
                                  "Content-Length: %lu\r\nConnection: close\r\n\r\n",
                       page->file_size);
    if (buf_len <= 0)
        return -1;

    SSL_write(client_ssl, resp, buf_len);
    if (!is_head)
    {
        sendfile_to_client(client_ssl, page->fd, page->file_size);
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
    page_cache *page_reqd = NULL;

    if (*buf == '/')
        buf++;

    file_end = strchr(buf, ' ');
    if (file_end == NULL)
        return send_internal_server_err(client_ssl);

    len = file_end - buf;
    if (len < 0 || len >= PATH_MAX)
        return send_internal_server_err(client_ssl);

    (*file_end) = '\0';
    page_reqd = get_page_cache(buf);
    if (page_reqd == NULL)
    {
        LOG_ERROR("%s Requested page %s not found", __func__, buf);
        return send_not_found(client_ssl);
    }
    return send_response(client_ssl, page_reqd, is_head);
}

// Read the incoming HTTP Request
// Check the method type of the request
// And handle it accordingly
void handle_http_request(void *arg)
{
    int ret = 0;
    ssize_t bytes_read = 0;
    char buffer[BUFFER_SIZE];
    SSL *client_ssl = (SSL *)arg;

    // Below part needs better handling
    bytes_read = SSL_read(client_ssl, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0)
    {
        // return -1;
        return;
    }
    buffer[bytes_read] = '\0';
    // LOG_INFO("%s", buffer);

    if (strncmp(buffer, "GET", 3) == 0)
        ret = process_get_request(client_ssl, buffer + 4, false);

    else if (strncmp(buffer, "HEAD", 4) == 0)
        ret = process_get_request(client_ssl, buffer + 5, true);

    else
        ret = send_internal_server_err(client_ssl);


    remove_client_ssl_from_list(client_ssl);
    SSL_shutdown(client_ssl);
    close(SSL_get_fd(client_ssl));
    SSL_free(client_ssl);
}