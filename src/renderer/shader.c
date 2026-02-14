#include "renderer/shader.h"
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/flex_array.h"
#include "utils/handles.h"
#include "shader_internal.h"

/*
 * Internal shader management
*/
DEFINE_FLEX_ARRAY(Shader, ShaderArray)
static ShaderArray* _shaders = NULL;

// Free shader list
static uint32_t* _free_list = NULL;
static uint32_t _free_count = 0;
static uint32_t _free_capacity = 0;

// Shader_get
DEFINE_RESOURCE_GETTER(Shader, ShaderHandle, _shaders, "SHADER_MGR")

/* ------- Helpers -------- */

static uint32_t get_available_slot() {
    // Check for inactive slots to reuse
    if (_free_count > 0) {
        _free_count--;
        return _free_list[_free_count];
    }
    
    // If no free slots, make a new one in the flex array
    Shader empty_shader = {0};
    _shaders = ShaderArray_push(_shaders, empty_shader);
    
    // Return the newly created index
    return _shaders->count - 1;
}

// Helper to fully read a file for shader compilation
static char* read_shader(const char* filepath) {
    FILE* fptr = fopen(filepath, "rb");
    if (fptr == NULL) {
        printf("Failed to open file\n");
        return NULL;
    }

    size_t size;
    if (fseek(fptr, 0, SEEK_END) == 0) {
        size = ftell(fptr);
    }
    else {
        printf("Couldn't seek to the end of shader file. %s\n", filepath);
        fclose(fptr);
        return NULL;
    }
    fseek(fptr, 0, SEEK_SET); // Return to start for reading

    char* buffer = malloc(size + 1);
    if (buffer == NULL) {
        printf("Failed to allocate memory for shader buffer\n");
        fclose(fptr);
        return NULL;
    }
 
    size_t bytes_read = fread(buffer, 1, size, fptr);
    if (bytes_read != size) {
        printf("Error reading file: %s\n", filepath);
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
    if (!success) {
        int loglen = 0;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &loglen);

        char* log = malloc(loglen);
        glGetShaderInfoLog(shader_id, loglen, NULL, log);

        fprintf(stderr, "Shader failed to compile: %s\n", log);
        free(log);
        return false;
    }
    return true;
}

// Check programiv for errors during shader linking
static bool verify_link(unsigned int program_id) {
    int success = 0;
    glGetProgramiv(program_id, GL_LINK_STATUS, &success);
    if (!success) {
        int loglen = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &loglen);

        char* log = malloc(loglen);
        glGetProgramInfoLog(program_id, loglen, NULL, log);

        fprintf(stderr, "Shader failed to link: %s\n", log);
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
        
        printf("Cached uniform: %s\n", shader->uniforms[i].name);
    }
}

// A return value of 0 means an error occurred
static unsigned int shader_program_build(const char* vertex_path, const char* fragment_path) {
    // Read the source files into memory
    char* vertsource = read_shader(vertex_path);
    char* fragsource = read_shader(fragment_path);
    if (vertsource == NULL || fragsource == NULL) {
        printf("Failed to read at least one of the shader source files.\n");
        free(vertsource);
        free(fragsource);
        return 0;
    }

    // Ensure the shader paths can fit within the shader struct's path arrays
    if (strlen(vertex_path) >= MAX_SHADER_PATH || strlen(fragment_path) >= MAX_SHADER_PATH) {
        fprintf(stderr, "Error: Shader paths are too long.\n");
        free(vertsource);
        free(fragsource);
        return 0;
    }
    
    unsigned int vertex, fragment;
    
    // Create and compile vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, (const char**)&vertsource, NULL);
    glCompileShader(vertex);
    if (!verify_compile(vertex)) {
        free(vertsource);
        free(fragsource);
        return 0;
    }
    
    // Create and compile fragment shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, (const char**)&fragsource, NULL);
    glCompileShader(fragment);
    if (!verify_compile(fragment)) {
        free(vertsource);
        free(fragsource);
        return 0;
    }
    
    // Create shader program
    unsigned int sid;
    sid = glCreateProgram(); // This returns 0 on fail
    
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
    free(vertsource);
    free(fragsource);
    
    // Return newly built program id
    return sid;
}

/* ------- Shader Implementation -------- */

void shaders_init() {
    if (_shaders == NULL) {
        _shaders = ShaderArray_create(16);
        // TODO: Load internal shaders here
    }
}

ShaderHandle shader_create(const char* vertex_path, const char* fragment_path) {
    unsigned int sid = shader_program_build(vertex_path, fragment_path);
    if (sid == 0) {
        printf("Failed to build shader program, returning fallback shader.\n");
        return ShaderHandle_null();
    }
    // If everything compiled and linked without errors, the shader is valid and is added to the
    // internal buffer
    
    // Create a new entry for the shader
    uint32_t index = get_available_slot();
    Shader* shader = &_shaders->data[index];

    // Store the generation and reset the memory of the slot in case slot was previously used
    uint32_t prev_gen = shader->generation; 
    memset(shader, 0, sizeof(Shader));
    
    // Populate the new shader entry with data
    shader->active = true;
    shader->generation = prev_gen;
    shader->id = sid;
    // Null terminate da shit at the very end in case the path is EXACTLY MAX_SHADER_PATH
    strncpy(shader->vert_path, vertex_path, MAX_SHADER_PATH);
    shader->vert_path[MAX_SHADER_PATH-1] = '\0';
    strncpy(shader->frag_path, fragment_path, MAX_SHADER_PATH);
    shader->frag_path[MAX_SHADER_PATH-1] = '\0';
    
    // Load the uniform locations into memory
    cache_uniforms(shader);

    
    // Generate a handle for the new shader
    return ShaderHandle_pack(index, shader->generation);
}

void shader_use(ShaderHandle handle) {
    Shader* shader = Shader_get(handle);
    glUseProgram(shader->id);
}

void shader_delete(ShaderHandle handle) {
    Shader* shader = Shader_get(handle);
    // index 0 is the fallback, which shouldn't be deleted
    if (shader == &_shaders->data[0] || !shader->active) return;

    if (shader->id != 0) {
        glDeleteProgram(shader->id);
        shader->id = 0;
    }
    
    // Clear unifom array
    shader->uniform_count = 0;
    memset(shader->uniforms, 0, sizeof(Uniform) * MAX_UNIFORMS);

    shader->active = false;
    shader->generation++;

    // Add this index to the free list
    if (_free_count >= _free_capacity) {
        _free_capacity = _free_capacity == 0 ? 16 : _free_capacity * 2;
        _free_list = realloc(_free_list, _free_capacity * sizeof(uint32_t));
    }
    _free_list[_free_count++] = ShaderHandle_index(handle);
}

void shader_reload(ShaderHandle handle) {
    Shader* shader = Shader_get(handle);

    // Uses the same paths that are already stored
    unsigned int new_id = shader_program_build(shader->vert_path, shader->frag_path);

    if (new_id != 0) {
        glDeleteProgram(shader->id);
        shader->id = new_id;
    }
    else {
        printf("Shader reload failed. Keeping previous version.");
    }
}

// A memory safe wrapper for the internal get_uniform_location
// Uses handles instead of Shader*
int shader_get_uniform_location(ShaderHandle handle, const char* name) {
    Shader* shader = Shader_get(handle);
    return get_uniform_location(shader, name);
}

