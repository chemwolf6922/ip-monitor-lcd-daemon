#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <math.h>

#include "ip_renderer.h"
#include "libschrift/schrift.h"
#include "tev/tev.h"

#define DEFAULT_FONT_SIZE 20.0

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct
{
    ip_renderer_t base;
    SFT sft;
} ip_renderer_impl_t;

static void ip_renderer_destroy(ip_renderer_t* base);
static int ip_renderer_render_ip(ip_renderer_t* base, lcd_frame_buffer_t* fb, const struct in_addr* ip);
static void ip_renderer_rotate_clockwise(ip_renderer_t* base, lcd_frame_buffer_t* fb);
static double get_perfect_font_size(SFT* sft, const char* str, int width, int height);
static double get_render_height(SFT* sft, const char* str);
static double get_render_width(SFT* sft, const char* str);
static double render_one_char(SFT* sft, char c, lcd_frame_buffer_t* fb, double x, double y);

ip_renderer_t* ip_renderer_new(const char* font_path)
{
    if(!font_path)
        return NULL;
    ip_renderer_impl_t* this = (ip_renderer_impl_t*)malloc(sizeof(ip_renderer_impl_t));
    if(!this)
        goto error;
    memset(this, 0, sizeof(ip_renderer_impl_t));
    
    this->base.destroy = ip_renderer_destroy;
    this->base.render_ip = ip_renderer_render_ip;
    this->base.rotate_clockwise = ip_renderer_rotate_clockwise;
    
    /** Default size. Will be adjusted on render */
    this->sft.xScale = DEFAULT_FONT_SIZE;
    this->sft.yScale = DEFAULT_FONT_SIZE;
    this->sft.flags = SFT_DOWNWARD_Y;
    this->sft.font = sft_loadfile(font_path);
    if(!this->sft.font)
        goto error;
    return (ip_renderer_t*)this;
error:
    ip_renderer_destroy((ip_renderer_t*)this);
    return NULL;
}

static void ip_renderer_destroy(ip_renderer_t* base)
{
    ip_renderer_impl_t* this = (ip_renderer_impl_t*)base;
    if(this)
    {
        if(this->sft.font)
            sft_freefont(this->sft.font);
        free(this);
    }
}

static int ip_renderer_render_ip(ip_renderer_t* base, lcd_frame_buffer_t* fb, const struct in_addr* ip)
{
    ip_renderer_impl_t* this = (ip_renderer_impl_t*)base;
    if(!this)
        return -1;
    /** convert ip to string */
    char ip_str[INET_ADDRSTRLEN] = {0};
    if(!inet_ntop(AF_INET, ip, ip_str, INET_ADDRSTRLEN))
        return -1;
    /** find the perfect font size */
    double font_size = get_perfect_font_size(&this->sft, ip_str, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
    if(font_size < 0)
        return -1;
    this->sft.xScale = font_size;
    this->sft.yScale = font_size;
    /** get the offsets */
    double render_height = get_render_height(&this->sft, ip_str);
    if(render_height < 0)
        return -1;
    double render_width = get_render_width(&this->sft, ip_str);
    if(render_width < 0)
        return -1;
    /** Be careful with the filling as those may be negative */
    double x_offset = (FRAME_BUFFER_WIDTH - render_width) / 2;
    double y_offset = (FRAME_BUFFER_HEIGHT - render_height) / 2 + render_height;
    /** render the string */
    for(int i = 0; i < strlen(ip_str); i++)
    {
        double x_advance = render_one_char(&this->sft, ip_str[i], fb, x_offset, y_offset);
        if(x_advance < 0)
            return -1;
        x_offset += x_advance;
    }
    return 0;
}

static void ip_renderer_rotate_clockwise(ip_renderer_t* base, lcd_frame_buffer_t* fb)
{
    lcd_frame_buffer_t tmp;
    memcpy(&tmp, fb, sizeof(lcd_frame_buffer_t));
    for(int i = 0; i < FRAME_BUFFER_WIDTH; i++)
    {
        for(int j = 0; j < FRAME_BUFFER_HEIGHT; j++)
        {
            fb->pixels[i * FRAME_BUFFER_HEIGHT + j] = 
                tmp.pixels[(FRAME_BUFFER_HEIGHT - j - 1) * FRAME_BUFFER_WIDTH + i];
        }
    }    
}

static double get_perfect_font_size(SFT* sft, const char* str, int width, int height)
{
    sft->xScale = DEFAULT_FONT_SIZE;
    sft->yScale = DEFAULT_FONT_SIZE;
    double render_height = get_render_height(sft, str);
    if(render_height < 0)
        return -1;
    double render_width = get_render_width(sft, str);
    if(render_width < 0)
        return -1;
    /** scale the larger scale to 95% of the limit */
    double target_height = (double)height * 0.95;
    double target_width = (double)width * 0.95;
    double scale = MAX(render_height / target_height, render_width / target_width);
    sft->xScale /= scale;
    sft->yScale /= scale;
    return sft->xScale;
}

static double get_render_height(SFT* sft, const char* str)
{
    double y_min = INFINITY;
    double y_max = -INFINITY;
    for(int i = 0; i < strlen(str); i++)
    {
        SFT_Glyph gid;
        if(sft_lookup(sft, str[i], &gid) < 0)
            return -1;
        SFT_GMetrics gmtx;
        if(sft_gmetrics(sft, gid, &gmtx) < 0)
            return -1;
        y_min = MIN(y_min, gmtx.yOffset);
        y_max = MAX(y_max, gmtx.yOffset + gmtx.minHeight);
    }
    return y_max - y_min;
}

static double get_render_width(SFT* sft, const char* str)
{
    double width = 0;
    for(int i = 0; i < strlen(str); i++)
    {
        SFT_Glyph gid;
        if(sft_lookup(sft, str[i], &gid) < 0)
            return -1;
        SFT_GMetrics gmtx;
        if(sft_gmetrics(sft, gid, &gmtx) < 0)
            return -1;
        width += gmtx.advanceWidth;
    }
    return width;
}

static double render_one_char(SFT* sft, char c, lcd_frame_buffer_t* fb, double x, double y)
{
    SFT_Glyph gid;
    if(sft_lookup(sft, c, &gid) < 0)
        return -1;
    SFT_GMetrics gmtx;
    if(sft_gmetrics(sft, gid, &gmtx) < 0)
        return -1;
    SFT_Image img = {
        .width = (gmtx.minWidth + 3) & ~3,
        .height = gmtx.minHeight
    };
    uint8_t pixels[img.width * img.height];
    img.pixels = pixels;
    if(sft_render(sft, gid, img) < 0)
        return -1;
    /** render img to fb. Be aware of the bounds */
    x += gmtx.leftSideBearing;
    y += gmtx.yOffset;
    for(int i = 0; i < img.height; i++)
    {
        for(int j = 0; j < img.width; j++)
        {
            if((int)(x + j) < 0 || 
                (int)(y + i) < 0 || 
                (int)(x + j) >= FRAME_BUFFER_WIDTH || 
                (int)(y + i) >= FRAME_BUFFER_HEIGHT)
                continue;
            fb->pixels[(int)(y + i) * FRAME_BUFFER_WIDTH + (int)(x + j)].R = pixels[i * img.width + j];
            fb->pixels[(int)(y + i) * FRAME_BUFFER_WIDTH + (int)(x + j)].G = pixels[i * img.width + j];
            fb->pixels[(int)(y + i) * FRAME_BUFFER_WIDTH + (int)(x + j)].B = pixels[i * img.width + j];
        }
    }
    return gmtx.advanceWidth;
}
