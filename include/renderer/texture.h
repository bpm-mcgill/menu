#ifndef TEXTURE_H
#define TEXTURE_H

#include "utils/handles.h"
#include <cglm/types.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_TEXTURE_PATH 256

typedef struct {
    uint32_t id; // OpenGL binding texture id
    int width;
    int height;
    uint32_t format;

    //int layer_count; // For texture arrays if needed later
    char path[MAX_TEXTURE_PATH]; // Debugging

    bool active;
    uint32_t generation;
} Texture;

DECLARE_HANDLE(TextureHandle);

// --- RESOURCE POOL ---
// Declared for the resource getter (static inline)
DEFINE_FLEX_ARRAY(Texture, TextureArray)
DECLARE_RESOURCE_POOL(Texture, TextureArray)
DEFINE_RESOURCE_GETTER(Texture, TextureHandle, _Texture_pool, "TEXTURE")

// --- PUBLIC API ---

void textures_init(void);
void textures_free(void);

TextureHandle texture_new(int w, int h, uint32_t format);
TextureHandle texture_load(const char* filepath, bool premultiply_alpha);
TextureHandle texture_load_etc2_bin(const char* filepath);

void texture_bind(TextureHandle handle, uint32_t slot);
void texture_delete(TextureHandle handle);


/* ----- TEXTURE ATLAS ----- */


#define MAX_REGION_NAME 64

typedef struct {
    vec2 uv_min;
    vec2 uv_max;
    int width;
    int height;
} TextureRegion;

typedef struct {
    char name[MAX_REGION_NAME];
    uint32_t key_hash;
    TextureRegion region;
    bool occupied;
} AtlasSlot;

typedef struct {
    TextureHandle parent_texture;
    AtlasSlot* slots;
    uint32_t capacity;
    uint32_t count;
    int width;
    int height;
} TextureAtlas;

TextureAtlas* atlas_create(TextureHandle texture, uint32_t initial_capacity);
void atlas_define_region(TextureAtlas* atlas, int x, int y, int w, int h, const char* name);
TextureRegion* atlas_get_region(TextureAtlas* atlas, const char* name);
void atlas_free(TextureAtlas* atlas);

#endif // !TEXTURE_H
