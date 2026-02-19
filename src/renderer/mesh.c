#include "renderer/mesh.h"
#include "engine.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "utils/handles.h"
#include <GLES3/gl3.h>
#include <cglm/io.h>
#include <cglm/mat4.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---------- RESOURCE POOLS ---------- */

DEFINE_RESOURCE_POOL(Material, MaterialHandle, MaterialArray);
DEFINE_RESOURCE_POOL(Mesh, MeshHandle, MeshArray);

/* ---------- UNIFORMS ---------- */

/*
 * Initializes an empty UniformStore
 * 
 * initial_capacity defines how many UniformEntries can fit in the store's entries
 *  before the UniformStore entries needs to be reallocated.
*/
void uniform_store_init(UniformStore* store, uint32_t initial_capacity) {
    store->count = 0;
    store->capacity = initial_capacity;
    store->data_size = 0;
    store->entries = NULL;
    store->data = NULL;
    if (initial_capacity > 0) {
        store->entries = calloc(initial_capacity, sizeof(UniformEntry));
        ENGINE_ASSERT(store->entries != NULL, "Failed to allocated UniformStore entries.");
    }
}

void uniform_store_free(UniformStore *store) {
    if (store->entries) free(store->entries);
    if (store->data) free(store->data);
    store->entries = NULL;
    store->data = NULL;
    store->count = 0;
    store->capacity = 0;
}

/*
 * Creates a new UniformEntry and adds it to the UniformStore's entries
 *
 * If the UniformStore entry capacity has been reached, the capacity is doubled
 *  and the entry array is reallocated to fit the new capacity.
 *
 * Function is exited early if the entry realloc fails
 *
 * The uniform data block is also reallocated to fit new data passed in `value`
 * If an error occurs on second realloc, the data isn't added to the uniform store
*/
void uniform_store_add(UniformStore* store, const char* name, UniformType type, void* value) {
    // Ensure if another UniformEntry can be stored
    if (store->count >= store->capacity) {
        store->capacity = (store->capacity == 0) ? 4 : store->capacity * 2;
        void* temp = realloc(store->entries, sizeof(UniformEntry) * store->capacity);
        ENGINE_ASSERT(temp != NULL, "Error reallocating uniform store entries.");
        store->entries = temp;
    }

    // Get the size of the new type to be added to the uniform store
    uint32_t size = 0;
    switch (type) {
        case UNI_FLOAT: size = sizeof(float); break;
        case UNI_VEC2: size = sizeof(float) * 2; break;
        case UNI_VEC3: size = sizeof(float) * 3; break;
        case UNI_VEC4: size = sizeof(float) * 4; break;
        case UNI_MAT4: size = sizeof(float) * 16; break;
        case UNI_INT: size = sizeof(int); break;
    }

    // Create the uniform entry
    UniformEntry* entry = &store->entries[store->count];
    strncpy(entry->name, name, 31); // 32 is max uniform name length. -1 for null termination
    entry->name[31] = '\0'; // Add null termination char
    entry->type = type;
    entry->offset = store->data_size;
    entry->location = -1; // Set to -1 until shader is bound

    // Reallocate to fit new data and copy data into new allocation
    void* temp = realloc(store->data, store->data_size + size);
    ENGINE_ASSERT(temp != NULL, "Error reallocating uniform data.");
    store->data = temp;

    memcpy(store->data + entry->offset, value, size);

    store->data_size += size;
    store->count++;
}

/* 
 * Sends all uniform entries in a uniform store to the GPU
 * Caches all uniform locations from shader uniform cache, if nto cached already
 * Supports any type defined in the UniformType enum
*/
void uniform_store_apply(UniformStore* store, Shader* shader) {
    for (unsigned int i = 0; i < store->count; i++) {
        UniformEntry* e = &store->entries[i];

        if (e->location == -1) {
            e->location = get_uniform_location(shader, e->name);
        }

        if (e->location == -1) continue; // Uniform not found, skipping

        void* val = store->data + e->offset;

        switch (e->type) {
            case UNI_FLOAT: glUniform1fv(e->location, 1, (float*)val); break;
            case UNI_VEC2:  glUniform2fv(e->location, 1, (float*)val); break;
            case UNI_VEC3:  glUniform3fv(e->location, 1, (float*)val); break;
            case UNI_VEC4:  glUniform4fv(e->location, 1, (float*)val); break;
            case UNI_MAT4:  glUniformMatrix4fv(e->location, 1, GL_FALSE, (float*)val); break;
            case UNI_INT:   glUniform1iv(e->location, 1, (int*)val); break;
        }
    }
}

/*
 * Fetches the pointer to the memory where a uniform's data is stored.
 * Accessed by uniform name
*/
void* uniform_store_get(UniformStore* store, const char* name) {
    if (!store || !name) return NULL;

    for (uint32_t i = 0; i < store->count; i++) {
        if (strcmp(store->entries[i].name, name) == 0) {
            return (void*)(store->data + store->entries[i].offset);
        }
    }
    LOG_WARN("Unable to find uniform '%s'", name);
    return NULL;
}

/* ---------- MATERIAL ---------- */

MaterialHandle material_create(ShaderHandle shader, TextureHandle texture) {
    MaterialHandle handle = Material_alloc();
    Material* mat = Material_get(handle);

    mat->shader = shader;
    mat->texture = texture;
    uniform_store_init(&mat->uniforms, 4);
    
    return handle;
}

void material_set_uniform(MaterialHandle handle, const char* name, UniformType type, void* value) {
    Material* mat = Material_get(handle);
    if (!mat) return;
    uniform_store_add(&mat->uniforms, name, type, value);
}

void material_apply(MaterialHandle handle) {
    Material* mat = Material_get(handle);
    if (!mat) return;

    // Bind Shader
    shader_use(mat->shader);
    Shader* s = Shader_get(mat->shader);

    // Bind Texture
    texture_bind(mat->texture, 0); // Default to slot 0

    // Apply uniforms
    uniform_store_apply(&mat->uniforms, s);
}

static void material_unload_internal(Material* mat) {
    uniform_store_free(&mat->uniforms);
}

/* ---------- MESHOBJ ----------- */

/*
 * Takes a data range:
 *  - offset: Byte offset into the buffer to the start of the dirty data
 *  - size: The byte size/width of the dirty data
 *
 *  If there is already a dirty range, the range will be expanded to
 *   encompass all of the dirty data.
*/
void range_mark_dirty(DirtyRange* dirty,  uint32_t offset, uint32_t size) {
    if (!dirty->is_dirty) {
        dirty->min_offset = offset;
        dirty->max_offset = offset + size;
        dirty->is_dirty = true;
    }
    else {
        // Expand the range to encompass all dirty sections
        if (offset < dirty->min_offset) dirty->min_offset = offset;
        if (offset + size > dirty->max_offset) dirty->max_offset = offset + size;
    }
}

/*
 * Resets a dirty range.
 * Used when the data is synced on the GPU
 *  and the data is no longer dirty
*/
void dirty_reset(DirtyRange* dirty) {
    dirty->is_dirty = false;
    dirty->min_offset = 0;
    dirty->max_offset = 0;
}

/*
 * Creates a heap allocated empty MeshObj
 * VertexLayout needs to be defined and passed to the MeshObj
 * Returns the pointer to the created MeshObj on success.
*/
MeshObj* mesh_obj_create(VertexLayout layout, bool dynamic) {
    MeshObj* obj = calloc(1, sizeof(MeshObj));
    ENGINE_ASSERT(obj != NULL, "Out of memory allocating MeshObj.");
    obj->layout = layout;
    obj->usage = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW; 
    return obj;
}

/*
 * Frees all of the memory allocated to a MeshObj
 * Useful once the MeshObj is built into a Mesh, and the MeshObj is no longer required
*/
void mesh_obj_destroy(MeshObj *obj) {
    if (!obj) return;
    free(obj->vertices);
    free(obj->indices);
    free(obj->registry);
    free(obj->free_list);
    free(obj);
}

/*

 * Applies matrix transformations to an object in a MeshObj
 * Identifies the object by MeshHandle, retrieves the data via mesh_obj_get_data,
 *  then runs md_apply_mat4 to the resulting MeshData.
 * Marks the updated data as dirty to be reuploaded to the GPU
// TODO:
// This probably needs to be changed to a mark_handle_dirty function or something similar
// The transformations will be done on the MeshData, and then this function can be called
// on the handle to mark that MeshData as dirty

void mesh_obj_transform(MeshObj* obj, MeshHandle handle, mat4 matrix) {
    MeshData view = mesh_obj_get_data(obj, handle);
    if (view.vertices == NULL) return;

    md_apply_mat4(&view, obj->layout, matrix);

    MeshRegistryEntry* entry = &obj->registry[handle];
    range_mark_dirty(&obj->v_dirty, entry->v_offset, entry->v_count * obj->layout.stride);
}
*/

/*
 * Helper for mesh_obj_push
 *
 * Makes sure the vertex and index buffers are big enough to fit new data
 * MeshObj keeps track of the current buffer capacity, and if the new data
 *  exceeds that capacity, the buffer is reallocated 
 *     - capacity = (capacity + new_elements) * 2
 * Handles both the vertex and index buffers
*/
void mesh_obj_ensure_capacity(MeshObj* obj, uint32_t add_v, uint32_t add_i) {
    // Vertex Buffer
    if (obj->vertex_count + add_v > obj->vertex_capacity) {
        obj->vertex_capacity = (obj->vertex_capacity + add_v) * 2;
        void* temp = realloc(obj->vertices, obj->vertex_capacity * obj->layout.stride);
        ENGINE_ASSERT(temp != NULL, "Vertex buffer realloc failed.");
        obj->vertices = temp;
    }

    // Index Buffer
    if (obj->index_count + add_i > obj->index_capacity) {
        obj->index_capacity = (obj->index_capacity + add_i) * 2;
        void* temp = realloc(obj->indices, obj->index_capacity * sizeof(uint32_t));
        ENGINE_ASSERT(temp != NULL, "Index buffer realloc failed.");
        obj->indices = temp;
    }
}

/*
 * Takes a MeshObj and MeshData and adds the MeshData to the MeshObj
 *
 * This will resize the vertex and index buffers to fit the new data,
 * retrieve a new free registry handle,
 * populate the registry entry with the proper offsets and counts,
 * copy the vertex and index data from MeshData into MeshObj buffers,
 * and then mark the new data range as dirty to be uploaded to the GPU
*/
SubMeshID mesh_obj_push(MeshObj* obj, MeshData data) {
    // 1. Realloc vertices and indices to fit new data
    mesh_obj_ensure_capacity(obj, data.vertex_count, data.index_count);
    
    // 2. Get an available registry slot, or append a new one if none are free
    SubMeshID id;
    if (obj->free_list_count > 0) {
        return obj->free_list[--obj->free_list_count];
    }
    else {
        if (obj->registry_count >= obj->registry_capacity) {
            uint32_t new_cap = (obj->registry_capacity == 0) ? 16 : obj->registry_capacity * 2;
            obj->registry = realloc(obj->registry, new_cap * sizeof(MeshRegistryEntry));
            memset(obj->registry + obj->registry_capacity, 0, (new_cap - obj->registry_capacity) * sizeof(MeshRegistryEntry));
            obj->registry_capacity = new_cap;
        }
        id = (SubMeshID)obj->registry_count++;
    }

    // 3. Config new registry slot with the proper information
    MeshRegistryEntry* entry = &obj->registry[id];
    entry->v_offset = obj->vertex_count * obj->layout.stride;
    entry->v_count = data.vertex_count;
    entry->i_start = obj->index_count;
    entry->i_count = data.index_count;
    entry->active = true;
    
    // 4. Copy data into the main vertex buffer
    void* v_dest = (uint8_t*)obj->vertices + entry->v_offset;
    memcpy(v_dest, data.vertices, data.vertex_count * obj->layout.stride);
    
    // 5. Copy data into the main index buffer
    uint32_t* i_dest = obj->indices + obj->index_count; // Typed pointer, so pointer arithmetic is easy
    uint32_t i_offset_start = obj->vertex_count; // The first 
    for (uint32_t i = 0; i < data.index_count; i++) {
        // Insert the MeshData indices in the free space in the index buffer
        i_dest[i] = data.indices[i] + i_offset_start;
    }

    // Update totals
    obj->vertex_count += data.vertex_count;
    obj->index_count += data.index_count;

    // Mark new data as dirty so it will be updated in the GPU
    range_mark_dirty(&obj->v_dirty, entry->v_offset, data.vertex_count * obj->layout.stride);
    range_mark_dirty(
        &obj->i_dirty, 
        entry->i_start * sizeof(uint32_t),
        data.index_count * sizeof(uint32_t)
    );

    return id;
}

/*
 * Removes an object from a MeshObj from MeshHandle
 * 
 * Deletes the data by taking all of the data following the object being removed
 *  and moving it over the data to remove.
 * Every registry entry after the object removed needs to be updated, as their
 *  positions in the data buffers changed after the memmove, meaning their offsets
 *  into the data are different.
 * Their new offsets are equal to their previous offset - the size of the data that was removed
*/
void mesh_obj_remove(MeshObj* obj, SubMeshID id) {
    if (id < 0 || !obj->registry[id].active) return;

    MeshRegistryEntry* to_remove = &obj->registry[id];

    uint32_t v_remove_bytes = to_remove->v_count * obj->layout.stride;
    uint32_t i_remove_count = to_remove->i_count;
    
    // Pointer to the starting memory address of the object being removed
    uint8_t* v_gap_start = (uint8_t*)obj->vertices + to_remove->v_offset;
    // Pointer to the ending memory address of the object being removed
    uint8_t* v_next_block = v_gap_start + v_remove_bytes;
    // Total bytes from end of object memory block to the end of the used buffer
    size_t v_bytes_to_move = ((uint8_t*)obj->vertices + (obj->vertex_count * obj->layout.stride)) - v_next_block;

    // Overwrites the data that needs to be removed by moving the data
    //  after it into it's place, keeping the buffer packed and deleting it
    if (v_bytes_to_move > 0) {
        memmove(v_gap_start, v_next_block, v_bytes_to_move);
    }

    // 2. Shift indices
    uint32_t* i_gap_start = obj->indices + to_remove->i_start;
    uint32_t* i_next_block = i_gap_start + i_remove_count;
    size_t i_elements_to_move = (obj->indices + obj->index_count) - i_next_block;

    if (i_elements_to_move > 0) {
        memmove(i_gap_start, i_next_block, i_elements_to_move * sizeof(uint32_t));
    }

    // 3. Patch the registry, and fix indices
    // Update the v_offset and i_start of every object that lived after the removed data
    // (the data that was shifted)
    for (uint32_t i = 0; i < obj->registry_count; i++) {
        if(!obj->registry[i].active) continue; // skip deactivated entries
        
        // If the object was located further down in the buffer, slide it's metadata
        if (obj->registry[i].v_offset > to_remove->v_offset) {
            obj->registry[i].v_offset -= v_remove_bytes;
            obj->registry[i].i_start -= i_remove_count;

            // Since vertices were removed in front of this object, and this object's 
            //  vertices were moved down, the indices need to be updated for the new vertex locations
            uint32_t* moved_indices = obj->indices + obj->registry[i].i_start;
            uint32_t v_shift_count = to_remove->v_count;

            for (uint32_t j = 0; j < obj->registry[i].i_count; j++) {
                moved_indices[j] -= v_shift_count;
            }
        }
    }
    
    // 4. Update counters and registry
    obj->vertex_count -= to_remove->v_count;
    obj->index_count -= i_remove_count;
    to_remove->active = false;
    obj->free_list[obj->free_list_count++] = id;

    // 5. Mark the data that was changed as dirty
    range_mark_dirty(
        &obj->v_dirty, 
        to_remove->v_offset, 
        obj->vertex_count * obj->layout.stride - to_remove->v_offset
    );
    range_mark_dirty(
        &obj->i_dirty, 
        to_remove->i_start * sizeof(uint32_t),
        (obj->index_count - to_remove->i_start) * sizeof(uint32_t)
    );
}

/*
 * Finds an object in a MeshObj and coverts it into a MeshData
 *
 * Pointers to the object's data are fetched by adding the registry entry's offset
 *  to the MeshObj buffer pointer.
 * These offset pointer are then stored in a MeshData and the MeshData is returned
*/
MeshData mesh_obj_get_data(MeshObj* obj, SubMeshID id) {
    if (id < 0 || id >= (int)obj->registry_count || !obj->registry[id].active) {
        printf("MeshObj get data call failed. Object not valid\n");
        return (MeshData){.vertices = NULL, .indices = NULL, .vertex_count = 0, .index_count = 0};
    }

    MeshRegistryEntry* entry = &obj->registry[id];
    MeshData view;

    // Retrieve pointers to the objects data in the buffers
    view.vertices = (uint8_t*)obj->vertices + entry->v_offset;
    view.indices = obj->indices + entry->i_start;

    view.vertex_count = entry->v_count;
    view.index_count = entry->i_count;

    return view;
}

/* ---------- MESH ---------- */

/*
 * Will initialize the Mesh and Material resource pools
*/
void meshes_init(void) {
    if (_Mesh_pool.items == NULL) {
        _Mesh_pool.items = MeshArray_create(16);
        _Mesh_pool.free_list = IndexArray_create(16);

        // TODO: Fallback mesh (quad)
        // Placeholder used for now
        Mesh fallback = { .active = true, .generation = 0 };
        _Mesh_pool.items = MeshArray_push(_Mesh_pool.items, fallback);

        LOG_INFO("Mesh Resource: Initialized (Capacity: %u, Pool: %p)",
                _Mesh_pool.items->capacity, (void*)&_Mesh_pool);
    }
    else {
        LOG_WARN("Attempted to reinitialize mesh resource. Ignoring.");
    }
    
    if (_Material_pool.items == NULL) {
        _Material_pool.items = MaterialArray_create(16);
        _Material_pool.free_list = IndexArray_create(16);
        
        Material fallback = { .active = true, .generation = 0 };
        _Material_pool.items = MaterialArray_push(_Material_pool.items, fallback);

        LOG_INFO("Material Resource: Initialized (Capacity: %u, Pool: %p)",
                _Mesh_pool.items->capacity, (void*)&_Mesh_pool);
    }
    else {
        LOG_WARN("Attempted to reinitialize material resource. Ignoring.");
    }
}

/*
 * Takes a MeshObj, and uploads the MeshObj data to the GPU
 * Creates a Mesh, which holds the OpenGL bindings to the data on the GPU
 *
 * Clears the UniformStore of the MeshObj passed in, 
 *  so that a subsequent mesh_obj_destroy doesn't free the live data in the store
*/
MeshHandle mesh_create_from_obj(MeshObj* obj) {
    MeshHandle handle = Mesh_alloc();
    Mesh* mesh = Mesh_get(handle);

    mesh->layout = obj->layout;
    mesh->index_count = obj->index_count;

    // 1. Generate handles
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
    
    // 2. Bind VAO to record VBO and EBO state
    glBindVertexArray(mesh->vao);

    // 3. Upload Vertex Data
    mesh->vbo_capacity = obj->vertex_count * obj->layout.stride;
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 mesh->vbo_capacity,
                 obj->vertices,
                 obj->usage);

    // 4. Upload Index Data
    mesh->ebo_capacity = obj->index_count * sizeof(uint32_t);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh->ebo_capacity,
                 obj->indices,
                 obj->usage);

    // 5. Set up Vertex Attributes
    vertex_layout_apply(&mesh->layout);

    // 6. Unbind
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return handle;
}

/*
 * - Takes a Mesh + MeshObj pair, and updates the data on the GPU (Mesh) with data from CPU (MeshObj)
 * - Only uploads the section of memory on the CPU that actually changed (dirty range)
 * - If the upload is going to be bigger than the GPU buffer, the entire CPU
 *   buffer is reuploaded with the new buffer size.
 * - After an upload, the dirty ranges are cleared, as the data is synced
 * - Handles VBO and EBO buffers (vertex and indices)
*/
void mesh_update_from_obj(MeshHandle handle, MeshObj* obj) { 
    Mesh* mesh = Mesh_get(handle);
    if (!mesh) return;

    // Update vertex buffer
    if (obj->v_dirty.is_dirty) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);

        uint32_t cpu_cap_bytes = obj->vertex_capacity * obj->layout.stride;
        
        // If the cpu buffer is larger than gpu buffer, resize gpu buffer to match
        if (cpu_cap_bytes > mesh->vbo_capacity) {
            glBufferData(GL_ARRAY_BUFFER, cpu_cap_bytes, obj->vertices, obj->usage);
            mesh->vbo_capacity = cpu_cap_bytes;
        } else {
            uint32_t update_size = obj->v_dirty.max_offset - obj->v_dirty.min_offset;

            // Pointer to the memory address of the start of the data that needs to be updated
            void* data_start = (uint8_t*)obj->vertices + obj->v_dirty.min_offset;
            glBufferSubData(GL_ARRAY_BUFFER, obj->v_dirty.min_offset, update_size, data_start);
        }
        dirty_reset(&obj->v_dirty);
    }
    
    // Update index buffer
    if (obj->i_dirty.is_dirty) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);

        uint32_t cpu_cap_bytes = obj->index_capacity * sizeof(uint32_t);
        
        // If the cpu buffer is larger than gpu buffer, resize gpu buffer to match
        if (cpu_cap_bytes > mesh->ebo_capacity) {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, cpu_cap_bytes, obj->indices, obj->usage);
            mesh->ebo_capacity = cpu_cap_bytes;
        } else {
            uint32_t update_size = obj->i_dirty.max_offset - obj->i_dirty.min_offset;

            // Pointer to the memory address of the start of the data that needs to be updated
            void* data_start = (uint8_t*)obj->indices + obj->i_dirty.min_offset;
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, obj->i_dirty.min_offset, update_size, data_start);
        }
        dirty_reset(&obj->i_dirty);
    }

    mesh->index_count = obj->index_count;
}

void mesh_bind(MeshHandle handle) {
    Mesh* mesh = Mesh_get(handle);
    if (!mesh || !mesh->active) return;

    glBindVertexArray(mesh->vao);
}

static void mesh_unload_internal(Mesh* mesh) {
    if (mesh->vao) glDeleteVertexArrays(1, &mesh->vao);
    if (mesh->vbo) glDeleteBuffers(1, &mesh->vbo);
    if (mesh->ebo) glDeleteBuffers(1, &mesh->ebo);
    mesh->vao = mesh->vbo = mesh->ebo = 0;
}

void mesh_delete(MeshHandle handle) {
    Mesh* mesh = Mesh_get(handle);
    if (!mesh || !mesh->active) return;
    mesh_unload_internal(mesh);
    Mesh_free(handle);
}

void meshes_free(void) {
    Mesh_pool_shutdown(mesh_unload_internal);

    Material_pool_shutdown(material_unload_internal);
}
