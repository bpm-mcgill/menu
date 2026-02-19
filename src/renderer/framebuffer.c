#include "renderer/framebuffer.h"
#include "engine.h"
#include "renderer/texture.h"
#include <GLES3/gl3.h>
#include <stdio.h>

Framebuffer fbo_create(int w, int h) {
    Framebuffer fbo = { .width = w, .height = h };

    // Create a texture for the framebuffer
    fbo.texture = texture_new(w, h, GL_RGBA);
    Texture* tex = Texture_get(fbo.texture);
    
    // Set up framebuffer
    glGenFramebuffers(1, &fbo.id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.id);

    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA8, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Attach texture to currently bound fbo
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->id, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        LOG_ERROR("FBO failed to complete");
        texture_delete(fbo.texture);
        glDeleteFramebuffers(1, &fbo.id);
        return (Framebuffer){0};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

void fbo_destroy(Framebuffer* fbo) {
    if (!fbo || fbo->id == 0) return;

    texture_delete(fbo->texture);

    glDeleteFramebuffers(1, &fbo->id);
    fbo->id = 0;
}

void fbo_bind(Framebuffer* fbo) {
    if (fbo && fbo->id != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo->id);
        glViewport(0, 0, fbo->width, fbo->height);
    }
    else {
        LOG_WARN("Attempted to bind an invalid/null FBO.");
    }
}

void fbo_unbind(int screen_width, int screen_height) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screen_width, screen_height);
}

void fbo_resize(Framebuffer* fbo, int w, int h) {
    if (!fbo || fbo->id == 0) return;
    if (fbo->width == w && fbo->height == h) return;

    fbo->width = w;
    fbo->height = h;
    
    Texture* tex = Texture_get(fbo->texture);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}
