#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include "ip_monitor.h"

// Only to make intellisense happy
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

typedef struct
{
    ip_monitor_t base;
    tev_handle_t tev;
    char iface[IFNAMSIZ];
    struct in_addr ip;
    int netlink_fd;
} ip_monitor_impl_t;

static void ip_monitor_destroy(ip_monitor_t* base);
static int ip_monitor_get_ip(ip_monitor_t* base, struct in_addr* ip);
static void netlink_read_handler(void* ctx);

ip_monitor_t* ip_monitor_new(tev_handle_t tev, const char* iface)
{
    if(!tev || !iface)
        return NULL;
    ip_monitor_impl_t* this = malloc(sizeof(ip_monitor_impl_t));
    if(!this)
        goto error;
    memset(this, 0, sizeof(ip_monitor_impl_t));
    this->base.get_ip = ip_monitor_get_ip;
    this->base.destroy = ip_monitor_destroy;
    this->tev = tev;
    strncpy(this->iface, iface, IFNAMSIZ - 1);
    this->netlink_fd = -1;
    /** open netlink */
    this->netlink_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if(this->netlink_fd < 0)
        goto error;
    int flags = fcntl(this->netlink_fd, F_GETFL, 0);
    if(flags < 0)
        goto error;
    if(fcntl(this->netlink_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        goto error;
    struct sockaddr_nl sa;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
    if(bind(this->netlink_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0)
        goto error;
    if(tev_set_read_handler(this->tev, this->netlink_fd, netlink_read_handler, this) < 0)
        goto error;
    return (ip_monitor_t*)this;
error:
    ip_monitor_destroy((ip_monitor_t*)this);
    return NULL;
}

static void ip_monitor_destroy(ip_monitor_t* base)
{
    ip_monitor_impl_t* this = (ip_monitor_impl_t*)base;
    if(this)
    {
        if(this->netlink_fd >= 0)
        {
            tev_set_read_handler(this->tev, this->netlink_fd, NULL, NULL);
            close(this->netlink_fd);
        }
        free(this);
    }
}

static int ip_monitor_get_ip(ip_monitor_t* base, struct in_addr* ip)
{
    if(!ip)
        return -1;
    ip_monitor_impl_t* this = (ip_monitor_impl_t*)base;
    struct ifaddrs* ifaddr = NULL;
    if(getifaddrs(&ifaddr) < 0)
        return -1;
    ip->s_addr = INVALID_IP;
    for(struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr == NULL)
            continue;
        if(ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if(strcmp(ifa->ifa_name, this->iface) != 0)
            continue;
        struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
        memcpy(ip, &addr->sin_addr, sizeof(*ip));
        memcpy(&this->ip, &addr->sin_addr, sizeof(*ip));
        break;
    }
    freeifaddrs(ifaddr);
    return 0;
}

static void netlink_read_handler(void* ctx)
{
    ip_monitor_impl_t* this = (ip_monitor_impl_t*)ctx;
    char buffer[4096];
    ssize_t len = recv(this->netlink_fd, buffer, sizeof(buffer), 0);
    if(len < 0)
    {
        abort();
        return;
    }
    struct nlmsghdr* nlh = NULL;
    for(nlh = (struct nlmsghdr*)buffer; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len))
    {
        if(nlh->nlmsg_type == NLMSG_DONE)
            break;
        if(nlh->nlmsg_type == RTM_NEWADDR || nlh->nlmsg_type == RTM_DELADDR)
        {
            struct ifaddrmsg* ifa = NLMSG_DATA(nlh);
            
            /** filter interface name */
            char ifname[IFNAMSIZ];
            if(!if_indextoname(ifa->ifa_index, ifname))
                continue;
            if(strcmp(ifname, this->iface) != 0)
                continue;

            /** Only care about ipv4 address */
            if(ifa->ifa_family != AF_INET)
                continue;

            struct rtattr* rta = IFA_RTA(ifa);
            int rta_len = IFA_PAYLOAD(nlh);
            for(; RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len))
            {
                if(rta->rta_type != IFA_LOCAL)
                    continue;
                struct in_addr ip;
                if(nlh->nlmsg_type == RTM_DELADDR)
                {
                    ip.s_addr = INVALID_IP;
                }
                else
                {
                    memcpy(&ip, RTA_DATA(rta), sizeof(ip));
                }
                if(memcmp(&this->ip, &ip, sizeof(ip)) == 0)
                    continue;
                this->ip = ip;
                if(this->base.callbacks.on_ip_changed)
                    this->base.callbacks.on_ip_changed(&ip, this->base.callbacks.on_ip_changed_ctx);
            }
        }       
    }
}
