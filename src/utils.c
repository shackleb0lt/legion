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

int add_fd_to_list(client_list *clist, const int fd)
{
    ssize_t curr = 0;
    if(clist->last_free == -1)
    {
        fprintf(stderr, "Client queue is full, rejecting connection %d\n", fd);
        return -1;
    }
    
    clist->fd_list[clist->last_free] = fd;
    curr = clist->last_free + 1;
    for(; curr < MAX_ALIVE_CONN; curr++)
    {
        if(clist->fd_list[curr] == -1)
            break;
    }

    clist->last_free = curr;
    if(curr >= MAX_ALIVE_CONN)
    {
        clist->last_free = -1;
    }
    return 0;
}

int remove_fd_from_list(client_list *clist, const int fd)
{
    ssize_t curr = 0;
    for(; curr < MAX_ALIVE_CONN; curr++)
    {
        if(clist->fd_list[curr] == fd)
            break;
    }

    if(curr >= MAX_ALIVE_CONN)
        return -1;
    
    clist->fd_list[curr] = -1;

    if(curr < clist->last_free)
        clist->last_free = curr;

    return 0;
}

int debug_log(const char *fmt, ...)
{
    int bytes_written = 0;
    va_list args;
    va_start(args, fmt);

    bytes_written = vprintf(fmt, args);
    fflush(stdout);
    
    va_end(args);
    return bytes_written;
}

size_t recursive_read(const char *root_path, const char* rel_path, size_t curr_count)
{
    struct dirent *entry;
    struct stat statbuf;
    DIR *dir = NULL;
    bool is_insert = true;
    char fullpath[PATH_MAX];
    char new_rel_path[PATH_MAX];

    if(g_cache == NULL)
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
        
        snprintf(fullpath, PATH_MAX - 1, "%s/%s", root_path, entry->d_name);
        if(rel_path[0] == '\0')
            snprintf(new_rel_path, PATH_MAX - 1, "%s", entry->d_name);
        else
            snprintf(new_rel_path, PATH_MAX - 1, "%s/%s", rel_path, entry->d_name);

        if (stat(fullpath, &statbuf) == -1)
        {
            perror("stat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            curr_count = recursive_read(fullpath, new_rel_path, curr_count);
        }
        else
        {   
            if(is_insert)
            {
                g_cache[curr_count].file_name = strdup(new_rel_path);
                g_cache[curr_count].file_size = (size_t) statbuf.st_size;
                g_cache[curr_count].fd = open(fullpath, O_RDONLY);
            }
            curr_count++;
        }
    }
    closedir(dir);
    return curr_count;
}

size_t initiate_cache(const char *root_path)
{
    size_t file_count = 0;
    file_count = recursive_read(root_path, "", 0);
    if(file_count == 0)
    {
        fprintf(stderr, "No assets found at %s\n", root_path);
        return 0;
    }

    g_cache = (page_cache *) calloc(file_count, sizeof(page_cache));
    if(g_cache == NULL)
    {
        perror("calloc");
        return 0;
    }
    return recursive_read(root_path, "", 0);
}

page_cache *get_page_cache(const char * path)
{
    size_t curr = 0;
    if(*path == '\0')
        return get_page_cache("index.html");
    for(; curr < g_cache_size; curr++)
    {
        if(strcmp(g_cache[curr].file_name, path) == 0)
            return &g_cache[curr];
    }
    return NULL;
}