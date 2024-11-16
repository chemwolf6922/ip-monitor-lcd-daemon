#include <stdio.h>
#include <arpa/inet.h>
#include "../ip_renderer.h"

int main(int argc, char const *argv[])
{
    lcd_frame_buffer_t fb = {0};
    ip_renderer_t* renderer = ip_renderer_new("./OpenSans-Medium.ttf");
    if(!renderer)
        return 1;
    struct in_addr ip;
    if(inet_pton(AF_INET, "192.168.0.123", &ip)<0)
        return 1;
    if(renderer->render_ip(renderer, &fb, &ip)<0)
        return 1;
    renderer->destroy(renderer);

    /** dump the fb to ppm */
    printf("P3\n");
    printf("%d %d\n", FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
    printf("255\n");
    for(int i = 0; i < FRAME_BUFFER_WIDTH * FRAME_BUFFER_HEIGHT; i++)
    {
        printf("%d %d %d\n", fb.pixels[i].R, fb.pixels[i].G, fb.pixels[i].B);
    }

    return 0;
}


