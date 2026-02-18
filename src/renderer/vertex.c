#include "renderer/vertex.h"
#include <GLES3/gl3.h>
#include <cglm/mat4.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// INFO: If layout usages are implemented, checks can be added in each
// Applier function to ensure the applications are on the correct data
// Reduce assumptions and be sure

/* ---------- VERTEX ATTRIBUTES ---------- */

/*
 * Adds a VertexAttribute to the VertexLayout
 *
 * Will skip if you try to add more vertex attributes than `MAX_VERTEX_ATTRIBS`
 * Keeps track of stride for attribute offsets
 *
 * If the `type` isn't recognized, the type size will default to 4
*/
void vertex_layout_add(VertexLayout* layout, uint32_t index, int32_t size, uint32_t type, bool normalized) {
    if (layout->count >= MAX_VERTEX_ATTRIBS) {
        printf("Trying to add too many vertex attributes. The max is 8\n");
        return;
    }

    VertexAttribute* attr = &layout->attributes[layout->count];
    attr->index = index;
    attr->size = size;
    attr->type = type;
    attr->normalized = normalized ? GL_TRUE : GL_FALSE;
    attr->offset = layout->stride;

    // Increment stride
    uint32_t type_size = 0;
    switch (type) {
        case GL_FLOAT:         type_size = sizeof(float); break;
        case GL_UNSIGNED_BYTE: type_size = sizeof(uint8_t); break;
        case GL_SHORT:         type_size = sizeof(int16_t); break;
        case GL_INT:           type_size = sizeof(int32_t); break;
        default:
            printf("Couldn't find vertex attrib type, defaulting.\n");
            type_size = 4; break;
    }

    layout->stride += (size * type_size);
    layout->count++;
}

/*
 * Takes a VertexLayout and enables all of it's stored the vertex attributes
*/
void vertex_layout_apply(VertexLayout* layout) {
    for (uint32_t i = 0; i < layout->count; i++) {
        VertexAttribute* attr = &layout->attributes[i];

        glEnableVertexAttribArray(attr->index);
        glVertexAttribPointer(
            attr->index,
            attr->size,
            attr->type,
            attr->normalized,
            layout->stride,
            (void*)attr->offset
        );
    }
}


/*
 * ------ UI Elements ------
*/

VertexLayout uiv_layout() {
    VertexLayout layout = {0};
    vertex_layout_add(&layout, 0, 2, GL_FLOAT, false);        // POS
    vertex_layout_add(&layout, 1, 2, GL_FLOAT, false);        // UV

    // OpenGL will take the uint8_t and normalize it into a float for us
    vertex_layout_add(&layout, 2, 4, GL_UNSIGNED_BYTE, true); // COLOR

    return layout;
}

MeshData uiv_gen_quad() {
    MeshData data;

    data.vertex_count = 4;
    data.index_count = 6;
    
    data.vertices = calloc(data.vertex_count, sizeof(UIVertex));
    data.indices = calloc(data.index_count, sizeof(uint32_t));
    
    UIVertex* v = (UIVertex*)data.vertices;

    v[0] = (UIVertex){{-1.0f, -1.0f}, {0.0f, 0.0f}, {255, 255, 255, 255}};
    v[1] = (UIVertex){{ 1.0f, -1.0f}, {1.0f, 0.0f}, {255, 255, 255, 255}};
    v[2] = (UIVertex){{ 1.0f,  1.0f}, {1.0f, 1.0f}, {255, 255, 255, 255}};
    v[3] = (UIVertex){{-1.0f,  1.0f}, {0.0f, 1.0f}, {255, 255, 255, 255}};
    
    uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };
    memcpy(data.indices, indices, sizeof(indices));

    return data;
}

/*
 * Applies a texture region's uv to a MeshData (quad)
 * This will only work on MeshData objects using the default UIVertex format
 * Specifically, this will only work with quads
 *
 * Useful for creating icons from a texture atlas
*/
void uiv_apply_region(MeshData* data, TextureRegion* region) {
    if (data->vertex_count != 4) return; // 4 vertices for a quad are required

    UIVertex* v = (UIVertex*)data->vertices;

    // v[0] - Bottom-Left
    v[0].uv[0] = region->uv_min[0]; 
    v[0].uv[1] = region->uv_min[1];

    // v[1] - Bottom-Right
    v[1].uv[0] = region->uv_max[0]; 
    v[1].uv[1] = region->uv_min[1];

    // v[2] - Top-Right
    v[2].uv[0] = region->uv_max[0]; 
    v[2].uv[1] = region->uv_max[1];

    // v[3] - Top-Left
    v[3].uv[0] = region->uv_min[0]; 
    v[3].uv[1] = region->uv_max[1];
}

/*
 * This function assumes the first two elements in each vertex is an x and y float
*/
void uiv_apply_mat4(MeshData* data, VertexLayout layout, mat4 matrix) {
    uint8_t* ptr = (uint8_t*)data->vertices;

    for (uint32_t i = 0; i < data->vertex_count; i++) {
        // This assumes that the vertex this is being run on
        //  has 2 floats at the start that define the vertex positions
        float* pos = (float*)ptr;
        
        // glm_mat4_mulv3 requires a vec3
        vec3 temp_v3 = { pos[0], pos[1], 1.0f };

        glm_mat4_mulv3(matrix, temp_v3, 1.0f, temp_v3);

        pos[0] = temp_v3[0];
        pos[1] = temp_v3[1];

        ptr += layout.stride;
    }
}


/*
 * ------ Framebuffer ------
*/

VertexLayout fbv_layout() {
    VertexLayout layout = {0};
    vertex_layout_add(&layout, 0, 2, GL_FLOAT, false); // POS
    vertex_layout_add(&layout, 1, 2, GL_FLOAT, false); // UV

    return layout;
}

MeshData fbv_gen_quad() {
    MeshData data;

    data.vertex_count = 4;
    data.index_count = 6;
    
    data.vertices = calloc(data.vertex_count, sizeof(FBOVertex));
    data.indices = calloc(data.index_count, sizeof(uint32_t));
    
    FBOVertex* v = (FBOVertex*)data.vertices;

    v[0] = (FBOVertex){{-1.0f, -1.0f}, {0.0f, 0.0f}};
    v[1] = (FBOVertex){{ 1.0f, -1.0f}, {1.0f, 0.0f}};
    v[2] = (FBOVertex){{ 1.0f,  1.0f}, {1.0f, 1.0f}};
    v[3] = (FBOVertex){{-1.0f,  1.0f}, {0.0f, 1.0f}};
    
    uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };
    memcpy(data.indices, indices, sizeof(indices));

    return data;
}
