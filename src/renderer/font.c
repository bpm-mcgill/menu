#include "renderer/font.h"
#include "engine.h"
#include "renderer/vertex.h"
#include "renderer/texture.h"
#include "utils/handles.h"
#include <GLES3/gl3.h>
#include <cglm/vec2.h>
#include <stdio.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// TODO: Add font kerning support

/*
 * Internal font management
*/
DEFINE_RESOURCE_POOL(Font, FontHandle, FontArray);

/* -------- System Initialization -------- */

void fonts_init() {
    if (_Font_pool.items == NULL) {
        _Font_pool.items = FontArray_create(16);
        _Font_pool.free_list = IndexArray_create(16);

        // TODO: Make the fallback a simple bitmap font
        Font fallback = { .active = true, .generation = 0, .type = FONT_TYPE_BITMAP };
        _Font_pool.items = FontArray_push(_Font_pool.items, fallback);
        
        LOG_INFO("Font Resource: Initialized (Capacity: %u, Pool: %p)",
                _Font_pool.items->capacity, (void*)&_Font_pool);
    }
    else {
        LOG_WARN("Attempted to reinitialize fonts resource. Ignoring.");
    }
}

/* -------- Asset Loading -------- */

FontHandle font_load(const char* ttf_path, float font_size) {
    ENGINE_ASSERT(ttf_path != NULL, "font_load called with NULL path");

    // 1. Read the TTF file info memory
    FILE* f = fopen(ttf_path, "rb");
    if (UNLIKELY(!f)) {
        LOG_ERROR("Failed to open font file: '%s'", ttf_path);
        return FontHandle_null();
    }

    fseek(f, 0, SEEK_END);
    long f_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char* ttf_buffer = (unsigned char*)malloc(f_size);
    ENGINE_ASSERT(ttf_buffer != NULL, "Out of memory allocating TTF buffer");
    
    if (UNLIKELY(fread(ttf_buffer, 1, f_size, f) != (size_t)f_size)) {
        LOG_ERROR("Failed to read entire font file: '%s'", ttf_path);
        free(ttf_buffer);
        fclose(f);
        return FontHandle_null();
    }
    fclose(f);

    // 2. Allocate memory for the bitmap
    unsigned char* temp_bitmap = (unsigned char*)malloc(FONT_ATLAS_WIDTH * FONT_ATLAS_HEIGHT);

    // 3. Allocate the Font struct
    Font font = {
        .active = true,
        .type = FONT_TYPE_BITMAP,
        .px_range = 0,
        .atlas_w = FONT_ATLAS_WIDTH,
        .atlas_h = FONT_ATLAS_HEIGHT,
        .pixel_height = font_size,
        .ascent = font_size,
        .descent = 0,
        .gen_size = font_size,
        .line_gap = 0
    };

    // 4. Bake the font into a bitmap
    int result = stbtt_BakeFontBitmap(
        ttf_buffer,
        0,
        font_size,
        temp_bitmap,
        FONT_ATLAS_WIDTH,
        FONT_ATLAS_HEIGHT,
        32,
        96,
        font.data.bitmap
    );

    if (result <= 0) {
        LOG_ERROR("Failed to bake font bitmap: '%s'", ttf_path);
        free(ttf_buffer);
        free(temp_bitmap);
        return FontHandle_null();
    }

    TextureHandle texture = texture_new(FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT, GL_R8);
    font.texture = texture;

    texture_bind(texture, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // 6. Upload bitmap to Texture
    glTexImage2D(
        GL_TEXTURE_2D, 
        0,
        GL_R8,
        FONT_ATLAS_WIDTH,
        FONT_ATLAS_HEIGHT,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        temp_bitmap
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 7. Calculate Vertical Metrics
    //  Used for newlines and centering
    stbtt_fontinfo info;
    if (stbtt_InitFont(&info, ttf_buffer, 0)) {
        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

        // Convert unscaled design units to pixels
        float scale = stbtt_ScaleForPixelHeight(&info, font_size);
        font.ascent = ascent * scale;
        font.descent = descent * scale;
        font.line_gap = line_gap * scale;
    }
    
    // 8. Clean up and return
    free(ttf_buffer);
    free(temp_bitmap);
    
    // Create or reuse a font slot and copy the data into it
    FontHandle handle = Font_alloc();
    Font* fontptr = Font_get(handle);
    font.generation = fontptr->generation; // Copy slot's old generation
    memcpy(fontptr, &font, sizeof(Font));
    
    return handle;
}

/*
 * Used for reading the .bin font file header
*/
typedef struct {
    char magic[4];
    float size;        // The font size the atlas was generated at
    float px_range;
    uint32_t w;        // Atlas width
    uint32_t h;        // Atlas height
    float line_height; // Vertical spacing
    float ascent;      // Max height above baseline
    float descent;     // Max depth below baseline
    uint32_t glyph_count;
} FontHeader;

FontHandle font_load_bin(const char* path) {
    ENGINE_ASSERT(path != NULL, "font_load_bin called with NULL path");

    // 1. Read the TTF file info memory
    FILE* f = fopen(path, "rb");
    if (UNLIKELY(!f)) {
        LOG_ERROR("Failed to open font .bin file: '%s'", path);
        return FontHandle_null();
    }
    
    FontHeader header;
    if (UNLIKELY(fread(&header, sizeof(FontHeader), 1, f) != 1)) {
        LOG_ERROR("Failed to read header from font .bin file: '%s'", path);
        fclose(f);
        return FontHandle_null();
    }
    
    // 3. Magic number validation
    if (UNLIKELY(strncmp(header.magic, "FONT", 4) != 0)) {
        LOG_ERROR("Invalid magic number in font .bin header: '%s'", path);
        fclose(f);
        return FontHandle_null();
    }
    
    // 4. Made new font
    // If nothing fails, this will be copied into a new font slot
    Font font = {
        .active = true,
        .type = FONT_TYPE_MSDF,
        .px_range = header.px_range,
        .atlas_w = (float)header.w,
        .atlas_h = (float)header.h,
        .pixel_height = header.line_height,
        .ascent = header.ascent,
        .descent = header.descent,
        .gen_size = header.size
    };

    // 5. Read Glyph Table (96 entries)
    size_t glyphs_read = fread(&font.data.msdf, sizeof(Glyph), header.glyph_count, f);
    if (UNLIKELY(glyphs_read != header.glyph_count)) {
        LOG_ERROR("Incomplete glyph table in: '%s'", path);
        fclose(f);
        return FontHandle_null();
    }

    // 6. Read pixels
    size_t img_size = header.w * header.h * 3; // RGB
    unsigned char* pixels = (unsigned char*)malloc(img_size);
    if (UNLIKELY(fread(pixels, 1, img_size, f) != img_size)) {
        fprintf(stderr, "Error: Truncated texture data in %s\n", path);
        free(pixels);
        fclose(f);
        return FontHandle_null();
    }

    fclose(f);
    
    TextureHandle texture = texture_new(header.w, header.h, GL_RGB8);
    font.texture = texture;
    texture_bind(texture, 1);
    
    // 5. Upload to Texture
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 
        0,
        GL_RGB8,
        header.w,
        header.h,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        pixels
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(pixels);
    
    // Create or reuse a font slot and copy the data into it
    FontHandle handle = Font_alloc();
    Font* fontptr = Font_get(handle);
    font.generation = fontptr->generation; // Copy slot's old generation
    memcpy(fontptr, &font, sizeof(Font));
    
    return handle;
}

void fonts_free(void) {
    Font_pool_shutdown(NULL);
}

void font_delete(FontHandle handle) {
    Font* font = Font_get(handle);
    // index 0 is the fallback, which shouldn't be deleted
    if (font == &_Font_pool.items->data[0] || !font->active) return;
    
    // Delete the texture associated with the font
    texture_delete(font->texture);
    
    Font_free(handle);
}

// Converts String -> MeshData
MeshData font_generate_mesh_data(FontHandle handle, const char* text, TextParams params){
    int len = strlen(text);
    if (len == 0) return (MeshData){0};

    Font* font = Font_get(handle);

    // 1. Set up MeshData
    MeshData md;
    md.vertex_count = len * 4;
    md.index_count = len * 6;
    md.vertices = malloc(md.vertex_count * sizeof(UIVertex));
    md.indices = malloc(md.index_count * sizeof(uint32_t));
    ENGINE_ASSERT(md.vertices && md.indices, "Out of memory allocating Font Mesh");

    UIVertex* verts = (UIVertex*)md.vertices;
    
    // Cursor position
    float x = 0.0f;
    float y = 0.0f;

    // Pre-calculate inverse size for UV normalization
    float inv_w = 1.0f / font->atlas_w;
    float inv_h = 1.0f / font->atlas_h;
    
    // Covert 4-byte color array into single 32-bit int for fast copying
    uint32_t color_packed = *((uint32_t*)params.color);

    int q_idx = 0; // Valid quad counter
    for (int i = 0; i < len; i++) {
        if (text[i] < 32 || text[i] >= 128) continue;
        int glyph_idx = text[i] - 32;

        float x0, y0, x1, y1; // Vertex Positions
        float u0, v0, u1, v1; // Texture Coordinates
        
        if (font->type == FONT_TYPE_MSDF) {
            Glyph* g = &font->data.msdf[glyph_idx];

            // 1. Calculate Screen Coordinates
            // Plane bounds are normalized (0.0 to 1.0 relative to font size), so they are mutliplied by size
            
            // Left & Right
            x0 = x + (g->plane_l * params.size);
            x1 = x + (g->plane_r * params.size);

            // Top & Bottom
            // g->plane_t is usually positive (distance UP from baseline)
            y0 = y - (g->plane_t * params.size); 
            y1 = y - (g->plane_b * params.size); 

            // 2. Calculate UVs (Normalized)
            u0 = g->atlas_l * inv_w;
            v0 = g->atlas_b * inv_h;
            u1 = g->atlas_r * inv_w;
            v1 = g->atlas_t * inv_h;

            // 3. Advance Cursor
            x += g->advance * params.size;
        }
        else {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->data.bitmap, font->atlas_w, font->atlas_h, glyph_idx, &x, &y, &q, 1);
            
            // Scale needs to be calculated so it can to relative to the base bitmap atlas
            float scale = params.size / font->gen_size;
            if (scale != 1.0f) {
                q.x0 *= scale; q.y0 *= scale;
                q.x1 *= scale; q.y1 *= scale;
            }

            x0 = q.x0; x1 = q.x1;
            y0 = q.y0; y1 = q.y1;
            u0 = q.s0; u1 = q.s1;
            v0 = q.t0; v1 = q.t1;
        }

        // --- Fill vertices ---
        
        int v_start = q_idx * 4;

        verts[v_start + 0].pos[0] = x0; verts[v_start + 0].pos[1] = y1;
        verts[v_start + 0].uv[0]  = u0; verts[v_start + 0].uv[1]  = v1;
        
        verts[v_start + 1].pos[0] = x1; verts[v_start + 1].pos[1] = y1;
        verts[v_start + 1].uv[0]  = u1; verts[v_start + 1].uv[1]  = v1;
        
        verts[v_start + 2].pos[0] = x1; verts[v_start + 2].pos[1] = y0;
        verts[v_start + 2].uv[0]  = u1; verts[v_start + 2].uv[1]  = v0;
        
        verts[v_start + 3].pos[0] = x0; verts[v_start + 3].pos[1] = y0;
        verts[v_start + 3].uv[0]  = u0; verts[v_start + 3].uv[1]  = v0;

        *((uint32_t*)verts[v_start + 0].color) = color_packed;
        *((uint32_t*)verts[v_start + 1].color) = color_packed;
        *((uint32_t*)verts[v_start + 2].color) = color_packed;
        *((uint32_t*)verts[v_start + 3].color) = color_packed;

        // --- Fill indices ---
        int i_start = q_idx * 6;
        md.indices[i_start + 0] = v_start + 0;
        md.indices[i_start + 1] = v_start + 1;
        md.indices[i_start + 2] = v_start + 2;
        md.indices[i_start + 3] = v_start + 2;
        md.indices[i_start + 4] = v_start + 3;
        md.indices[i_start + 5] = v_start + 0;
        
        q_idx++;
    }
    
    // Update the final counts
    //   If a char was skipped because it was unsupported, the counts will be wrong
    //   So the counts need to be updated based on the actual working quads generated
    md.vertex_count = q_idx * 4;
    md.index_count = q_idx * 6;

    // Only realloc if characters were skipped
    if (q_idx < len) {
        md.vertices = realloc(md.vertices, md.vertex_count * sizeof(UIVertex));
        md.indices = realloc(md.indices, md.index_count * sizeof(uint32_t));
    }

    return md;
}
