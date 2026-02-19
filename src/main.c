#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "renderer/font.h"
#include "renderer/mesh.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include <GLES3/gl3.h>
#include <SDL.h>
#include <cglm/cglm.h>
#include <cglm/io.h>
#include <cglm/mat4.h>
#include <stdio.h>
#include <stdbool.h>

// Here until the renderer is built
#include "renderer/vertex.h"

bool init();
void cleanup();

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

SDL_Window* gwindow = NULL;
SDL_GLContext gglcontext;

bool init() {
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed %s\n", SDL_GetError());
        return false;
    }
    
    // Use OpenGL ES 3.1 (Pi 4 and 5 support)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    gwindow = SDL_CreateWindow("SDL wind", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (gwindow == NULL) {
        printf("Window couldn't be created! %s\n", SDL_GetError());
        return false;
    }
    
    gglcontext = SDL_GL_CreateContext(gwindow);

    if (!gglcontext) {
        printf("GL context creation failed! %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1); // vsync ON
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void cleanup() {
    SDL_GL_DeleteContext(gglcontext);
    SDL_DestroyWindow(gwindow);
    gwindow = NULL;
    gglcontext = NULL;

    SDL_Quit();
}


int main(void) {
    if (!init()) {
        printf("Init failed\n");
        return 1;
    }
    
    // ----- Resource Initialization -----
    shaders_init();
    textures_init();
    fonts_init();
    meshes_init();

    // Create 2D projection matrix
    mat4 projection;
    glm_ortho(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f, projection);

    SDL_Event e;
    bool quit = false;

    while (!quit) {
        while(SDL_PollEvent(&e)) {
            switch(e.type){
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_RIGHT:
                            printf("Right\n");
                            break;
                        case SDLK_LEFT:
                            printf("Left\n");
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }
        float time = SDL_GetTicks() * 0.001f;
    }
    
    shaders_free();
    textures_free();
    fonts_free();
    meshes_free();
    cleanup();

    return 0;
}
