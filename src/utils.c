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

#include <dirent.h>
#include <sys/stat.h>
#include <sys/resource.h>

static size_t g_cache_size;
static page_cache *g_cache;
static client_info clist[MAX_FD_COUNT];

int ssl_log_err(const char *errstr, size_t len, void *u)
{
    (void)len;
    (void)u;
#ifndef DEBUG
    (void)errstr;
#endif
    LOG_ERROR("%s", errstr);
    return 0;
}

/**
 * Function that limits the total open file descriptor
 * for this process to MAX_FD_COUNT
 */
int set_fd_limit()
{
    int ret = 0;
    struct rlimit rl;

    // Get the current limit
    ret = getrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: Retrieval getrlimit", __func__);
        return -1;
    }

    if (rl.rlim_cur == MAX_FD_COUNT && rl.rlim_max == MAX_FD_COUNT)
        return 0;

    // Set a new soft and hard limit for open files
    rl.rlim_cur = MAX_FD_COUNT;
    rl.rlim_max = MAX_FD_COUNT;

    // Set the new limit
    ret = setrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: setrlimit", __func__);
        return -1;
    }

    // Verify the change
    ret = getrlimit(RLIMIT_NOFILE, &rl);
    if (ret == -1)
    {
        LOG_ERROR("%s: Verify getrlimit", __func__);
        return -1;
    }

    if (rl.rlim_cur != MAX_FD_COUNT || rl.rlim_max != MAX_FD_COUNT)
    {
        LOG_ERROR("%s: Verification failed soft %ld, hard = %ld\n",
                  __func__, rl.rlim_cur, rl.rlim_max);
        return -1;
    }
    return 0;
}

/**
 * Convert a normal socket to a non-blocking socket
 * Returns 0 on success, -1 otherwise
 */
int set_nonblocking(const int fd)
{
    int ret = 0;
    ret = fcntl(fd, F_GETFL, 0);
    if (ret == -1)
    {
        LOG_ERROR("%s fcntl get", __func__);
        return -1;
    }

    ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
    if (ret == -1)
    {
        LOG_ERROR("%s fcntl set", __func__);
        return -1;
    }
    return 0;
}

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
 *
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
 *
 */
void remove_client_info(client_info * cinfo)
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
 * 
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

/**
 * Function to perform cache cleanup
 * Releases all dynamically allocated memory and
 * open file descriptors of asset files
 */
void release_cache()
{
    size_t i = 0;
    if (g_cache == NULL)
        return;
    for (i = 0; i < g_cache_size; i++)
    {
        if (g_cache[i].file_name != NULL)
            free(g_cache[i].file_name);
        if (g_cache[i].fd > 0)
            close(g_cache[i].fd);
    }
    free(g_cache);
    g_cache = NULL;
    g_cache_size = 0;
}

/**
 * Recursively add each file in the root directory
 * and sub directories to the cache for faster access
 * If g_cache is NULL while calling this function then
 * it only counts and returns the number of files.
 */
static size_t recursive_read(const char *root_path, size_t curr_count)
{
    struct dirent *entry;
    struct stat statbuf;
    DIR *dir = NULL;
    bool is_insert = true;
    char fullpath[PATH_MAX];
    ssize_t path_len = 0;

    if (g_cache == NULL)
        is_insert = false;

    dir = opendir(root_path);
    if (dir == NULL)
    {
        LOG_ERROR("%s opendir", __func__);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct path for files or nested directory calls
        path_len = snprintf(fullpath, PATH_MAX - 1, "%s%s", root_path, entry->d_name);
        if (path_len < 0 || path_len >= PATH_MAX)
        {
            LOG_ERROR("Max recursion depth at path %s", root_path);
            return 0;
        }

        // Retrieve entry info
        if (stat(fullpath, &statbuf) == -1)
        {
            LOG_ERROR("%s stat", __func__);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            // If entry is a directory then recurse on it
            // Add a trailing / before passing
            // Remove it after exiting for further use
            fullpath[path_len] = '/';
            fullpath[path_len + 1] = '\0';
            curr_count = recursive_read(fullpath, curr_count);
            fullpath[path_len] = '\0';
        }
        else
        {
            if (is_insert)
            {
                // Need to perform error checking here in future
                g_cache[curr_count].file_name = strdup(fullpath);
                g_cache[curr_count].file_size = (size_t)statbuf.st_size;
                g_cache[curr_count].fd = open(fullpath, O_RDONLY);
                LOG_INFO("Adding file %s to cache", fullpath);
            }
            curr_count++;
        }
    }
    closedir(dir);
    return curr_count;
}

/**
 * Construct the cache by calling above function twice
 * First to calculate the number of entries
 * Later to populate the cache entries
 */
size_t initiate_cache(const char *root_path)
{
    size_t file_count = 0;
    file_count = recursive_read(root_path, 0);
    if (file_count == 0)
    {
        fprintf(stderr, "No assets found at %s\n", root_path);
        return 0;
    }

    // Allocates an array of struct and intitalizes it to zero
    g_cache = (page_cache *)calloc(file_count, sizeof(page_cache));
    if (g_cache == NULL)
    {
        LOG_ERROR("%s calloc", __func__);
        return 0;
    }
    g_cache_size = recursive_read(root_path, 0);
    return g_cache_size;
}

/**
 * Retrive a cache entry given a potential
 * filepath relative to asset directory
 * Returns NULL if unable to find a matching cache entry
 */
page_cache *get_page_cache(const char *path)
{
    size_t curr = 0;
    char *curr_path = NULL;
    if (*path == '\0')
        return get_page_cache(INDEX_PAGE);

    for (; curr < g_cache_size; curr++)
    {
        curr_path = g_cache[curr].file_name + DEFAULT_ASSET_LEN - 1;
        if (strcmp(curr_path, path) == 0)
            return &g_cache[curr];
    }
    return NULL;
}