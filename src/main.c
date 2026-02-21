#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "renderer/font.h"
#include "renderer/framebuffer.h"
#include "renderer/mesh.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include <GLES3/gl3.h>
#include <SDL.h>
#include <cglm/affine-pre.h>
#include <cglm/affine.h>
#include <cglm/cglm.h>
#include <cglm/io.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <stdio.h>
#include <stdbool.h>
#include "renderer/renderer.h"

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
    
    // ----- Renderer Initialization -----
    renderer_init();
    
    // Pass 0: Draw to the screen
    RenderPass pass_world = {
        .id = 0,
        .type = PASS_GEOMETRY,
        .target_fbo = NULL,
        .clear_color = {0.1f, 0.1f, 0.1f, 1.0f},
        .clear_target = true,
    };
    renderer_add_pass(pass_world); 
    
    // ----- Scene Vars -----
    // Create 2D projection matrix
    mat4 projection;
    glm_ortho(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f, projection);
    
    // --- Materials ---
    ShaderHandle shader = shader_create("shaders/text-v.glsl", "shaders/msdf-f.glsl");
    FontHandle rodin_bold = font_load_bin("assets/Rodin-Bold.bin");   // MSDF Font asset
    TextureAtlas* atlas = texture_load_msdf("assets/icons_msdf.bin"); // MSDF atlas asset
    
    MaterialHandle msdf_mat = material_create(shader, atlas->parent_texture);
    material_set_uniform(msdf_mat, "u_projection", UNI_MAT4, projection);
    float px_range = 4.0f;
    material_set_uniform(msdf_mat, "u_pxrange", UNI_FLOAT, &px_range); 
    float softness = 0.4f;
    material_set_uniform(msdf_mat, "u_softness", UNI_FLOAT, &softness);

    // --- Render Objects --
    mat4 mod; glm_mat4_identity(mod);
    glm_translate(mod, (vec3){70.0f, ((float)SCREEN_HEIGHT/2.0f)-(35.0f/2.0f), 1.0f});
    glm_scale(mod, (vec3){35.0f, 35.0f, 1.0f});
    
    VertexLayout layout = uiv_layout();
    MeshObj* obj = mesh_obj_create(layout, false);
    
    MeshData quad_data = uiv_gen_quad();
    TextureRegion* region = atlas_get_region(atlas, "power");
    uiv_apply_region(&quad_data, region);
    uiv_apply_mat4(&quad_data, layout, mod);
    mesh_obj_push(obj, quad_data);
    
    quad_data = uiv_gen_quad();
    region = atlas_get_region(atlas, "settings");
    uiv_apply_region(&quad_data, region);
    glm_translate(mod, (vec3){2.5f, 0.0f, 0.0f});
    uiv_apply_mat4(&quad_data, layout, mod);
    mesh_obj_push(obj, quad_data);
    
    quad_data = uiv_gen_quad();
    region = atlas_get_region(atlas, "pics");
    uiv_apply_region(&quad_data, region);
    glm_translate(mod, (vec3){2.5f, 0.0f, 0.0f});
    uiv_apply_mat4(&quad_data, layout, mod);
    mesh_obj_push(obj, quad_data);
    MeshHandle quad_mesh = mesh_create_from_obj(obj);
    mesh_obj_destroy(obj); // Not needed after GPU upload

    // TEXT
    Font* rodin = Font_get(rodin_bold);
    MaterialHandle txt_mat = material_create(shader, rodin->texture);
    material_set_uniform(txt_mat, "u_projection", UNI_MAT4, projection);
    mat4 txtmod; glm_mat4_identity(txtmod);
    glm_translate(txtmod, (vec3){ 44.0f, 275.0f, 1.0f });
    TextParams params = {
        .color = {255,255,255,255}, 
        .size = 16.0f,
        .softness = 1.4f
    };
    material_set_uniform(txt_mat, "u_softness", UNI_FLOAT, &params.softness);
    material_set_uniform(txt_mat, "u_pxrange", UNI_FLOAT, &px_range); 
    MeshData text = font_generate_mesh_data(rodin_bold, "Power", params);
    uiv_apply_mat4(&text, layout, txtmod);
    MeshObj* txtobj = mesh_obj_create(layout, false);
    mesh_obj_push(obj, text);
    MeshHandle text_mesh = mesh_create_from_obj(txtobj);
    mesh_obj_destroy(obj);

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
        
        mat4 transform;
        glm_mat4_identity(transform);

        renderer_begin();
        renderer_submit(quad_mesh, msdf_mat, transform, 0, 0.5f);
        renderer_submit(text_mesh, txt_mat, transform, 0, 0.6f);
        renderer_end();

        SDL_GL_SwapWindow(gwindow);
    }
    
    renderer_free();
    cleanup();

    return 0;
}
