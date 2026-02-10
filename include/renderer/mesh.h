#ifndef MESH_H
#define MESH_H

#include "renderer/shader.h"
#include "renderer/texture.h"
#include <GLES3/gl3.h>
#include <cglm/cglm.h>
#include <cglm/types.h>
#include <stdbool.h>
#include <stdint.h>


/* ---------- UNIFORMS ---------- */


typedef enum {
    UNI_FLOAT,
    UNI_VEC2,
    UNI_VEC3,
    UNI_VEC4,
    UNI_MAT4,
    UNI_INT,
    //UNI_SAMPLER // Textures
} UniformType;

typedef struct {
    char name[32];
    UniformType type;
    uint32_t offset;
    int32_t location;
} UniformEntry;

typedef struct {
    uint8_t* data;
    UniformEntry* entries;
    uint32_t count;
    uint32_t capacity;
    uint32_t data_size;
} UniformStore;

void uniform_store_add(UniformStore* store, const char* name, UniformType type, void* value);
void uniform_store_apply(UniformStore* store, Shader* shader);


/* ---------- VERTEX ATTRIBUTES ---------- */


#define MAX_VERTEX_ATTRIBS 8

// Defines a single vertex attribute
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
} VertexLayout;

void vertex_layout_add(VertexLayout* layout, uint32_t index, int32_t size, uint32_t type, bool normalized);


/* ---------- MESH ---------- */

// Stores mesh data that will get pushed to a MeshObj
// OR 
// Stores pointers to data fetched by handle from a MeshObj
typedef struct {
    void* vertices;
    uint32_t* indices;
    uint32_t vertex_count;
    uint32_t index_count;
} MeshData;

MeshData gen_quad();
void md_apply_region(MeshData* data, TextureRegion* region);
void md_apply_mat4(MeshData* data, VertexLayout layout, mat4 matrix);

typedef int32_t MeshHandle; // Unique Mesh Registry Entry ID

typedef struct {
    // Byte offset in vertex buffer (needs to be byte offset since vertex stride is unknown)
    uint32_t v_offset;
    // Index of the start of the indices in index array
    uint32_t i_start;

    uint32_t v_count;  // Number of vertices the entry contains
    uint32_t i_count;  // Number of indices the entry contains
    bool active;       // Is slot in use?
} MeshRegistryEntry;

typedef struct {
    bool is_dirty; // Has there been an update to the data that hasn't been sent to the GPU?
    uint32_t min_offset; // Start offset of the data that needs to be updated on GPU
    uint32_t max_offset; // End offset of the data that needs to be updated on GPU
} DirtyRange;

// A CPU-side Mesh. This contains all the information required to build a Mesh.
// Has batching capabilities while storing data in contiguous arrays to prevent cache misses.
typedef struct {
    // Data buffers
    void* vertices;
    uint32_t* indices;

    MeshRegistryEntry* registry;
    uint32_t registry_count;
    uint32_t registry_capacity;
    uint32_t* free_list;  // Array of available handles. Disactivated handles are recycled
    uint32_t free_list_count;

    uint32_t vertex_count;
    uint32_t vertex_capacity;
    uint32_t index_count;
    uint32_t index_capacity;
 
    DirtyRange v_dirty, i_dirty;
    
    VertexLayout layout; // Defines the data format/layout of vertices for OpenGL
    UniformStore uniforms;

    // Defines if this MeshObj has dynamic data that needs to be updated frequently
    uint32_t usage; // GL_STATIC_DRAW || GL_DYNAMIC_DRAW
} MeshObj;

MeshObj* mesh_obj_create(uint32_t v_count, uint32_t i_count, VertexLayout layout, bool dynamic);
MeshHandle mesh_obj_push(MeshObj* obj, MeshData data);
MeshData mesh_obj_get_data(MeshObj* obj, MeshHandle handle);
void mesh_obj_transform(MeshObj* obj, MeshHandle handle, mat4 matrix);
MeshObj* mesh_obj_create_quad();
void mesh_obj_destroy(MeshObj* obj);

// A GPU-side Mesh. The previous MeshObj data is now stored on
//  the GPU, and this struct contains the bindings to that data.
typedef struct {
    uint32_t vbo;
    uint32_t vao;
    uint32_t ebo;
    uint32_t index_count;
    uint32_t vbo_capacity, ebo_capacity;
    
    VertexLayout layout; // Still stored for debugging
    UniformStore uniforms;
    
    Shader* shader;
    Texture* texture;
} Mesh;

// Builds a MeshObj into a Mesh
Mesh mesh_build(MeshObj* obj, Shader* shader);
void mesh_bind(Mesh* mesh);
void* mesh_get_uniform(Mesh* mesh, const char* name);
void mesh_update_gpu(Mesh* mesh, MeshObj* obj);
void mesh_destroy(Mesh* mesh);

#endif // !MESH_H
