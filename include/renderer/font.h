#ifndef FONT_H
#define FONT_H

#include "renderer/mesh.h"
#include "renderer/texture.h"
#include "stb_truetype.h"
#include <cglm/types.h>

#define FONT_ATLAS_WIDTH 512
#define FONT_ATLAS_HEIGHT 512

typedef enum {
    FONT_TYPE_BITMAP,
    FONT_TYPE_MSDF
} FontType;

typedef struct {
    float advance;
    float plane_l, plane_b, plane_r, plane_t;
    float atlas_l, atlas_b, atlas_r, atlas_t;
} Glyph;

typedef struct {
    Texture* texture;    // The Font bitmap or MSDF Atlas texture
    FontType type;       // BITMAP or MSDF

    // Global Metrics
    float pixel_height;  // The size the font was generated at (e.g. 32px)
    float ascent;
    float descent;
    float line_gap;

    // MSDF font data
    float px_range;      // The spread/softness range
    float atlas_w;
    float atlas_h;

    // ASCII 32-126 metrics
    union {
        stbtt_bakedchar bitmap[96];
        Glyph msdf[96];
    } data;
} Font;



typedef struct {
    vec4 color;
    float scale;      // 1.0 = pixel_height size
    float softness;   // SDF shader softness (0.0 hard edge, 1.0 = blurry)
    float wrap_width; // 0 = no wrap
    bool center_align;
} TextParams;

typedef struct {
    MeshHandle handle;     // The font's MeshData inside font batch
    MeshObj* parent_batch; // Pointer to the batch this lives in
    Font* font;            // Pointer to the font asset
    
    char* current_text;
    TextParams current_params;
} TextLabel;

// ------ API ------

// Asset Management
Font* font_load(const char* ttf_path, float font_size);
Font* font_load_bin(const char* path);
void font_destroy(Font* font);

// Converts String -> MeshData
MeshData font_generate_mesh_data(Font* font, const char* text, TextParams params);

// High Level Label API
void text_label_init(TextLabel* label, MeshObj* batch, Font* font);
void text_label_set(TextLabel* label, const char* text, TextParams);
void text_label_destroy(TextLabel* label);

#endif // !FONT_H
