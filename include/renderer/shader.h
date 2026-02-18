#ifndef SHADER_H
#define SHADER_H

#include <GLES3/gl3.h>
#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>
#include "utils/flex_array.h"
#include "utils/handles.h"
#include "engine.h"

#define MAX_UNIFORMS 16
#define MAX_SHADER_PATH 256

typedef struct {
    char name[32]; // GLSL uniform name
    int location; // Cached uniform location
} Uniform;

typedef struct {
    unsigned int id;
    
    // Paths stored for hot-reloading
    char vert_path[MAX_SHADER_PATH];
    char frag_path[MAX_SHADER_PATH];

    Uniform uniforms[MAX_UNIFORMS]; // 16 = max uniforms per shader
    int uniform_count;

    // Resource management fields
    bool active;
    uint32_t generation;
} Shader;

DECLARE_HANDLE(ShaderHandle);

// --- RESOURCE POOL ---
// Declared for the resource getter (static inline)
DEFINE_FLEX_ARRAY(Shader, ShaderArray)
DECLARE_RESOURCE_POOL(Shader, ShaderArray)
DEFINE_RESOURCE_GETTER(Shader, ShaderHandle, _Shader_pool, "SHADER")

// --- PUBLIC API ---

void shaders_init(void); // Allocates the internal flex array and loads internal shaders
void shaders_free(void);

ShaderHandle shader_create(const char* vertex_path, const char* fragment_path);
void shader_use(ShaderHandle handle);
void shader_delete(ShaderHandle handle);
void shader_reload(ShaderHandle handle);

/* Gets the cached location of a uniform */
int shader_get_uniform_loc(ShaderHandle handle, const char* name); // -1 means not found

// --- DEV API ---
/* This is all debugging/development shit. It will likely be removed later */
int get_uniform_location(Shader* shader, const char* name);
void set_uniform_1f(Shader* shader, const char* name, float f);
void set_uniform_1i(Shader* shader, const char* name, int i);
void set_uniform_vec2f(Shader* shader, const char* name, vec2 f);
void set_uniform_mat4(Shader* shader, const char* name, mat4 m);

#endif // !SHADER_H
