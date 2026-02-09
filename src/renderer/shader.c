#include "renderer/shader.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include <string.h>

// Helper to fully read a file for shader compilation
char* read_shader(const char* filepath) {
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

bool verify_compile(unsigned int shader_id) {
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

void cache_uniforms(Shader* shader) {
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

bool shader_create(Shader* shader, const char* vertex_path, const char* fragment_path) {
    // Clear the shader in case of old data
    //  This will also clear the paths, if they already exist, so shader->vertex_path
    //  cannot be passed to the function, because it will be wiped before it gets stored
    // memset(shader, 0, sizeof(Shader));

    // Read the source files into memory
    char* vertsource = read_shader(vertex_path);
    char* fragsource = read_shader(fragment_path);
    if (vertsource == NULL || fragsource == NULL) {
        printf("Failed to read at least one of the shader source files.\n");
        free(vertsource);
        free(fragsource);
        return false;
    }

    // Ensure the shader paths can fit within the shader struct's path arrays
    if (strlen(vertex_path) >= MAX_SHADER_PATH || strlen(fragment_path) >= MAX_SHADER_PATH) {
        fprintf(stderr, "Error: Shader paths are too long.\n");
        free(vertsource);
        free(fragsource);
        return false;
    }
    
    // Copy the shader paths into in the shader struct
    //   If there happens to already be a path stored in the struct,
    //   there could be leftover data that isn't overwritted, but
    //   a null termination character is added at the end of the new path
    //   so it *shouldn't* be a problem
    strncpy(shader->vert_path, vertex_path, MAX_SHADER_PATH);
    shader->vert_path[MAX_SHADER_PATH-1] = '\0';
    strncpy(shader->frag_path, fragment_path, MAX_SHADER_PATH);
    shader->frag_path[MAX_SHADER_PATH-1] = '\0';

    unsigned int vertex, fragment;
    
    // Create and compile vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, (const char**)&vertsource, NULL);
    glCompileShader(vertex);
    if (!verify_compile(vertex)) {
        free(vertsource);
        free(fragsource);
        return false;
    }
    
    // Create and compile fragment shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, (const char**)&fragsource, NULL);
    glCompileShader(fragment);
    if (!verify_compile(fragment)) {
        free(vertsource);
        free(fragsource);
        return false;
    }
    
    // Create shader program
    unsigned int sid;
    sid = glCreateProgram();
    shader->id = sid;
    
    // Attach and link vert and frag shader
    glAttachShader(sid, vertex);
    glAttachShader(sid, fragment);
    glLinkProgram(sid);

    // Load the uniform locations into memory
    cache_uniforms(shader);

    // Cleanup
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    free(vertsource);
    free(fragsource);

    return true; // Success
}

void shader_use(Shader* shader) {
    glUseProgram(shader->id);
}

void shader_destroy(Shader* shader) {
    if (shader->id != 0) {
        glDeleteProgram(shader->id);
        shader->id = 0;
    }
    shader->uniform_count = 0;
    
    // Clear unifom array
    memset(shader->uniforms, 0, sizeof(Uniform) * MAX_UNIFORMS);
}

bool shader_reload(Shader* shader) {
    if (shader->vert_path[0] == '\0' || shader->frag_path[0] == '\0') {
        fprintf(stderr, "Error: No paths stored in shader to reload.\n");
        return false;
    }

    shader_destroy(shader);
    
    return shader_create(shader, shader->vert_path, shader->frag_path);
}

// INFO:
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
