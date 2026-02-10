#include "renderer/texture.h"
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* ---------- HELPERS ---------- */

void premult_alpha(Texture* tex, unsigned char* data) {
    if (tex->channels == 4) {
        for (int i = 0; i < tex->width * tex->height; i++) {
            unsigned char* p = data + (i * 4);
            float alpha = p[3] / 255.0f;
            p[0] = (unsigned char)(p[0] * alpha);
            p[1] = (unsigned char)(p[1] * alpha);
            p[2] = (unsigned char)(p[2] * alpha);
        }
    }
}

uint32_t round_to_pow2(uint32_t v) {
    if (v == 0) return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

uint32_t hash_string(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}


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
    
    if (premultiply_alpha) {
        premult_alpha(tex, data);
    }

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // GL_NEAREST for Pixel Art

    GLenum format = (tex->channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, tex->width, tex->height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    tex->layer_count = 1;
    snprintf(tex->path, 256, "%s", filepath);

    return tex;
}

Texture* texture_load_etc2_bin(const char* filepath) {
    Texture* tex = malloc(sizeof(Texture));
    if (!tex) return NULL;
    
    FILE* f = fopen(filepath, "rb");
    if(!f) {
        printf("Error: COUld not open file %s\n", filepath);
        return NULL;
    }
    
    // Read the header
    BakedHeader header;
    if (fread(&header, sizeof(BakedHeader), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    // Validate the magic number in header
    if (header.magic != 0x58455442) {
        printf("Error: Invalid file format\n");
        fclose(f);
        return NULL;
    }

    // Read the compressed data
    void* compressed_data = malloc(header.data_size);
    if (fread(compressed_data, 1, header.data_size, f) != header.data_size) {
        printf("Error: Could not read full texture data\n");
        free(compressed_data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // GL_NEAREST for Pixel Art

    glCompressedTexImage2D(
        GL_TEXTURE_2D,
        0,
        header.internal_format, // From header. Likely stores GL_COMPRESSED_RGBA8_ETC2_EAC
        header.width,
        header.height,
        0,
        (GLsizei)header.data_size,
        compressed_data
    );

    free(compressed_data);

    tex->width = header.width;
    tex->height = header.height;
    tex->channels = 4; // The compressed format only uses RGBA8

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

TextureAtlas* atlas_create(Texture* texture, uint32_t initial_capacity) {
    TextureAtlas* atlas = malloc(sizeof(TextureAtlas));
    atlas->parent_texture = texture;
    atlas->width = texture->width;
    atlas->height = texture->height;
    atlas->count = 0;
    
    // Double the requested size to reduce load factor to 50%
    uint32_t target = initial_capacity * 2;
    if (target < 16) target = 16; // Minimum capacity
    atlas->capacity = round_to_pow2(target); // Keep capacity a power of 2
    atlas->slots = calloc(atlas->capacity, sizeof(AtlasSlot));
    
    return atlas;
}

void atlas_resize(TextureAtlas* atlas, uint32_t new_capacity) {
    uint32_t old_capacity = atlas->capacity;
    AtlasSlot* old_slots = atlas->slots;

    // Ensure the new capacity is a power of 2
    atlas->capacity = round_to_pow2(new_capacity);
    
    // Create the bitmask (Capacity - 1)
    // If capacity is 16 (10000), mask is 15 (01111)
    uint32_t mask = atlas->capacity - 1;

    atlas->slots = calloc(atlas->capacity, sizeof(AtlasSlot));

    for (uint32_t i = 0; i < old_capacity; i++) {
        if(!old_slots[i].occupied) continue; // Slot is empty
        
        uint32_t new_index = old_slots[i].key_hash & mask;

        // Linear probe for free slot on hash collision
        while (atlas->slots[new_index].occupied) {
            new_index = (new_index + 1) & mask;
        }

        atlas->slots[new_index] = old_slots[i];
    }

    free(old_slots);
}

void atlas_define_region(TextureAtlas* atlas, int x, int y, int w, int h, const char* name) {
    // Ensure atlas has enough free slots
    //   This resizes the hash table if the table is at >75% capacity
    //   This is to reduce hash collisions and subsequent probing
    //   (capacity - (capacity >> 2)) is effectively capacity * 0.75
    if (atlas->count >= (atlas->capacity - (atlas->capacity >> 2))) {
        atlas_resize(atlas, atlas->capacity * 2);
    }
    
    // Bitmask (capacity enforced power of 2)
    uint32_t mask = atlas->capacity - 1;

    uint32_t hash = hash_string(name);
    uint32_t index = hash & mask;

    while (atlas->slots[index].occupied) {
        index = (index + 1) & mask;
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

    uint32_t mask = atlas->capacity - 1;
    
    uint32_t index = hash & mask;
    uint32_t start_index = index;

    while (atlas->slots[index].occupied) {
        if (atlas->slots[index].key_hash == hash) {
            return &atlas->slots[index].region;
        }
        index = (index + 1) & mask;
        if (index == start_index) break; // Traversed whole table
    }
    return NULL;
}

void atlas_free(TextureAtlas* atlas) {
    texture_free(atlas->parent_texture);
    free(atlas->slots);
    free(atlas);
}
