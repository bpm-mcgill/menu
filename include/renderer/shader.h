#ifndef SHADER_H
#define SHADER_H

#include <GLES3/gl3.h>
#include <cglm/cglm.h>
#include <cglm/types.h>
#include <stdbool.h>
#include <stdint.h>
#include "utils/handles.h"

#define MAX_UNIFORMS 16
#define MAX_SHADER_PATH 256

// INFO: Defined in shader_internal.h
typedef struct Uniform Uniform;
typedef struct Shader Shader;

// ShaderHandle
DECLARE_HANDLE(ShaderHandle);

// Allocates the internal flex array and loads internal shaders
void shaders_init();

ShaderHandle shader_create(const char* vertex_path, const char* fragment_path);
void shader_use(ShaderHandle handle);
void shader_delete(ShaderHandle handle);
void shader_reload(ShaderHandle handle);

/* Gets the cached location of a uniform */
int shader_get_uniform_loc(ShaderHandle handle, const char* name); // -1 means not found

#endif // !SHADER_H
