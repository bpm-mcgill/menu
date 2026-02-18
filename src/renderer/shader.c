#include "renderer/shader.h"
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "utils/handles.h"

/*
 * Internal shader management
*/
DEFINE_RESOURCE_POOL(Shader, ShaderHandle, ShaderArray)

/* ------- Helpers -------- */

// Helper to fully read a file for shader compilation
static char* read_shader(const char* filepath) {
    ENGINE_ASSERT(filepath != NULL, "Attempted to read shader with NULL filepath");

    FILE* fptr = fopen(filepath, "rb");
    if (fptr == NULL) {
        LOG_ERROR("Failed to open shader file: %s", filepath);
        return NULL;
    }

    size_t size;
    if (fseek(fptr, 0, SEEK_END) == 0) {
        size = ftell(fptr);
    }
    else {
        LOG_ERROR("Couldn't seek to the end of shader file. %s", filepath);
        fclose(fptr);
        return NULL;
    }
    fseek(fptr, 0, SEEK_SET); // Return to start for reading

    char* buffer = malloc(size + 1);
    ENGINE_ASSERT(buffer != NULL, "Out of memory allocating shader file buffer");
 
    size_t bytes_read = fread(buffer, 1, size, fptr);
    if (bytes_read != size) {
        LOG_ERROR("Error reading file: %s", filepath);
        free(buffer);
        fclose(fptr);
        return NULL;
    }

    fclose(fptr);
    buffer[size] = '\0'; // Add null termination character

    return buffer;
}

// Checks shaderiv for errors during shader compilation
static bool verify_compile(unsigned int shader_id) {
    int success = 0;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if (UNLIKELY(!success)) {
        int loglen = 0;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &loglen);

        char* log = malloc(loglen);
        glGetShaderInfoLog(shader_id, loglen, NULL, log);

        LOG_ERROR("Shader failed to compile: %s", log);
        free(log);
        return false;
    }
    return true;
}

// Check programiv for errors during shader linking
static bool verify_link(unsigned int program_id) {
    int success = 0;
    glGetProgramiv(program_id, GL_LINK_STATUS, &success);
    if (UNLIKELY(!success)) {
        int loglen = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &loglen);

        char* log = malloc(loglen);
        glGetProgramInfoLog(program_id, loglen, NULL, log);

        LOG_ERROR("Shader failed to link: %s", log);
        free(log);
        return false;
    }
    return true;
}

static void cache_uniforms(Shader* shader) {
    int count;
    glGetProgramiv(shader->id, GL_ACTIVE_UNIFORMS, &count);

    shader->uniform_count = (count > MAX_UNIFORMS) ? MAX_UNIFORMS : count;
    
    for (int i = 0; i < shader->uniform_count; i++) {
        int size;
        GLenum type;
        
        glGetActiveUniform(shader->id, (unsigned int)i, 32, NULL, &size, &type, shader->uniforms[i].name);
        shader->uniforms[i].location = glGetUniformLocation(shader->id, shader->uniforms[i].name);
    }
}

// A return value of 0 means an error occurred
static unsigned int shader_program_build(const char* vertex_path, const char* fragment_path) {
    ENGINE_ASSERT(vertex_path != NULL, "Attempted to read vertex shader with NULL filepath");
    ENGINE_ASSERT(fragment_path != NULL, "Attempted to read fragment shader with NULL filepath");
    
    // Ensure the shader paths can fit within the shader struct's path arrays
    if (strlen(vertex_path) >= MAX_SHADER_PATH || strlen(fragment_path) >= MAX_SHADER_PATH) {
        LOG_ERROR("Shader paths exceed MAX_SHADER_PATH limit.");
        return 0;
    }

    // Read the source files into memory
    char* vertsource = read_shader(vertex_path);
    char* fragsource = read_shader(fragment_path);
    if (!vertsource || !fragsource) {
        LOG_ERROR("Failed to read at least one of the shader source files.");
        free(vertsource);
        free(fragsource);
        return 0;
    }
    
    // Create and compile vertex shader
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, (const char**)&vertsource, NULL);
    glCompileShader(vertex);
    if (!verify_compile(vertex)) {
        goto compile_cleanup;
    }
    
    // Create and compile fragment shader
    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, (const char**)&fragsource, NULL);
    glCompileShader(fragment);
    if (!verify_compile(fragment)) {
        glDeleteShader(vertex);
        goto compile_cleanup;
    }
    
    // Create shader program
    unsigned int sid = glCreateProgram(); // This returns 0 on fail
    
    // Attach and link vert and frag shader
    glAttachShader(sid, vertex);
    glAttachShader(sid, fragment);
    glLinkProgram(sid);

    if (!verify_link(sid)) {
        glDeleteProgram(sid);
        sid = 0;
    }
    
    // Cleanup
    glDeleteShader(vertex);
    glDeleteShader(fragment);

compile_cleanup:
    free(vertsource);
    free(fragsource);
    return sid;
}

static void shader_unload_internal(Shader* shader) {
    if (shader->id != 0) {
        glDeleteProgram(shader->id);
        shader->id = 0;
    }

    // Clean the memory to prevent ghosting in debug builds
    #ifdef DEBUG_BUILD
        memset(shader->uniforms, 0, sizeof(Uniform) * MAX_UNIFORMS); 
    #endif

    // Clear unifom array
    shader->uniform_count = 0;
}

/* -------- Public API -------- */

void shaders_init(void) {
    if (_Shader_pool.items == NULL) {
        _Shader_pool.items = ShaderArray_create(16);
        _Shader_pool.free_list = IndexArray_create(16);
        
        // TODO: Load a magenta fallback shader
        Shader fallback = { .active = true, .generation = 0, .id = 0 };
        _Shader_pool.items = ShaderArray_push(_Shader_pool.items, fallback);
        
        LOG_INFO("Shader Resource: Initialized (Capacity: %u, Pool: %p)",
                _Shader_pool.items->capacity, (void*)&_Shader_pool);
    }
    else {
        LOG_WARN("Attempted to reinitialize shaders resource. Ignoring.");
    }
}

ShaderHandle shader_create(const char* vertex_path, const char* fragment_path) {
    unsigned int sid = shader_program_build(vertex_path, fragment_path);
    if (sid == 0) {
        LOG_WARN("Failed to build shader program '%s', returning fallback.", vertex_path);
        return ShaderHandle_null(); // Returns index 0 safely
    }
    
    // Create a new entry for the shader
    ShaderHandle handle = Shader_alloc();
    Shader* shader = Shader_get(handle);
    
    // Populate the new shader entry with data
    shader->id = sid;
    // Null terminate da shit at the very end in case the path is EXACTLY MAX_SHADER_PATH
    strncpy(shader->vert_path, vertex_path, MAX_SHADER_PATH);
    shader->vert_path[MAX_SHADER_PATH-1] = '\0';
    strncpy(shader->frag_path, fragment_path, MAX_SHADER_PATH);
    shader->frag_path[MAX_SHADER_PATH-1] = '\0';
    
    // Load the uniform locations into memory
    cache_uniforms(shader);
    
    // Generate a handle for the new shader
    return handle;
}

void shader_use(ShaderHandle handle) {
    Shader* shader = Shader_get(handle);
    glUseProgram(shader->id);
}

void shader_delete(ShaderHandle handle) {
    Shader* shader = Shader_get(handle);
    // index 0 is the fallback, which shouldn't be deleted
    if (shader == &_Shader_pool.items->data[0] || !shader->active) return;
    
    shader_unload_internal(shader);

    Shader_free(handle);
}

void shader_reload(ShaderHandle handle) {
    Shader* shader = Shader_get(handle);
    if (shader == &_Shader_pool.items->data[0] || !shader->active) return; // Don't reload fallback
    
    LOG_INFO("Hot-reloading shader: %s", shader->vert_path);
    unsigned int new_id = shader_program_build(shader->vert_path, shader->frag_path);

    if (new_id != 0) {
        glDeleteProgram(shader->id);
        shader->id = new_id;
        cache_uniforms(shader); // Re-cache new uniform locations
    }
    else {
        LOG_WARN("Shader reload failed. Keeping previous version.");
    }
}

void shaders_free(void) {
    Shader_pool_shutdown(shader_unload_internal);
}

/* -------- Development API -------- */

// A memory safe wrapper for the internal get_uniform_location
// Uses handles instead of Shader*
int shader_get_uniform_location(ShaderHandle handle, const char* name) {
    Shader* shader = Shader_get(handle);
    return get_uniform_location(shader, name);
}

// PERF:
// Uniform locations are stored in a linear array
//  This means that the time complexity is O(n)
//  Hashmaps would allow for O(1), but take longer to lookup from compared to small linear array searching
//  If shaders begin to have more than 10 or so uniforms, linear arrays will no longer be faster
int get_uniform_location(Shader* shader, const char* name) {
    for (int i = 0; i < shader->uniform_count; i++) {
        if (strcmp(shader->uniforms[i].name, name) == 0) {
            return shader->uniforms[i].location;
        }
    }
    return -1;
}

void set_uniform_1f(Shader* shader, const char* name, float f) {
    glUniform1f(get_uniform_location(shader, name), f);
}

void set_uniform_1i(Shader* shader, const char* name, int i) {
    glUniform1i(get_uniform_location(shader, name), i);
}

void set_uniform_vec2f(Shader* shader, const char* name, vec2 f) {
    glUniform2fv(get_uniform_location(shader, name), 1, (float*)f);
}

void set_uniform_mat4(Shader* shader, const char* name, mat4 m) {
    glUniformMatrix4fv(get_uniform_location(shader, name), 1, GL_FALSE, (float*)m);
}
