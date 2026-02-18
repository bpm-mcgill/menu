#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "renderer/texture.h"
#include "utils/handles.h"
#include "engine.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// TODO: Create a global texture atlas hashmap

/*
 * Internal texture management
*/
DEFINE_RESOURCE_POOL(Texture, TextureHandle, TextureArray);

/* ---------- HELPERS ---------- */

void premult_alpha(Texture* tex, unsigned char* data) {
    if (tex->format == GL_RGBA) {
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

/*
 * Makes a new empty entry in the texture buffer and returns the handle
*/
TextureHandle texture_new(int w, int h, uint32_t format) {
    TextureHandle handle = Texture_alloc();
    Texture* tex = Texture_get(handle);
    
    // No memset to 0, since everything gets overwritten anyways

    tex->width = w;
    tex->height = h;
    tex->path[0] = '\0';
    tex->format = format;
    
    // Generate an opengl id
    glGenTextures(1, &tex->id);
    
    return handle;
}


void textures_init(void) {
    if (_Texture_pool.items == NULL) {
        _Texture_pool.items = TextureArray_create(16);
        _Texture_pool.free_list = IndexArray_create(16);
        
        // Create a magenta fallback texture at slot 0
        TextureHandle fallback_handle = texture_new(2,2,GL_RGBA);
        Texture* fallback_tex = Texture_get(fallback_handle);

        unsigned char magenta_pixel[16] = { 255, 0, 255, 255,     0,   0,   0, 255,
                                            0,   0,   0, 255,     255, 0, 255, 255};

        glBindTexture(GL_TEXTURE_2D, fallback_tex->id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, magenta_pixel);

        snprintf(fallback_tex->path, MAX_TEXTURE_PATH, "INTERNAL_FALLBACK");
        
        LOG_INFO("Texture Resource: Initialized (Capacity: %u, Pool: %p)",
                _Texture_pool.items->capacity, (void*)&_Texture_pool);
    }
    else {
        LOG_WARN("Attempted to reinitialize texture resource. Ignoring.");
    }
}

TextureHandle texture_load(const char* filepath, bool premultiply_alpha) {
    ENGINE_ASSERT(filepath != NULL, "texture_load called with NULL filepath");

    stbi_set_flip_vertically_on_load(false);
    
    int width, height, channels;
    unsigned char* data = stbi_load(filepath, &width, &height, &channels, 0);

    if (UNLIKELY(!data)) {
        LOG_ERROR("Failed to load texture: '%s'", filepath);
        return TextureHandle_null();
    }
    
    TextureHandle handle = texture_new(width, height, (channels == 4) ? GL_RGBA : GL_RGB);
    Texture* tex = Texture_get(handle);
    
    if (premultiply_alpha) premult_alpha(tex, data);

    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // GL_NEAREST for Pixel Art

    glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->width, tex->height, 0, tex->format, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    snprintf(tex->path, MAX_TEXTURE_PATH, "%s", filepath);

    return handle;
}


/*
 * Defines the struct which is used to parse the .bin format header
*/
typedef struct {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    uint32_t internal_format;
    uint32_t data_size;
} BakedHeader;
TextureHandle texture_load_etc2_bin(const char* filepath) {
    ENGINE_ASSERT(filepath != NULL, "texture_load_etc2_bin called with NULL filepath");

    FILE* f = fopen(filepath, "rb");
    if(UNLIKELY(!f)) {
        LOG_ERROR("Error: Could not open ETC2 texture: '%s'", filepath);
        return TextureHandle_null();
    }
    
    // Read the header
    BakedHeader header;
    if (UNLIKELY(fread(&header, sizeof(BakedHeader), 1, f) != 1)) {
        LOG_ERROR("Could not read header from ETC2 file: '%s'", filepath);
        fclose(f);
        return TextureHandle_null();
    }
    
    // Validate the magic number in header
    if (UNLIKELY(header.magic != 0x58455442)) {
        LOG_ERROR("Invalid magic number in ETC2 file header: '%s'", filepath);
        fclose(f);
        return TextureHandle_null();
    }

    // Read the compressed data
    void* compressed_data = malloc(header.data_size);
    ENGINE_ASSERT(compressed_data != NULL, "Out of memory allocating compressed texture data");

    if (UNLIKELY(fread(compressed_data, 1, header.data_size, f) != header.data_size)) {
        LOG_ERROR("Could not read full texture data: '%s'", filepath);
        free(compressed_data);
        fclose(f);
        return TextureHandle_null();
    }
    fclose(f);
    
    TextureHandle handle = texture_new(header.width, header.height, header.internal_format);
    Texture* tex = Texture_get(handle); 
    
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCompressedTexImage2D(
        GL_TEXTURE_2D,
        0,
        tex->format, // From header. Likely stores GL_COMPRESSED_RGBA8_ETC2_EAC
        tex->width,
        tex->height,
        0,
        (GLsizei)header.data_size,
        compressed_data
    );

    free(compressed_data);

    snprintf(tex->path, MAX_TEXTURE_PATH, "%s", filepath);
    return handle;
}

void texture_bind(TextureHandle handle, uint32_t slot) {
    Texture* tex = Texture_get(handle);
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, tex->id);
}

/*
 * Texture specific unload callback
 *
 * Used when freeing a texture to the free list, and when shutting down
*/
static void texture_unload_internal(Texture* tex) {
    if (tex->id != 0) {
        glDeleteTextures(1, &tex->id);
        tex->id = 0;
    }
}

/*
 * Deletes the texture from the pool and puts it in the free list
 *
 * Increments the texture's slot's generation number
*/
void texture_delete(TextureHandle handle) {
    Texture* tex = Texture_get(handle);
    // index 0 is the fallback, which shouldn't be deleted
    if (UNLIKELY(tex == &_Texture_pool.items->data[0] || !tex->active)) return;
    
    texture_unload_internal(tex);
    
    // Return the slot to the free list
    Texture_free(handle);
}

/*
 * Clean up all texture data
 *
 * Uses the texture unload as a callback to glDeleteTextures
*/
void textures_free(void) {
    Texture_pool_shutdown(texture_unload_internal);
}

/* ---------- TEXTURE ATLAS ---------- */

TextureAtlas* atlas_create(TextureHandle handle, uint32_t initial_capacity) {
    Texture* texture = Texture_get(handle);
    TextureAtlas* atlas = malloc(sizeof(TextureAtlas));
    ENGINE_ASSERT(atlas != NULL, "Out of memory allocating TextureAtlas");
    atlas->parent_texture = handle;
    atlas->width = texture->width;
    atlas->height = texture->height;
    atlas->count = 0;
    
    // Double the requested size to reduce load factor to 50%
    uint32_t target = initial_capacity * 2;
    if (target < 16) target = 16; // Minimum capacity
    atlas->capacity = round_to_pow2(target); // Keep capacity a power of 2
    atlas->slots = calloc(atlas->capacity, sizeof(AtlasSlot));
    ENGINE_ASSERT(atlas->slots != NULL, "Out of memory allocating Atlas slots");
    
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
    ENGINE_ASSERT(atlas->slots != NULL, "Out of memory during Atlas resize");
    
    // Rehash every entry for the new size
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
    ENGINE_ASSERT(atlas != NULL, "atlas_define_region called with NULL atlas");
    ENGINE_ASSERT(name != NULL, "atlas_define_region called with NULL name");

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
    
    // Linear probe until empty slot or an exact string match (overwrite)
    while (atlas->slots[index].occupied) {
        if (atlas->slots[index].key_hash == hash && strcmp(atlas->slots[index].name, name) == 0) {
            break; // Overwrite existing region with the same name
        }
        index = (index + 1) & mask;
    }

    AtlasSlot* slot = &atlas->slots[index];

    if (!slot->occupied) {
        slot->key_hash = hash;
        slot->occupied = true;
        strncpy(slot->name, name, MAX_REGION_NAME - 1);
        slot->name[MAX_REGION_NAME - 1] = '\0';
        atlas->count++;
    }


    float inv_w = 1.0f / (float)atlas->width;
    float inv_h = 1.0f / (float)atlas->height;

    slot->region.uv_min[0] = (float)x * inv_w;
    slot->region.uv_min[1] = (float)y * inv_h;
    slot->region.uv_max[0] = (float)(x + w) * inv_w;
    slot->region.uv_max[1] = (float)(y + h) * inv_h;

    slot->region.width = w;
    slot->region.height = h;
}

TextureRegion* atlas_get_region(TextureAtlas* atlas, const char* name) {
    ENGINE_ASSERT(atlas != NULL, "atlas_get_region called with NULL atlas");
    ENGINE_ASSERT(name != NULL, "atlas_get_region called with NULL name");
    
    uint32_t hash = hash_string(name);

    uint32_t mask = atlas->capacity - 1;
    
    uint32_t index = hash & mask;
    uint32_t start_index = index;

    while (atlas->slots[index].occupied) {
        if (atlas->slots[index].key_hash == hash && strncmp(atlas->slots[index].name, name, MAX_REGION_NAME) == 0) {
            return &atlas->slots[index].region;
        }
        index = (index + 1) & mask;
        if (index == start_index) break; // Traversed whole table
    }

    LOG_WARN("Atlas region '%s' not found.", name);
    return NULL;
}

void atlas_free(TextureAtlas* atlas) {
    // Probably dont want to delete the texture when atlas is freed
    // Since atlas might be freed after it's no longer need, but there might still be objects that use the texture
    //texture_delete(atlas->parent_texture);
    free(atlas->slots);
    free(atlas);
}
