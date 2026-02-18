#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
    uint32_t id;          // FBO ID
    uint32_t texture_id;  // The FBO's color attachment
    int width, height;
} Framebuffer;

Framebuffer fbo_create(int w, int h);
void fbo_destroy(Framebuffer* fbo);
void fbo_bind(Framebuffer* fbo);

#endif // !FRAMEBUFFER_H
