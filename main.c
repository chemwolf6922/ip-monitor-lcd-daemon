#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "tev/tev.h"
#include "ip_monitor.h"
#include "ip_renderer.h"

static tev_handle_t tev = NULL;
static ip_monitor_t* ip_monitor = NULL;
static ip_renderer_t* ip_renderer = NULL;
static char* lcd_device = NULL;
static struct in_addr current_ip = {0};
static tev_timeout_handle_t display_ip_retry_timer = NULL;

static void on_ip_change(const struct in_addr* ip, void* ctx);
static void try_display_ip(void* );
static int display_ip();

int main(int argc, char const *argv[])
{
    int opt;
    char* iface = NULL;
    char* font_path = NULL;
    while((opt = getopt(argc, (char**)argv, "i:d:f:")) != -1)
    {
        switch(opt)
        {
            case 'i':
                iface = optarg;
                break;
            case 'd':
                lcd_device = optarg;
                break;
            case 'f':
                font_path = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -i iface -d lcd_device -f font_path\n", argv[0]);
                return -1;
        }
    }
    if(!iface || !lcd_device || !font_path)
    {
        fprintf(stderr, "Usage: %s -i iface -d lcd_device -f font_path\n", argv[0]);
        return -1;
    }

    tev = tev_create_ctx();
    if(!tev)
        return -1;
    
    ip_monitor = ip_monitor_new(tev, iface);
    if(!ip_monitor)
        return -1;
    ip_monitor->callbacks.on_ip_changed = on_ip_change;

    ip_renderer = ip_renderer_new(font_path);
    if(!ip_renderer)
        return -1;

    ip_monitor->get_ip(ip_monitor, &current_ip);
    try_display_ip(NULL);

    tev_main_loop(tev);

    tev_free_ctx(tev);

    return 0;
}

static void on_ip_change(const struct in_addr* ip, void* ctx)
{
    memcpy(&current_ip, ip, sizeof(current_ip));
    try_display_ip(NULL);
}

static void try_display_ip(void* )
{
    tev_clear_timeout(tev, display_ip_retry_timer);
    if(display_ip() != 0)
    {
        display_ip_retry_timer = tev_set_timeout(tev, try_display_ip, NULL, 5000);
    }
}

static int display_ip()
{
    static lcd_frame_buffer_t fb = {0};
    memset(&fb, 0, sizeof(fb));
    if(ip_renderer->render_ip(ip_renderer, &fb, &current_ip) < 0)
        return -1;
    ip_renderer->rotate_clockwise(ip_renderer, &fb);
    struct stat st = {0};
    if(stat(lcd_device, &st) != 0)
        return -1;
    FILE* lcd = fopen(lcd_device, "w");
    if(!lcd)
        return -1;
    rewind(lcd);
    size_t write_len = fwrite(fb.data, 1, sizeof(fb.data), lcd);
    fflush(lcd);
    fclose(lcd);
    if(write_len != sizeof(fb.data))
        return -1;

    return 0;
}
