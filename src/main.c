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
    
    Framebuffer fbo_full = fbo_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    Framebuffer fbo_small_a = fbo_create(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    Framebuffer fbo_small_b = fbo_create(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
     
    // Pass 1: Bloom post processing
    ShaderHandle downsample = shader_create("shaders/fbovert.glsl", "shaders/downsamplefrag.glsl");
    MaterialHandle downsample_mat = material_create(downsample, TextureHandle_null());
    float threshold = 0.5f;
    vec2 res = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
    material_set_uniform(downsample_mat, "u_threshold", UNI_FLOAT, &threshold);
    material_set_uniform(downsample_mat, "u_resolution", UNI_VEC2, &res);

    ShaderHandle blur = shader_create("shaders/fbovert.glsl", "shaders/bloomfrag.glsl");
    vec2 small_res = { (float)SCREEN_WIDTH / 2, (float)SCREEN_HEIGHT / 2 };
    float intensity = 0.6f;
    
    MaterialHandle blur_h_mat = material_create(blur, TextureHandle_null());
    vec2 dir_h = {1.0f, 0.0f};
    material_set_uniform(blur_h_mat, "u_intensity", UNI_FLOAT, &intensity);
    material_set_uniform(blur_h_mat, "u_direction", UNI_VEC2, &dir_h);
    material_set_uniform(blur_h_mat, "u_resolution", UNI_VEC2, &small_res);
    
    MaterialHandle blur_v_mat = material_create(blur, TextureHandle_null());
    vec2 dir_v = {0.0f, 1.0f};
    material_set_uniform(blur_v_mat, "u_intensity", UNI_FLOAT, &intensity);
    material_set_uniform(blur_v_mat, "u_direction", UNI_VEC2, &dir_v);
    material_set_uniform(blur_v_mat, "u_resolution", UNI_VEC2, &small_res);
 
    // Pass 0: Glowing geometry
    RenderPass pass_geom = {
        .id = 0,
        .type = PASS_GEOMETRY,
        .target_fbo = &fbo_full,
        .clear_target = true,
        .clear_color = {0.0f, 0.0f, 0.0f, 0.0f}
    };
    renderer_add_pass(pass_geom);
    
    // Downsamlpe and extract brightness
    RenderPass pass_downsample = {
        .id = 1,
        .type = PASS_POSTPROCESS,
        .target_fbo = &fbo_small_a,
        .clear_target = true,
        .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},

        .post_material = downsample_mat,
        .input_count = 1,
        .input_textures = { fbo_full.texture }
    };
    renderer_add_pass(pass_downsample);
    
    RenderPass pass_blur_h = {
        .id = 2,
        .type = PASS_POSTPROCESS,
        .target_fbo = &fbo_small_b,
        .clear_target = true,
        .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},

        .post_material = blur_h_mat,
        .input_count = 1,
        .input_textures = { fbo_small_a.texture }
    };
    renderer_add_pass(pass_blur_h);
    
    RenderPass pass_blur_v = {
        .id = 3,
        .type = PASS_POSTPROCESS,
        .target_fbo = NULL,
        .clear_target = true,
        .clear_color = {0.2f, 0.2f, 0.2f, 1.0f},

        .post_material = blur_v_mat,
        .input_count = 1,
        .input_textures = { fbo_small_b.texture }
    };
    renderer_add_pass(pass_blur_v);

    // Pass 2: Draw meshes to the FBO
    RenderPass pass_world = {
        .id = 4,
        .type = PASS_GEOMETRY,
        .target_fbo = NULL,
        .clear_target = false,
    };
    renderer_add_pass(pass_world); 
    

    // --- Materials ---
    ShaderHandle shader = shader_create("shaders/iconvert.glsl", "shaders/iconfrag.glsl");
    TextureHandle tex = texture_load_etc2_bin("assets/icons.bin");
    TextureAtlas* atlas = atlas_create(tex, 16);
    atlas_define_region(atlas, 116, 99, 215, 248, "power");
    MaterialHandle quad_mat = material_create(shader, tex);
    
    // Create 2D projection matrix
    mat4 projection;
    glm_ortho(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f, projection);
    material_set_uniform(quad_mat, "u_projection", UNI_MAT4, projection);

    // --- Render Objects --
    VertexLayout layout = uiv_layout();
    MeshData quad_data = uiv_gen_quad();
    TextureRegion* region = atlas_get_region(atlas, "power");

    MeshObj* obj = mesh_obj_create(layout, false);
    //mesh_obj_push(obj, quad_data);
    uiv_apply_region(&quad_data, region);
    mesh_obj_push(obj, quad_data);
    MeshHandle quad_mesh = mesh_create_from_obj(obj);
    mesh_obj_destroy(obj); // Not needed after GPU upload

    // TEXT
    ShaderHandle txtshader = shader_create("shaders/text-v.glsl", "shaders/msdf-f.glsl");
    FontHandle rodin_bold = font_load_bin("assets/Rodin-Bold.bin");
    Font* rodin = Font_get(rodin_bold);
    MaterialHandle txt_mat = material_create(txtshader, rodin->texture);
    material_set_uniform(txt_mat, "u_projection", UNI_MAT4, projection);
    mat4 txtmod; glm_mat4_identity(txtmod);
    vec3 positionz = { 100.0f, 200.0f, 0.0f };
    glm_translate(txtmod, positionz);
    material_set_uniform(txt_mat, "u_model", UNI_MAT4, (void*)txtmod);
    int texid = 0;
    float px_range = 4.0f;
    material_set_uniform(txt_mat, "u_pxrange", UNI_FLOAT, &px_range);
    material_set_uniform(txt_mat, "u_tex", UNI_FLOAT, &texid);
    TextParams params = {
        .color = {255,255,255,255}, 
        .size = 32.0f,
        .softness = 0.03f
    };
    MeshData text = font_generate_mesh_data(rodin_bold, "Theme Settings", params);
    MeshObj* txtobj = mesh_obj_create(layout, false);
    mesh_obj_push(txtobj, text);
    MeshHandle text_mesh = mesh_create_from_obj(txtobj);
    mesh_obj_destroy(txtobj);

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
        glm_translate(transform, (vec3){220.0f, 140.0f, 0.0f});
        glm_scale(transform, (vec3){30.0f, 30.0f, 1.0f});

        renderer_begin();
        renderer_submit(quad_mesh, quad_mat, transform, 0, 0.5f); 
        renderer_submit(quad_mesh, quad_mat, transform, 4, 0.5f);
        glm_mat4_identity(transform);
        glm_translate(transform, (vec3){32.0f, 96.0f, 0.0f});
        glm_scale(transform, (vec3){1.0f, 1.0f, 1.0f});
        renderer_submit(text_mesh, txt_mat, transform, 0, 0.6f);
        renderer_submit(text_mesh, txt_mat, transform, 4, 0.6f);
        renderer_end();

        SDL_GL_SwapWindow(gwindow);
    }
    
    renderer_free();
    cleanup();

    return 0;
}
