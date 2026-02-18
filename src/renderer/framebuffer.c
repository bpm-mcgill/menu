#include "renderer/framebuffer.h"
#include <GLES3/gl3.h>
#include <stdio.h>

// TODO: Framebuffer resizing

Framebuffer fbo_create(int w, int h){
    // Framebuffer Object
    unsigned int fbo, fbotex;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &fbotex);
    glBindTexture(GL_TEXTURE_2D, fbotex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA8, GL_UNSIGNED_BYTE, NULL);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Attach texture to currently bound fbo
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbotex, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        fprintf(stderr, "Error: FBO failed to complete\n");
        glDeleteTextures(1, &fbotex);
        glDeleteFramebuffers(1, &fbo);
        return (Framebuffer){0};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return (Framebuffer) {fbo, fbotex, w, h};
}

void fbo_destroy(Framebuffer* fbo);

void fbo_bind(Framebuffer* fbo);
