#include "renderer/renderer.h"
#include "engine.h"
#include "renderer/font.h"
#include "renderer/framebuffer.h"
#include "renderer/mesh.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex.h"
#include "utils/flex_array.h"
#include <GLES3/gl3.h>
#include <cglm/common.h>
#include <cglm/mat4.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------- Internal renderer state ---------- */

#define MAX_RENDER_PASSES 12

typedef struct {
    RenderPass passes[MAX_RENDER_PASSES];
    uint8_t pass_count;
    
    // A fullscreen quad used for post-processing
    MeshHandle fbo_quad;

    RenderQueue* commands;
} Renderer;

static Renderer _renderer = {0};

/* ---------- Internal Helpers ---------- */

// QSort comparison function
static int compare_commands(const void* a, const void* b) {
    SortKey keyA = ((const RenderCommand*)a)->key;
    SortKey keyB = ((const RenderCommand*)b)->key;

    if (keyA < keyB) return -1;
    if (keyA > keyB) return 1;
    return 0;
}

static SortKey calculate_key(uint8_t pass_id, float depth, bool transparent, uint32_t shader_id, uint32_t tex_id) {
    SortKey key = 0;
    
    // 1. Pass ID (8 bits) - Highest priority
    key |= ((uint64_t)pass_id & 0xFF) << 56;

    // 2. Translucency (1 bit)
    if (transparent) key |= (1ULL << 55);

    // 3. Depth (24 bits)
    uint32_t depth_int = (uint32_t)(depth * 10000.0f); // Scale depth
    if (transparent) {
        // Transparent objects sort back to front (larger depth first)
        depth_int = ~depth_int;
    }
    // Clamp to 24 bits
    depth_int &= 0xFFFFFF;
    key |= ((uint64_t)depth_int) << 30;

    // 4. Shader ID (14 bits)
    key |= ((uint64_t)shader_id & 0x3FFF) << 16;

    // 5. Texture ID (16 bits)
    key |= ((uint64_t)tex_id & 0xFFFF);

    return key;
}

/* ---------- Public API ---------- */

void renderer_init(void) {
    LOG_INFO("STARTING RENDERER ... ");
    _renderer.commands = RenderQueue_create(INITIAL_COMMAND_CAPACITY);
    ENGINE_ASSERT(_renderer.commands != NULL, "Failed to allocate RenderQueue.");
    _renderer.pass_count = 0;
    
    // Init systems
    shaders_init();
    textures_init();
    fonts_init();
    meshes_init();

    // Generate Fullscreen Quad for Post-Processing
    VertexLayout fbvlay = fbv_layout();
    MeshData fbodat = fbv_gen_quad();
    MeshObj* fboobj = mesh_obj_create(fbvlay, false);
    mesh_obj_push(fboobj, fbodat);
    _renderer.fbo_quad = mesh_create_from_obj(fboobj);
    mesh_obj_destroy(fboobj); // Can be deleted once it is on the GPU
    
    LOG_INFO("Renderer System: Initialized");
}

void renderer_free(void) {
    LOG_INFO("Renderer system shutting down ...");
    if (_renderer.commands) RenderQueue_free(_renderer.commands);
    _renderer.commands = NULL;
    mesh_delete(_renderer.fbo_quad);
    
    shaders_free();
    textures_free();
    fonts_free();
    meshes_free();
}

void renderer_add_pass(RenderPass pass) {
    ENGINE_ASSERT(_renderer.pass_count < MAX_RENDER_PASSES, "Exceeded maximum Render Passes!");
    _renderer.passes[_renderer.pass_count++] = pass;
}

void renderer_begin(void) {
    _renderer.commands->count = 0;
}

void renderer_submit(MeshHandle mesh, MaterialHandle material, mat4 transform, uint8_t pass_id, float depth) {
    Material* mat = Material_get(material);
    uint32_t shader_idx = ShaderHandle_index(mat->shader);
    uint32_t tex_idx = TextureHandle_index(mat->texture);

    bool is_transparent = false; // TODO: determine if transparent from material
    
    RenderCommand cmd = {0};
    cmd.mesh = mesh;
    cmd.material = material;
    glm_mat4_copy(transform, cmd.transform);
    cmd.key = calculate_key(pass_id, depth, is_transparent, shader_idx, tex_idx);
    _renderer.commands = RenderQueue_push(_renderer.commands, cmd);
    ENGINE_ASSERT(_renderer.commands != NULL, "Failed to realloc RenderQueue.");
}

void renderer_end(void) {
    if (_renderer.commands->count == 0 &&_renderer.pass_count == 0) return;
    
    // Sort the command queue by the sort keys
    qsort(_renderer.commands->data, _renderer.commands->count, sizeof(RenderCommand), compare_commands);
    
    uint32_t cmd_idx = 0;
    for (int p = 0; p < _renderer.pass_count; p++) {
        RenderPass* pass = &_renderer.passes[p];

        if (pass->target_fbo) {
            fbo_bind(pass->target_fbo);
        }
        else {
            // TODO: make renderer store a screen width and height
            fbo_unbind(640, 480);
        }

        if (pass->clear_target) {
            glClearColor(pass->clear_color[0], pass->clear_color[1], pass->clear_color[2], pass->clear_color[3]);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        if (pass->type == PASS_GEOMETRY) {
            MaterialHandle current_material = MaterialHandle_null();

            while (cmd_idx < _renderer.commands->count) {
                RenderCommand* cmd = &_renderer.commands->data[cmd_idx];
                uint8_t cmd_pass = (cmd->key >> 56) & 0xFF;

                if (cmd_pass != pass->id) break;
                
                // Minimize state changes by only applying / binding if material changed
                if (MaterialHandle_index(cmd->material) != MaterialHandle_index(current_material)) {
                    material_apply(cmd->material);
                    current_material = cmd->material;
                }

                mesh_bind(cmd->mesh);
                Mesh* m = Mesh_get(cmd->mesh);

                Material* mat = Material_get(cmd->material);
                Shader* s = Shader_get(mat->shader);
                set_uniform_mat4(s, "u_model", cmd->transform);

                glDrawElements(GL_TRIANGLES, m->index_count, GL_UNSIGNED_INT, 0);

                cmd_idx++;
            }
        }
        else if (pass->type == PASS_POSTPROCESS) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            material_apply(pass->post_material);
            Material* mat = Material_get(pass->post_material);
            Shader* s = Shader_get(mat->shader);
            
            // Bind all of the input textures from previous passses
            for (int i = 0; i < pass->input_count; i++) {
                glActiveTexture(GL_TEXTURE0 + i);
                Texture* tex = Texture_get(pass->input_textures[i]);
                glBindTexture(GL_TEXTURE_2D, tex->id);

                char uniform_name[16];
                snprintf(uniform_name, sizeof(uniform_name), "u_tex%d", i);
                set_uniform_1i(s, uniform_name, i);
            }

            mesh_bind(_renderer.fbo_quad);
            Mesh* quad = Mesh_get(_renderer.fbo_quad);
            glDrawElements(GL_TRIANGLES, quad->index_count, GL_UNSIGNED_INT, 0);
        }
    }
}
