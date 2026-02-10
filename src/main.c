#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_platform.h"
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
#include <stdlib.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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
    
    Shader shader;
    shader_create(&shader, 
                  "/home/user/Documents/sdlmenu/shaders/basicvert.glsl",
                  "/home/user/Documents/sdlmenu/shaders/waves.glsl"
    );
    
    Shader fbshader;
    shader_create(&fbshader, 
                  "/home/user/Documents/sdlmenu/shaders/fbovert.glsl",
                  "/home/user/Documents/sdlmenu/shaders/fbofrag.glsl"
    );

    Shader iconshader;
    shader_create(&iconshader, 
                  "/home/user/Documents/sdlmenu/shaders/iconvert.glsl",
                  "/home/user/Documents/sdlmenu/shaders/iconfrag.glsl"
    );
    
    Shader textshader;
    shader_create(&textshader, 
                  "/home/user/Documents/sdlmenu/shaders/msdf-v.glsl",
                  "/home/user/Documents/sdlmenu/shaders/msdf-f.glsl"
    );

    
    
    // ----------- FONT BATCH -----------
    //Font* rodin_bold = font_load("assets/Rodin-Bold.ttf", 64.0f);
    Font* rodin_bold = font_load_bin("assets/Rodin-Bold.bin");
    TextParams params = {
        .color = {1.0f,1.0f,1.0f,1.0f}, 
        .scale = 64.0f,
        .softness = 4.0f
    };
    MeshData txt = font_generate_mesh_data(rodin_bold, "Bitch ass", params);
    
    VertexLayout flayout = {0};
    vertex_layout_add(&flayout, 0, 2, GL_FLOAT, false);
    vertex_layout_add(&flayout, 1, 2, GL_FLOAT, false);
    vertex_layout_add(&flayout, 2, 4, GL_FLOAT, false);
    MeshObj* txtobj = mesh_obj_create(0, 0, flayout, true);
    MeshHandle txthand = mesh_obj_push(txtobj, txt);
    
    mat4 txtmod; glm_mat4_identity(txtmod);
    vec3 positionz = { 100.0f, 200.0f, 0.0f };
    glm_translate(txtmod, positionz);
    uniform_store_add(&txtobj->uniforms, "u_model", UNI_MAT4, (void*)txtmod);
    int texdid = 1;
    uniform_store_add(&txtobj->uniforms, "u_tex", UNI_INT, &texdid);
    uniform_store_add(&txtobj->uniforms, "u_pxRange", UNI_FLOAT, &params.softness);
    
    Mesh txtmesh = mesh_build(txtobj, &textshader); 

    // ------------ IDK SHIT --------------------
    
    VertexLayout layout = {0};
    vertex_layout_add(&layout, 0, 3, GL_FLOAT, false);
    vertex_layout_add(&layout, 1, 2, GL_FLOAT, false);
    vertex_layout_add(&layout, 2, 4, GL_UNSIGNED_BYTE, true);

    MeshObj* obj = mesh_obj_create(0, 0, layout, true);
    
    mat4 mod; glm_mat4_identity(mod);
    uniform_store_add(&obj->uniforms, "u_model", UNI_MAT4, (void*)mod);
    int texid = 1;
    uniform_store_add(&obj->uniforms, "u_tex", UNI_INT, &texid);

    MeshData mdat = gen_quad();
    //mesh_obj_push(obj, mdat);
    MeshHandle hand = mesh_obj_push(obj, mdat);
    printf("Handle: %d\n", (int)hand);

    Texture* tex = texture_load_etc2_bin("assets/icons.bin");
    TextureAtlas* atlas = atlas_create(tex, 16);
    atlas_define_region(atlas, 116, 99, 215, 248, "power");
    TextureRegion* region = atlas_get_region(atlas, "power");
    if (!region) {
        printf("Couldn't find region!!\n");
    }

    mat4 mod2; glm_mat4_identity(mod2);
    vec3 position2 = { 200.0f, 200.0f, 0.0f };
    glm_translate(mod2, position2);
    vec3 size2 = { 200.0f, 200.0f, 1.0f };
    glm_scale(mod2, size2);
    MeshData dat2 = mesh_obj_get_data(obj, hand);
    md_apply_mat4(&dat2, obj->layout, mod2); 
    md_apply_region(&dat2, region);

    Mesh quad = mesh_build(obj, &iconshader);

    //mesh_obj_destroy(obj); 

    MeshObj* fboobj = mesh_obj_create_quad();
    Mesh fboquad = mesh_build(fboobj, &fbshader);
    // Framebuffer Object
    unsigned int fbo, fbotex;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &fbotex);
    glBindTexture(GL_TEXTURE_2D, fbotex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 320, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Attach texture to currently bound fbo
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbotex, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        printf("FBO failed to complete\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
        
        // Bind framebuffer to draw to
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, 320, 240);
        glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw framebuffer quad
        shader_use(&shader);
        mesh_bind(&fboquad);
        set_uniform_1f(&shader, "u_time", time);
        set_uniform_vec2f(&shader, "u_resolution", (vec2){320, 240});
        glDrawElements(GL_TRIANGLES, fboquad.index_count, GL_UNSIGNED_INT, 0);

        // Switch to default Framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Draw framebuffer quad
        glUseProgram(0);
        shader_use(&fbshader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbotex);
        set_uniform_1i(&fbshader, "u_texture", 0);
        glDrawElements(GL_TRIANGLES, fboquad.index_count, GL_UNSIGNED_INT, 0);
        
        // Draw icon quad
        shader_use(&iconshader);
        //glm_mat4_identity(mod2);
        //glm_rotate(mod2, 0.01f, (vec3){0,0,1});
        //mesh_obj_transform(obj, hand, mod2);
        //mesh_update_gpu(&quad, obj); 
        
        mesh_bind(&quad);
        texture_bind(tex, 1);
        uniform_store_apply(&quad.uniforms, &iconshader);
        set_uniform_mat4(&iconshader, "u_projection", projection);
        glDrawElements(GL_TRIANGLES, quad.index_count, GL_UNSIGNED_INT, 0);

        // -------- DRAW TEXT BATCH --------
        shader_use(&textshader);
        mesh_bind(&txtmesh);
        texture_bind(rodin_bold->texture, 1);
        uniform_store_apply(&txtmesh.uniforms, &textshader);
        set_uniform_mat4(&textshader, "u_projection", projection);
        glDrawElements(GL_TRIANGLES, txtmesh.index_count, GL_UNSIGNED_INT, 0);

        SDL_GL_SwapWindow(gwindow);
    }
    
    shader_destroy(&shader);
    shader_destroy(&fbshader);
    shader_destroy(&iconshader);
    cleanup();

    return 0;
}
