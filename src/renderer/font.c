#include "renderer/font.h"
#include "renderer/mesh.h"
#include "renderer/texture.h"
#include <GLES3/gl3.h>
#include <cglm/vec2.h>
#include <stdio.h>
#include <string.h>

Font* font_load(const char* ttf_path, float font_size) {
    // 1. Read the TTF file info memory
    FILE* f = fopen(ttf_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Failed to open font file: %s\n", ttf_path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long f_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char* ttf_buffer = (unsigned char*)malloc(f_size);
    fread(ttf_buffer, 1, f_size, f);
    fclose(f);

    // 2. Allocate memory for the bitmap
    unsigned char* temp_bitmap = (unsigned char*)malloc(FONT_ATLAS_WIDTH * FONT_ATLAS_HEIGHT);

    // 3. Allocate the Font struct
    Font* font = (Font*)calloc(1, sizeof(Font));
    font->type = FONT_TYPE_BITMAP;
    font->pixel_height = font_size;
    font->atlas_w = FONT_ATLAS_WIDTH;
    font->atlas_h = FONT_ATLAS_HEIGHT;

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
        font->data.bitmap
    );

    if (result <= 0) {
        fprintf(stderr, "Error: Failed to bake font bitmap.\n");
        free(ttf_buffer);
        free(temp_bitmap);
        free(font->texture);
        free(font);
        return NULL;
    }

    // 5. Allocate a new texture for the bitmap
    font->texture = calloc(1, sizeof(Texture));
    font->texture->width = FONT_ATLAS_WIDTH;
    font->texture->height = FONT_ATLAS_HEIGHT;
    font->texture->channels = 1; // Only red channel for fonts
    snprintf(font->texture->path, MAX_TEXTURE_PATH, "%s", ttf_path); // Copy ttf path to texture path

    // 6. Turn bitmap into a texture
    glGenTextures(1, &font->texture->id);
    glBindTexture(GL_TEXTURE_2D, font->texture->id);

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
        font->ascent = ascent * scale;
        font->descent = descent * scale;
        font->line_gap = line_gap * scale;
    }
    else {
        // Fallback if init fails
        font->ascent = font_size;
        font->descent = 0;
        font->line_gap = 0;
    }
    
    // 8. Clean up and return
    free(ttf_buffer);
    free(temp_bitmap);
    return font;
}

typedef struct {
    char magic[4];
    float px_range;
    uint32_t w; // Atlas width
    uint32_t h; // Atlas height
    uint32_t glyph_count;
} FontHeader;

Font* font_load_bin(const char* path) {
    // 1. Read the .bin file info memory
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Failed to open font .bin file: %s\n", path);
        return NULL;
    }
    
    FontHeader header;
    if (fread(&header, sizeof(FontHeader), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    // 3. Magic number validation
    if (strncmp(header.magic, "FONT", 4) != 0) {
        fprintf(stderr, "Error: Invalid font format in: %s\n", path);
        fclose(f);
        return NULL;
    }
    
    // 4. Allocate a new font and texture for the atlas
    Font* font = calloc(1, sizeof(Font));
    font->texture = calloc(1, sizeof(Texture));
    
    font->type = FONT_TYPE_MSDF;
    font->px_range = header.px_range;
    font->atlas_w = (float)header.w;
    font->atlas_h = (float)header.h;
    font->texture->width = header.w;
    font->texture->height = header.h;

    font->pixel_height = 32.0f;
    font->ascent = 32.0f;
    font->descent = 0.0f;

    // 5. Read Glyph Table (96 entries)
    size_t glyphs_read = fread(font->data.msdf, sizeof(Glyph), header.glyph_count, f);
    if (glyphs_read != header.glyph_count) {
        fprintf(stderr, "Error: Incomplete glyph table in %s\n", path);
        free(font->texture);
        free(font);
        fclose(f);
        return NULL;
    }

    // 6. Read pixels
    size_t img_size = header.w * header.h * 3; // RGB
    unsigned char* pixels = (unsigned char*)malloc(img_size);
    if (fread(pixels, 1, img_size, f) != img_size) {
        fprintf(stderr, "Error: Truncated texture data in %s\n", path);
        free(pixels);
        free(font->texture);
        free(font);
        fclose(f);
        return NULL;
    }

    fclose(f);
    
    // 5. Upload to GPU
    glGenTextures(1, &font->texture->id);
    glBindTexture(GL_TEXTURE_2D, font->texture->id);

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
    return font;
}

void font_destroy(Font* font);

typedef struct {
    vec2 pos;
    vec2 uv;
    vec4 color;
} Vertex;
// Converts String -> MeshData
MeshData font_generate_mesh_data(Font* font, const char* text, TextParams params){
    int len = strlen(text);
    if (len == 0) return (MeshData){0};

    // 1. Set up MeshData
    MeshData md;
    md.vertex_count = len * 4;
    md.index_count = len * 6;
    md.vertices = malloc(md.vertex_count * sizeof(Vertex));
    md.indices = malloc(md.index_count * sizeof(uint32_t));

    Vertex* verts = (Vertex*)md.vertices;
    
    // Cursor position
    float x = 0.0f;
    float y = 0.0f;

    // Pre-calculate inverse size for UV normalization
    float inv_w = 1.0f / font->atlas_w;
    float inv_h = 1.0f / font->atlas_h;

    int q_idx = 0; // Valid quad counter
    for (int i = 0; i < len; i++) {
        if (text[i] < 32 || text[i] >= 128) continue;
        int glyph_idx = text[i] - 32;

        float x0, y0, x1, y1; // Vertex Positions
        float u0, v0, u1, v1; // Texture Coordinates
        
        if (font->type == FONT_TYPE_MSDF) {
            Glyph* g = &font->data.msdf[glyph_idx];

            // 1. Calculate Screen Coordinates
            // Plane bounds are normalized (0.0 to 1.0 relative to font size), 
            // so we multiply by params.scale.
            // Note: In Y-Down (UI), "Top" means Subtract, "Bottom" means Add.
            
            // Left & Right
            x0 = x + (g->plane_l * params.scale);
            x1 = x + (g->plane_r * params.scale);

            // Top & Bottom (Assuming Y-Down coordinate system)
            // g->plane_t is usually positive (distance UP from baseline)
            y0 = y - (g->plane_t * params.scale); 
            y1 = y - (g->plane_b * params.scale); 

            // 2. Calculate UVs (Normalized)
            u0 = g->atlas_l * inv_w;
            v0 = g->atlas_b * inv_h; // Depending on atlas gen, T/B might be swapped
            u1 = g->atlas_r * inv_w;
            v1 = g->atlas_t * inv_h;

            // 3. Advance Cursor
            x += g->advance * params.scale;
        }
        else {
            stbtt_bakedchar* b = &font->data.bitmap[glyph_idx];
            stbtt_aligned_quad q;

            stbtt_GetBakedQuad(font->data.bitmap, font->atlas_w, font->atlas_h, glyph_idx, &x, &y, &q, 1);

            float s = params.scale;
            if (s != 1.0f) {
                q.x0 *= s; q.y0 *= s;
                q.x1 *= s; q.y1 *= s;
            }

            x0 = q.x0; x1 = q.x1;
            y0 = q.y0; y1 = q.y1;
            u0 = q.s0; u1 = q.s1;
            v0 = q.t0; v1 = q.t1;
        }

        // --- Fill vertices ---
        
        int v_start = q_idx * 4;

        // Vert 0: Bottom-Left (x0, y1)
        glm_vec2_copy((vec2){x0, y1}, verts[v_start + 0].pos);
        glm_vec2_copy((vec2){u0, v0}, verts[v_start + 0].uv); // Use Bottom V

        // Vert 1: Bottom-Right (x1, y1)
        glm_vec2_copy((vec2){x1, y1}, verts[v_start + 1].pos);
        glm_vec2_copy((vec2){u1, v0}, verts[v_start + 1].uv); // Use Bottom V

        // Vert 2: Top-Right (x1, y0)
        glm_vec2_copy((vec2){x1, y0}, verts[v_start + 2].pos);
        glm_vec2_copy((vec2){u1, v1}, verts[v_start + 2].uv); // Use Top V

        // Vert 3: Top-Left (x0, y0)
        glm_vec2_copy((vec2){x0, y0}, verts[v_start + 3].pos);
        glm_vec2_copy((vec2){u0, v1}, verts[v_start + 3].uv); // Use Top V

        // Apply color to all 4 vertices
        for (int k = 0; k < 4; k++) {
            glm_vec4_copy(params.color, verts[v_start + k].color);
        }

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

    return md;
}


// High Level Label API
void text_label_init(TextLabel* label, MeshObj* batch, Font* font);
void text_label_set(TextLabel* label, const char* text, TextParams);
void text_label_destroy(TextLabel* label);
