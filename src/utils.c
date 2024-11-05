#include "server.h"

int set_nonblocking(int fd)
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

int add_fd_to_list(client_list *clist, int fd)
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

int remove_fd_from_list(client_list *clist, int fd)
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