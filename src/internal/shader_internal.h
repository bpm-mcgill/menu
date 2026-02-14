#ifndef SHADER_INTERNAL_H
#define SHADER_INTERNAL_H

/*
 * Private internal shader
 *
 * This file should only be included by renderer files
 * 
 * Anything defined in here shouldn't be used outside of the renderer for safety
*/

#include <stdbool.h>
#include <stdint.h>
#include "renderer/shader.h"

#define MAX_UNIFORMS 16
#define MAX_SHADER_PATH 256

struct Uniform {
    char name[32]; // GLSL uniform name
    unsigned int location; // Cached uniform location
};

struct Shader {
    unsigned int id;
    
    // Paths stored for hot-reloading
    char vert_path[MAX_SHADER_PATH];
    char frag_path[MAX_SHADER_PATH];

    Uniform uniforms[MAX_UNIFORMS]; // 16 = max uniforms per shader
    int uniform_count;

    // Resource management fields
    bool active;
    uint32_t generation;
};

// Internal getter
Shader* Shader_get(ShaderHandle handle);

int get_uniform_location(Shader* shader, const char* name);

// These take Shader* and not ShaderHandle. For renderer use only
void set_uniform_1f(Shader* shader, const char* name, float f);
void set_uniform_1i(Shader* shader, const char* name, int i);
void set_uniform_vec2f(Shader* shader, const char* name, vec2 f);
void set_uniform_mat4(Shader* shader, const char* name, mat4 m);

#endif // !SHADER_INTERNAL_H
