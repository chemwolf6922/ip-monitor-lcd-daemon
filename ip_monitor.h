#pragma once

#include <netinet/in.h>
#include "tev/tev.h"

#define INVALID_IP (0)

typedef struct ip_monitor_s ip_monitor_t;
struct ip_monitor_s
{
    int (*get_ip)(ip_monitor_t* self, struct in_addr* ip);
    void (*destroy)(ip_monitor_t* self);
    struct
    {
        void (*on_ip_changed)(const struct in_addr* ip, void* ctx);
        void* on_ip_changed_ctx;
    } callbacks;
};

ip_monitor_t* ip_monitor_new(tev_handle_t tev, const char* iface);
