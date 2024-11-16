#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include "../tev/tev.h"
#include "../ip_monitor.h"

static void on_ip_change(const struct in_addr* ip, void* ctx)
{
    char ip_str[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, ip, ip_str, sizeof(ip_str));
    printf("ip changed: %s\n", ip_str);
}

static void end_test(void* ctx)
{
    ip_monitor_t* ip_monitor = (ip_monitor_t*)ctx;
    ip_monitor->destroy(ip_monitor);
}

int main(int argc, char const *argv[])
{
    if(argc < 2)
        return -1;
    tev_handle_t tev = tev_create_ctx();
    if(!tev)
        return -1;
    ip_monitor_t* ip_monitor = ip_monitor_new(tev, argv[1]);
    if(!ip_monitor)
        return -1;
    ip_monitor->callbacks.on_ip_changed = on_ip_change;
    struct in_addr ip;
    ip_monitor->get_ip(ip_monitor, &ip);
    char ip_str[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &ip, ip_str, sizeof(ip_str));
    printf("current ip: %s\n", ip_str);

    tev_set_timeout(tev, end_test, ip_monitor, 10000);

    tev_main_loop(tev);

    tev_free_ctx(tev);

    return 0;
}

