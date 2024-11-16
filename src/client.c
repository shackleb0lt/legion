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

static client_info clist[MAX_FD_COUNT];

/**
 * Initiated list of to store incoming client connections
 */
void init_client_list()
{
    size_t curr = 0;
    for (curr = 0; curr < MAX_FD_COUNT; curr++)
    {
        clist[curr].fd = -1;
        clist[curr].ssl = NULL;
        clist[curr].keep_alive = false;
    }
}

/**
 * Releases all remaining connected clients at exit
 */
void cleanup_client_list()
{
    size_t curr = 0;

    for (curr = 0; curr < MAX_FD_COUNT; curr++)
    {
        if (clist[curr].ssl != NULL)
        {
            SSL_free(clist[curr].ssl);
            clist[curr].ssl = NULL;
        }

        if (clist[curr].fd > 0)
        {
            close(clist[curr].fd);
            clist[curr].fd = -1;
        }
    }
}

/**
 * Stores the metadata abour incoming client in an array for future use.
 * Since the file descriptor for each client is unique and won't exceed MAX_FD_COUNT
 * the clients are stored directly at the location indexed by file descriptor 
 */
int add_client_info(const int client_fd, SSL *client_ssl)
{
    if (client_fd < 0 || client_fd >= MAX_FD_COUNT || client_ssl == NULL)
    {
        LOG_ERROR("%s: Invalid client details received", __func__);
        return -1;
    }
    clist[client_fd].fd = client_fd;
    clist[client_fd].ssl = client_ssl;
    clist[client_fd].keep_alive = false;
    return 0;
}

/**
 * Removes client info from array
 */
void remove_client_info(client_info *cinfo)
{
    if (cinfo == NULL)
    {
        LOG_ERROR("%s: Invalid client details received", __func__);
        return;
    }

    SSL_shutdown(cinfo->ssl);
    close(cinfo->fd);
    SSL_free(cinfo->ssl);

    cinfo->fd = -1;
    cinfo->ssl = NULL;
    cinfo->keep_alive = false;
}

/**
 * Reteurns to a pointer to the client info
 */
client_info *get_client_info(const int client_fd)
{
    if (client_fd < 0 || client_fd >= MAX_FD_COUNT || clist[client_fd].fd < 0)
    {
        LOG_ERROR("%s: Invalid client details received", __func__);
        return NULL;
    }
    return &clist[client_fd];
}
