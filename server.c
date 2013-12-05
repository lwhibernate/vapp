/*
 * server.c
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>

#include "server.h"
#include "vapp.h"

typedef enum {
    ServerSockInit,
    ServerSockAccept,
    ServerSockDone,
    ServerSockError

} ServerSockStatus;

extern int app_running;

Server* new_server(const char* name, const char* path)
{
    Server* server = (Server*) calloc(1, sizeof(Server));

    //TODO: handle errors here

    strncpy(server->name, name, NAMELEN);
    strncpy(server->path, path ? path : VAPP_SOCK_NAME, PATH_MAX);
    server->status = INSTANCE_CREATED;

    return server;
}

static int init_server(Server* server)
{
    struct sockaddr_un un;
    size_t len;

    if (server->status != INSTANCE_CREATED)
        return 0;

    unlink(server->path); // remove if exists

    // Create the socket
    if ((server->sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, server->path);

    len = sizeof(un.sun_family) + strlen(server->path);

    // Bind
    if (bind(server->sock, (struct sockaddr *) &un, len) == -1) {
        perror("bind");
        return -1;
    }

    // Listen
    if (listen(server->sock, 1) == -1) {
        perror("listen");
        return -1;
    }

    init_fd_list(&server->fd_list,FD_LIST_SELECT_5);

    server->status = INSTANCE_INITIALIZED;

    return 0;
}

static int end_server(Server* server)
{
    if (server->status != INSTANCE_INITIALIZED)
        return 0;

    // Close and unlink the socket
    close(server->sock);
    unlink(VAPP_SOCK_NAME);

    server->status = INSTANCE_END;

    return 0;
}

static int receive_sock_server(FdNode* node)
{
    Server* server = (Server*) node->context;
    int sock = node->fd;
    ServerMsg msg;
    ServerSockStatus status = ServerSockAccept;
    int r;

    msg.fd_num = sizeof(msg.fds)/sizeof(int);

    r = vhost_user_recv_fds(sock, &msg.msg, msg.fds, &msg.fd_num);
    if (r < 0) {
        perror("recv");
        status = ServerSockError;
    } else if (r == 0) {
        status = ServerSockDone;
        del_fd_list(&server->fd_list, FD_READ, sock);
        close(sock);
    }

    if (status == ServerSockAccept) {
#ifdef DUMP_PACKETS
        dump_vappmsg(&msg.msg);
#endif
        r = 0;
        // Handle the packet to the registered server backend
        if (server->handlers.in_handler) {
            void* ctx = server->handlers.context;
            r = server->handlers.in_handler(ctx, &msg);
            if ( r < 0) {
                fprintf(stderr, "Error processing message: %s\n",
                        cmd_from_vappmsg(&msg.msg));
                status = ServerSockError;
            }
        } else {
            // ... or just dump it for debugging
            dump_vappmsg(&msg.msg);
        }

        if (status == ServerSockAccept) {
            // in_handler will tell us if we need to reply
            if (r > 0) {
                if (vhost_user_send_fds(sock, &msg.msg, 0, 0) < 0) {
                    perror("send");
                    status = ServerSockError;
                }
            }
        }

    }

    return status;
}

static int accept_sock_server(FdNode* node)
{
    int sock;
    struct sockaddr_un un;
    socklen_t len = sizeof(un);
    ServerSockStatus status = ServerSockInit;
    Server* server = (Server*)node->context;

    // Accept connection on the server socket
    if ((sock = accept(server->sock, (struct sockaddr *) &un, &len)) == -1) {
        perror("accept");
    } else {
        status = ServerSockAccept;
    }

    add_fd_list(&server->fd_list, FD_READ, sock, (void*) server, receive_sock_server);

    return status;
}

static int loop_server(Server* server)
{
    add_fd_list(&server->fd_list, FD_READ, server->sock, (void*) server,
            accept_sock_server);

    // externally modified
    app_running = 1;

    while (app_running) {
        traverse_fd_list(&server->fd_list);
        if (server->handlers.poll_handler) {
            server->handlers.poll_handler(server->handlers.context);
        }
    }

    return 0;
}

int set_handler_server(Server* server, AppHandlers* handlers)
{
    memcpy(&server->handlers,handlers, sizeof(AppHandlers));

    return 0;
}

int run_server(Server* server)
{
    if (init_server(server) != 0)
        return -1;

    loop_server(server);

    end_server(server);

    return 0;
}