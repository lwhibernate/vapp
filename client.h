/*
 * client.h
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include <limits.h>

#include "common.h"
#include "fd_list.h"
#include "vapp.h"

struct ClientStruct {
    char name[NAMELEN + 1];
    char sock_path[PATH_MAX];
    int status;
    int sock;
    FdList fd_list;
    AppHandlers handlers;
};

typedef struct ClientStruct Client;

Client* new_client(const char* name, const char* path);
int init_client(Client* client);
int end_client(Client* client);
int set_handler_client(Client* client, AppHandlers* handlers);
int loop_client(Client* client);

int vhost_ioctl(Client* client, VhostUserRequest request, ...);

#endif /* CLIENT_H_ */
