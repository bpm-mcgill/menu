#ifndef TEXTURE_H
#define TEXTURE_H

#include <cglm/types.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t id; // OpenGL binding texture id
    int width;
    int height;
    int channels;

    int layer_count; // For texture arrays if needed later
    char path[256]; // Debugging
} Texture;

Texture* texture_load(const char* filepath, bool premultiply_alpha);
void texture_bind(Texture* texture, uint32_t slot);
void texture_free(Texture* texture);


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
    Texture* parent_texture;
    AtlasSlot* slots;
    uint32_t capacity;
    uint32_t count;
    int width;
    int height;
} TextureAtlas;

TextureAtlas* atlas_create(Texture* texture);
void atlas_define_region(TextureAtlas* atlas, int x, int y, int w, int h, const char* name);
TextureRegion* atlas_get_region(TextureAtlas* atlas, const char* name);
void atlas_free(TextureAtlas* atlas);

#endif // !TEXTURE_H
