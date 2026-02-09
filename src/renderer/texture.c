#include "renderer/texture.h"
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* ---------- TEXTURE ---------- */

Texture* texture_load(const char* filepath, bool premultiply_alpha) {
    Texture* tex = malloc(sizeof(Texture));
    if (!tex) return NULL;

    stbi_set_flip_vertically_on_load(false);

    unsigned char* data = stbi_load(filepath, &tex->width, &tex->height, &tex->channels, 0);
    if (!data) {
        fprintf(stderr, "Failed to load texture: %s\n", filepath);
        free(tex);
        return NULL;
    }

    if (premultiply_alpha && tex->channels == 4) {
        for (int i = 0; i < tex->width * tex->height; i++) {
            unsigned char* p = data + (i * 4);
            float alpha = p[3] / 255.0f;
            p[0] = (unsigned char)(p[0] * alpha);
            p[1] = (unsigned char)(p[1] * alpha);
            p[2] = (unsigned char)(p[2] * alpha);
        }
    }

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Use GL_NEAREST for Pixel Art

    GLenum format = (tex->channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, tex->width, tex->height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    tex->layer_count = 1;
    snprintf(tex->path, 256, "%s", filepath);

    return tex;
}

void texture_bind(Texture* texture, uint32_t slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture->id);
}

void texture_free(Texture* texture) {
    glDeleteTextures(1, &texture->id);
    free(texture);
}

/* ---------- TEXTURE ATLAS ---------- */

uint32_t hash_string(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

TextureAtlas* atlas_create(Texture* texture) {
    TextureAtlas* atlas = malloc(sizeof(TextureAtlas));
    atlas->parent_texture = texture;
    atlas->width = texture->width;
    atlas->height = texture->height;
    atlas->count = 0;
    
    atlas->capacity = 16;
    atlas->slots = calloc(atlas->capacity, sizeof(AtlasSlot));
    
    return atlas;
}

void atlas_define_region(TextureAtlas* atlas, int x, int y, int w, int h, const char* name) {
    // 1. Ensure atlas has enough free slots
    if (atlas->count >= atlas->capacity) {
        atlas->capacity *= 2;
        atlas->slots = realloc(atlas->slots, sizeof(AtlasSlot) * atlas->capacity);
    }

    uint32_t hash = hash_string(name);
    uint32_t index = hash % atlas->capacity;

    while (atlas->slots[index].occupied) {
        index = (index + 1) % atlas->capacity;
    }

    AtlasSlot* slot = &atlas->slots[index];

    slot->key_hash = hash;
    slot->occupied = true;

    strncpy(slot->name, name, MAX_REGION_NAME - 1);
    slot->name[MAX_REGION_NAME - 1] = '\0';

    float inv_w = 1.0f / (float)atlas->parent_texture->width;
    float inv_h = 1.0f / (float)atlas->parent_texture->height;

    slot->region.uv_min[0] = (float)x * inv_w;
    slot->region.uv_min[1] = (float)y * inv_h;
    slot->region.uv_max[0] = (float)(x + w) * inv_w;
    slot->region.uv_max[1] = (float)(y + h) * inv_h;

    slot->region.width = w;
    slot->region.height = h;

    atlas->count++;
}

TextureRegion* atlas_get_region(TextureAtlas* atlas, const char* name) {
    uint32_t hash = hash_string(name);
    uint32_t index = hash % atlas->capacity;
    uint32_t start_index = index;

    while (atlas->slots[index].occupied) {
        if (atlas->slots[index].key_hash == hash) {
            return &atlas->slots[index].region;
        }
        index = (index + 1) % atlas->capacity;
        if (index == start_index) break; // Traversed whole table
    }
    return NULL;
}

void atlas_free(TextureAtlas* atlas) {
    texture_free(atlas->parent_texture);
    free(atlas->slots);
    free(atlas);
}
