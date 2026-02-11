#ifndef FONT_H
#define FONT_H

#include "renderer/mesh.h"
#include "renderer/texture.h"
#include "stb_truetype.h"
#include <cglm/types.h>

// Bitmap font atlas dimensions
//   If a big font size is to be used, this needs to be larger to fit
#define FONT_ATLAS_WIDTH 512
#define FONT_ATLAS_HEIGHT 512

typedef enum {
    FONT_TYPE_BITMAP,
    FONT_TYPE_MSDF
} FontType;

/* MSDF Glyph data */
typedef struct {
    float advance;
    float plane_l, plane_b, plane_r, plane_t; // Vertex pos
    float atlas_l, atlas_b, atlas_r, atlas_t; // UVs
} Glyph;

typedef struct {
    Texture* texture;    // The Font bitmap or MSDF Atlas texture
    FontType type;       // BITMAP or MSDF

    // Global Metrics
    float pixel_height;  // The size the font was generated at (e.g. 32px)
    float ascent;
    float descent;
    float line_gap;
    float gen_size;      // The size the bitmap or MSDF font was generated at

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
    float size;       // The target font size.
                      //  - Will scale the atlas based on gen_size (target_size / gen_size)
    float softness;   // MSDF shader softness (0.0 hard edge, 1.0 = blurry)
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

#endif // !FONT_H
