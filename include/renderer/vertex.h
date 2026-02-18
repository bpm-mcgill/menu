#ifndef VERTEX_H
#define VERTEX_H

#include "renderer/texture.h"
#include <cglm/types.h>
#include <stddef.h>
#include <stdint.h>

// Stores mesh data that will get pushed to a MeshObj
// OR 
// Stores pointers to data fetched by handle from a MeshObj
// NOTE: Could potentially store VertexLayout in MeshData
typedef struct {
    void* vertices;
    uint32_t* indices;
    uint32_t vertex_count;
    uint32_t index_count;
} MeshData;

/* ---------- VERTEX ATTRIBUTES ---------- */

#define MAX_VERTEX_ATTRIBS 8

// Defines a single vertex attribute
// TODO: Define an enum that will define attrib usage/types
// For example:
// typedef enum {
//    ATTRIB_USAGE_POSITION,
//    ATTRIB_USAGE_UV,
//    ATTRIB_USAGE_COLOR,
//    ...
// } AttribUsage;
// This way, the vertex functions will be sure it's function is being applied correctly
typedef struct {
    uint32_t index;
    int32_t size;
    uint32_t type;
    unsigned char normalized; // GL_TRUE || GL_FALSE
    size_t offset;
} VertexAttribute;

// Defines the vertex attribute layout for an entire mesh
typedef struct {
    VertexAttribute attributes[MAX_VERTEX_ATTRIBS]; // Fixed size of 8 attributes
    uint32_t count;
    uint32_t stride;

    uint32_t id;
} VertexLayout;

void vertex_layout_add(VertexLayout* layout, uint32_t index, int32_t size, uint32_t type, bool normalized);
void vertex_layout_apply(VertexLayout* layout);

/*
 * ------ UI Elements ------
*/
typedef struct {
    vec2 pos;
    vec2 uv;
    uint8_t color[4];
} UIVertex;

VertexLayout uiv_layout();

MeshData uiv_gen_quad();
void uiv_apply_region(MeshData* data, TextureRegion* region);
void uiv_apply_mat4(MeshData* data, VertexLayout layout, mat4 matrix);

/*
 * ------ Framebuffer ------
*/
typedef struct {
    vec2 pos;
    vec2 uv;
} FBOVertex;

VertexLayout fbv_layout();

MeshData fbv_gen_quad();

#endif // !VERTEX_H
