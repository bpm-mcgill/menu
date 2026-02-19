#ifndef MESH_H
#define MESH_H

#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex.h"
#include "utils/flex_array.h"
#include "utils/handles.h"
#include <GLES3/gl3.h>
#include <cglm/cglm.h>
#include <cglm/types.h>
#include <stdbool.h>
#include <stdint.h>


/* -------- UNIFORMS -------- */

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

void uniform_store_init(UniformStore* store, uint32_t initial_capacity);
void uniform_store_free(UniformStore* store);
void uniform_store_add(UniformStore* store, const char* name, UniformType type, void* value);
void uniform_store_apply(UniformStore* store, Shader* shader);
void* uniform_store_get(UniformStore* store, const char* name);

/* -------- MATERIAL -------- */

typedef struct {
    ShaderHandle shader;
    TextureHandle texture;
    UniformStore uniforms;

    bool active;
    uint32_t generation;
} Material;

DECLARE_HANDLE(MaterialHandle)

DEFINE_FLEX_ARRAY(Material, MaterialArray)
DECLARE_RESOURCE_POOL(Material, MaterialArray)
DEFINE_RESOURCE_GETTER(Material, MaterialHandle, _Material_pool, "MATERIAL")

MaterialHandle material_create(ShaderHandle shader, TextureHandle texture);
void material_set_uniform(MaterialHandle mat, const char* name, UniformType type, void* value);
void material_apply(MaterialHandle mat);

/* ---------- MESH ---------- */

typedef int32_t SubMeshID; // Unique Mesh Registry Entry ID

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
// TODO: Migrate the vertex and index buffers to using flex arrays
// Might have to use uint8_t instead of void* for pointer arithmetic
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
    // Defines if this MeshObj has dynamic data that needs to be updated frequently
    uint32_t usage; // GL_STATIC_DRAW || GL_DYNAMIC_DRAW
} MeshObj;

MeshObj* mesh_obj_create(VertexLayout layout, bool dynamic);
void mesh_obj_destroy(MeshObj* obj);

SubMeshID mesh_obj_push(MeshObj* obj, MeshData data);
MeshData mesh_obj_get_data(MeshObj* obj, SubMeshID handle);
void mesh_obj_transform(MeshObj* obj, SubMeshID handle, mat4 matrix);

/* -------- MESH RESOURCE -------- */

// A GPU-side Mesh. The previous MeshObj data is now stored on
//  the GPU, and this struct contains the bindings to that data.
typedef struct {
    uint32_t vbo;
    uint32_t vao;
    uint32_t ebo;
    uint32_t index_count;
    uint32_t vbo_capacity, ebo_capacity;
    
    VertexLayout layout; // Still stored for debugging
    MaterialHandle default_material;
    
    bool active;
    uint32_t generation;
} Mesh;

DECLARE_HANDLE(MeshHandle)

DEFINE_FLEX_ARRAY(Mesh, MeshArray)
DECLARE_RESOURCE_POOL(Mesh, MeshArray)
DEFINE_RESOURCE_GETTER(Mesh, MeshHandle, _Mesh_pool, "MESH")

void meshes_init(void);
void meshes_free(void);

// Builds a MeshObj into a Mesh
MeshHandle mesh_create_from_obj(MeshObj* obj);

// Updates an existing Mesh with new data from MeshObj (Uses dirty ranges)
void mesh_update_from_obj(MeshHandle handle, MeshObj* obj);

void mesh_bind(MeshHandle handle);
void mesh_delete(MeshHandle handle);

#endif // !MESH_H
