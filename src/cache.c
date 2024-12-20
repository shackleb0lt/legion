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

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <zlib.h>
#include <brotli/encode.h>

static size_t g_cache_size;
static page_cache *g_cache;
const page_cache *page_404 = NULL;
const page_cache *page_500 = NULL;

#define CHUNK_SIZE 16384

/**
int compress_file_zlib(const char *input_path)
{
    FILE *input = fopen(input_path, "rb");
    if (!input)
    {
        perror("Failed to open input file");
        return -1;
    }

    FILE *output = fopen(output_path, "wb");
    if (!output)
    {
        perror("Failed to open output file");
        fclose(input);
        return -1;
    }

    int result = 0;
    z_stream strm = {0};
    if (deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK)
    {
        fprintf(stderr, "Failed to initialize zlib\n");
        result = -1;
        goto cleanup;
    }

    unsigned char in[CHUNK_SIZE];
    unsigned char out[CHUNK_SIZE];

    int flush;
    do
    {
        strm.avail_in = fread(in, 1, CHUNK_SIZE, input);
        if (ferror(input))
        {
            fprintf(stderr, "Error reading input file\n");
            result = -1;
            break;
        }
        flush = feof(input) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        do
        {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = out;
            deflate(&strm, flush);
            fwrite(out, 1, CHUNK_SIZE - strm.avail_out, output);
        } while (strm.avail_out == 0);

    } while (flush != Z_FINISH);

    deflateEnd(&strm);
}

int compress_file_brotli(const char *input_path, const char *output_path)
{
    FILE *input = fopen(input_path, "rb");
    if (!input)
    {
        perror("Failed to open input file");
        return -1;
    }

    FILE *output = fopen(output_path, "wb");
    if (!output)
    {
        perror("Failed to open output file");
        fclose(input);
        return -1;
    }

    int result = 0;

        // Brotli compression
    BrotliEncoderState *state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
    if (!state)
    {
        fprintf(stderr, "Failed to initialize Brotli encoder\n");
        result = -1;
        goto cleanup;
    }
    BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, 11);
    BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, 22);

    unsigned char in[CHUNK_SIZE];
    unsigned char out[CHUNK_SIZE];
    size_t available_in, available_out, total_out;
    const unsigned char *next_in;
    unsigned char *next_out;

    int is_eof = 0;
    while (!is_eof)
    {
        available_in = fread(in, 1, CHUNK_SIZE, input);
        if (ferror(input))
        {
            fprintf(stderr, "Error reading input file\n");
            result = -1;
            break;
        }
        is_eof = feof(input);
        next_in = in;

        BrotliEncoderOperation op = is_eof ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS;
        do
        {
            available_out = CHUNK_SIZE;
            next_out = out;

            if (!BrotliEncoderCompressStream(state, op, &available_in, &next_in, &available_out, &next_out, &total_out))
            {
                fprintf(stderr, "Brotli compression failed\n");
                result = -1;
                break;
            }

            fwrite(out, 1, CHUNK_SIZE - available_out, output);
        } while (available_out == 0);

        if (result == -1)
            break;
    }

    BrotliEncoderDestroyInstance(state);
    

cleanup:
    fclose(input);
    fclose(output);
    return result;
}
 */
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
            free((void *)g_cache[i].file_name);
        if (g_cache[i].fd > 0)
            close(g_cache[i].fd);
        if (g_cache[i].file_map != NULL)
            munmap(g_cache[i].file_map, (size_t)g_cache[i].file_size);
    }
    free(g_cache);
    g_cache = NULL;
    g_cache_size = 0;
}

/**
 * Returns a string literal describing the mime type of file.
 * Returns default type if the file extension is unknown 
 */
const char *get_mime_type(const char *filename)
{
    size_t i = 0;
    char ext[8];
    const char *ptr = strrchr(filename, '.');
    if (ptr == NULL)
    {
        LOG_ERROR("%s: mime type not defined for %s", filename);
        return DEFAULT_MIME_T;
    }

    ptr++;
    while ((*ptr) != '\0' && i < 7)
    {
        ext[i] = (char)tolower(*ptr);
        ptr++;
        i++;
    }
    ext[i] = '\0';

    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html";
    if (strcmp(ext, "jpeg") == 0 || strcmp(ext, "jpg") == 0) return "image/jpg";
    if (strcmp(ext, "css")  == 0)   return "text/css"; 
    if (strcmp(ext, "js")   == 0)   return "application/javascript";
    if (strcmp(ext, "json") == 0)   return "application/json";
    if (strcmp(ext, "pdf")  == 0)   return "application/pdf";
    if (strcmp(ext, "txt")  == 0)   return "text/plain";
    if (strcmp(ext, "gif")  == 0)   return "image/gif";
    if (strcmp(ext, "png")  == 0)   return "image/png";
    if (strcmp(ext, "ico")  == 0)   return "image/vnd.microsoft.icon";

    return DEFAULT_MIME_T;
}

/**
 * Recursively add each file in the root directory
 * and sub directories to the cache for faster access
 * If g_cache is NULL while calling this function then
 * it only counts and returns the number of files.
 */
static size_t recursive_read(const char *root_path, size_t curr_count, const long page_size)
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
            curr_count = recursive_read(fullpath, curr_count, page_size);
            fullpath[path_len] = '\0';
            continue;
        }

        if (is_insert == false)
        {
            curr_count++;
            continue;
        }

        // Need to perform error checking here in future
        g_cache[curr_count].file_name = strdup(fullpath);
        g_cache[curr_count].file_size = statbuf.st_size;
        g_cache[curr_count].fd = open(fullpath, O_RDONLY);
        g_cache[curr_count].file_map = NULL;
        g_cache[curr_count].mime_type = get_mime_type(fullpath);

        if (g_cache[curr_count].file_size <= page_size)
        {
            g_cache[curr_count].file_map = mmap(NULL, (size_t)g_cache[curr_count].file_size, PROT_READ, MAP_PRIVATE, g_cache[curr_count].fd, 0);
            if (g_cache[curr_count].file_map == NULL)
            {
                LOG_ERROR("%s: mmap failed for %s", __func__, fullpath);
            }
        }

        if (g_cache[curr_count].file_map != NULL)
        {
            LOG_INFO("mmap successful file: %s size: %lu", fullpath, g_cache[curr_count].file_size);
            close(g_cache[curr_count].fd);
            g_cache[curr_count].fd = -1;
        }

        LOG_INFO("Adding file %s of type %s to cache", fullpath, g_cache[curr_count].mime_type);
        curr_count++;
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
    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size < 0)
        page_size = DEFAULT_PAGE_SIZE;

    file_count = recursive_read(root_path, 0, page_size);
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
    g_cache_size = recursive_read(root_path, 0, page_size);
    page_404 = get_page_cache(ERROR_404_PAGE);
    page_500 = get_page_cache(ERROR_500_PAGE);
    if (page_404 == NULL || page_500 == NULL)
    {
        LOG_ERROR("page 404 and page 500 are not defined");
        release_cache();
        return 0;
    }

    return g_cache_size;
}

/**
 * Retrive a cache entry given a potential
 * filepath relative to asset directory
 * Returns NULL if unable to find a matching cache entry
 */
const page_cache *get_page_cache(const char *path)
{
    size_t curr = 0;
    const char *curr_path = NULL;
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