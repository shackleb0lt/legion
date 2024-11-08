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
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

extern size_t g_cache_size;
extern page_cache *g_cache;
extern client_list clist;

FILE *log_file = NULL;

/**
 * Convert a normal socket to a non-blocking socket.
 * Returns 0 on success, -1 otherwise
 */
int set_nonblocking(const int fd)
{
    int ret = 0;
    ret = fcntl(fd, F_GETFL, 0);
    if (ret == -1)
    {
        perror("fcntl: get flags");
        return -1;
    }

    ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
    if (ret == -1)
    {
        perror("fcntl: set non block");
        return -1;
    }
    return 0;
}

void init_client_list()
{
    ssize_t curr = 0;
    clist.last_free = 0;
    for (; curr < MAX_ALIVE_CONN; curr++)
    {
        clist.arr[curr].fd = -1;
        clist.arr[curr].ctx = NULL;
    }
}
/**
 * Function that adds a new client file descriptor to
 * an array to keep track of, and perform graceful shutdown
 * Returns 0 on success, -1 if list is full
 */
int add_fd_to_list(const int fd, SSL_CTX * ctx)
{
    ssize_t curr = 0;
    // If last_free is -1 it means list is full
    if (clist.last_free == -1)
    {
        fprintf(stderr, "Client queue is full, rejecting connection %d\n", fd);
        return -1;
    }

    // Add new file descriptor to last know free index
    clist.arr[clist.last_free].fd = fd;
    clist.arr[clist.last_free].ctx = ctx;

    // Update the free index for next call
    curr = clist.last_free + 1;
    for (; curr < MAX_ALIVE_CONN; curr++)
    {
        if (clist.arr[curr].fd == -1)
            break;
    }

    clist.last_free = curr;
    if (curr >= MAX_ALIVE_CONN)
    {
        clist.last_free = -1;
    }
    return 0;
}

/**
 * Function to remove a client file descriptor from the
 * an array whenever the connection is closed.
 * Returns 0 on success, -1 if client not found.
 */
int remove_fd_from_list(const int fd)
{
    ssize_t curr = 0;
    for (; curr < MAX_ALIVE_CONN; curr++)
    {
        if (clist.arr[curr].fd == fd)
            break;
    }

    // Client not found
    if (curr >= MAX_ALIVE_CONN)
        return -1;

    // Reset the file descriptor to -1
    // Update the last_free position if necessary
    clist.arr[curr].fd = -1;
    clist.arr[clist.last_free].ctx = NULL;

    if (curr < clist.last_free)
        clist.last_free = curr;

    return 0;
}

/**
 * Debug function to write the logs to a file
 * Currently work in progress
 */
void debug_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vfprintf(log_file, fmt, args);
    fflush(log_file);

    va_end(args);
}

/**
 * Check if previous log file existed,
 * if yes rename it to a backup file
 * and create new file for logging
 */
int initiate_logging()
{
    if (access(DEBUG_LOG_FILE, F_OK) == 0)
    {
        rename(DEBUG_LOG_FILE, DEBUG_LOG_OLD);
    }

    log_file = fopen(DEBUG_LOG_FILE, "w");
    if (log_file == NULL)
    {
        perror("fopen");
        return -1;
    }

    LOG("Logging is enabled");
    return 0;
}

/**
 * Clear write buffered log_file contents to memory
 * and close the file pointer
 */
void shutdown_loggging()
{
    if (log_file == NULL)
        return;

    fflush(log_file);
    fclose(log_file);
    log_file = NULL;
}

/**
 * Function to perform cache cleanup
 * Releases all dynamically allocated memory and
 * open file descriptors of asset files
 */
void free_cache()
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
size_t recursive_read(const char *root_path, const char *rel_path, size_t curr_count)
{
    struct dirent *entry;
    struct stat statbuf;
    DIR *dir = NULL;
    bool is_insert = true;
    char fullpath[PATH_MAX];
    char new_rel_path[PATH_MAX];

    if (g_cache == NULL)
        is_insert = false;

    dir = opendir(root_path);
    if (dir == NULL)
    {
        perror("opendir");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct path for files or nested directory calls
        snprintf(fullpath, PATH_MAX - 1, "%s/%s", root_path, entry->d_name);
        if (rel_path[0] == '\0')
            snprintf(new_rel_path, PATH_MAX - 1, "%s", entry->d_name);
        else
            snprintf(new_rel_path, PATH_MAX - 1, "%s/%s", rel_path, entry->d_name);

        // Retrieve entry info
        if (stat(fullpath, &statbuf) == -1)
        {
            perror("stat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            // If entry is a directory then recuse on it
            curr_count = recursive_read(fullpath, new_rel_path, curr_count);
        }
        else
        {
            if (is_insert)
            {
                // Need to perform error checking here in future
                g_cache[curr_count].file_name = strdup(new_rel_path);
                g_cache[curr_count].file_size = (size_t)statbuf.st_size;
                g_cache[curr_count].fd = open(fullpath, O_RDONLY);
                LOG("Adding file %s to cache", new_rel_path);
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
    file_count = recursive_read(root_path, "", 0);
    if (file_count == 0)
    {
        fprintf(stderr, "No assets found at %s\n", root_path);
        return 0;
    }

    // Allocates an array of struct and intitalizes it to zero
    g_cache = (page_cache *)calloc(file_count, sizeof(page_cache));
    if (g_cache == NULL)
    {
        perror("calloc");
        return 0;
    }
    return recursive_read(root_path, "", 0);
}

/**
 * Retrive a cache entry given a potential
 * filepath relative to asset directory
 * Returns NULL if unable to find a matching cache entry
 */
page_cache *get_page_cache(const char *path)
{
    size_t curr = 0;
    if (*path == '\0')
        return get_page_cache("index.html");
    for (; curr < g_cache_size; curr++)
    {
        if (strcmp(g_cache[curr].file_name, path) == 0)
            return &g_cache[curr];
    }
    return NULL;
}