/*
 * vapp_client.h
 */

#ifndef VAPP_CLIENT_H_
#define VAPP_CLIENT_H_

#include "client.h"
#include "stat.h"
#include "vring.h"

struct VappClientStruct {
    Client* client;
    VhostUserMemory memory;
    uint64_t features;           // features negotiated with the server

    struct vhost_vring* vring_table_shm[VAPP_CLIENT_VRING_NUM];

    VringTable vring_table;
    size_t  page_size;

    Stat stat;
};

typedef struct VappClientStruct VappClient;

VappClient* new_vapp_client(const char* name, const char* path);

int init_vapp_client(VappClient* vapp_client);
int end_vapp_client(VappClient* vapp_client);

// Test-only procedure
int run_vapp_client(VappClient* vapp_client);

#endif /* VAPP_CLIENT_H_ */
