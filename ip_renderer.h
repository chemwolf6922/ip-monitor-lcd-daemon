#pragma once

#include <stdint.h>
#include <netinet/in.h>

#define FRAME_BUFFER_WIDTH 160
#define FRAME_BUFFER_HEIGHT 50
#define FRAME_BUFFER_PIXEL_SIZE 3

typedef union
{
    /** BGR888 */
    struct
    {
        uint8_t B;
        uint8_t G;
        uint8_t R;
    } __attribute__((packed)) pixels[FRAME_BUFFER_WIDTH * FRAME_BUFFER_HEIGHT]; 
    uint8_t data[FRAME_BUFFER_WIDTH * FRAME_BUFFER_HEIGHT * 3];
}__attribute__((packed)) lcd_frame_buffer_t;

typedef struct ip_renderer_s ip_renderer_t;
struct ip_renderer_s
{
    int (*render_ip)(ip_renderer_t* self, lcd_frame_buffer_t* fb, const struct in_addr* ip);
    void (*rotate_clockwise)(ip_renderer_t* self, lcd_frame_buffer_t* fb);
    void (*destroy)(ip_renderer_t* self);
};

ip_renderer_t* ip_renderer_new(const char* font_path);

