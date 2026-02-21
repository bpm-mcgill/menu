#ifndef RENDERER_H
#define RENDERER_H

#include "renderer/framebuffer.h"
#include "renderer/mesh.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include <cglm/types.h>
#include <stdbool.h>
#include <stdint.h>

/* -------- RENDER COMMANDS -------- */

#define INITIAL_COMMAND_CAPACITY 16

// Bit Layout: [Pass ID: 8][Translucent: 1][Depth: 24][Shader ID: 15][Texture ID: 16]
typedef uint64_t SortKey;

typedef struct {
    SortKey key;
    MeshHandle mesh;
    MaterialHandle material;
    mat4 transform;
} RenderCommand;

DEFINE_FLEX_ARRAY(RenderCommand, RenderQueue);

/* -------- RENDER PASSES -------- */

typedef enum {
    PASS_GEOMETRY,
    PASS_POSTPROCESS
} PassType;

#define MAX_PASS_INPUTS 4

typedef struct {
    uint8_t id;              // The pass id that is used in the SortKey
    PassType type;

    // Output
    Framebuffer* target_fbo; // NULL = default framebuffer (screen)
    bool clear_target;
    vec4 clear_color;
    
    // -- Post processing shit --
    MaterialHandle post_material;
    TextureHandle input_textures[MAX_PASS_INPUTS];
    uint8_t input_count;
} RenderPass;

/* -------- PUBLIC API -------- */

void renderer_init(void);
void renderer_free(void);

void renderer_add_pass(RenderPass pass);

void renderer_begin(void);
void renderer_submit(MeshHandle mesh, MaterialHandle material, mat4 transform, uint8_t pass_id, float depth);

void renderer_end(void);

#endif // !RENDERER_H
