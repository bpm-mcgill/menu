#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "renderer/texture.h"
#include <stdint.h>

// INFO: This doesn't use handles, and doesn't have resource management, as it will be
// Managed by the renderer via RenderPasses. Renderer will handle cleanup

typedef struct {
    uint32_t id;            // FBO ID
    TextureHandle texture;  // The FBO's color attachment
    int width, height;
} Framebuffer;

Framebuffer fbo_create(int w, int h);
void fbo_destroy(Framebuffer* fbo);

void fbo_bind(Framebuffer* fbo);
void fbo_unbind(int screen_width, int screen_height);

void fbo_resize(Framebuffer* fbo, int w, int h);

#endif // !FRAMEBUFFER_H
