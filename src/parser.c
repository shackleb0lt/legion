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

const char * get_string()
{
    return "Hello World\n";
}

int handle_http_request(int client_fd)
{
    ssize_t bytes_read = 0;
    ssize_t bytes_written = 0;
    char buffer[BUFFER_SIZE];
    char resp[] = "HTTP/1.0 200 OK\r\n"
                  "Server: legion\r\n"
                  "Content-type: text/html\r\n\r\n"
                  "<html>Hello, World!</html>\r\n";

    bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
        return -1;

    // buffer[bytes_read] = '\0';
    bytes_written = send(client_fd, resp, strlen(resp), 0);

    if(bytes_written == -1)
        return -1;
    return 0;
}