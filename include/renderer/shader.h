#ifndef SHADER_H
#define SHADER_H

#include <GLES3/gl3.h>
#include <cglm/cglm.h>
#include <cglm/types.h>
#include <stdbool.h>

#define MAX_UNIFORMS 16
#define MAX_SHADER_PATH 256

typedef struct {
    char name[32]; // GLSL uniform name
    unsigned int location; // Cached uniform location
} Uniform;

typedef struct {
    unsigned int id;
    
    // Paths stored for hot-reloading
    char vert_path[MAX_SHADER_PATH];
    char frag_path[MAX_SHADER_PATH];

    Uniform uniforms[MAX_UNIFORMS]; // 16 = max uniforms per shader
    int uniform_count;
} Shader;

bool shader_create(Shader* shader, const char* vertex_path, const char* fragment_path);
void shader_use(Shader* shader);
void shader_destroy(Shader* shader);
bool shader_reload(Shader* shader);

int get_uniform_location(Shader* shader, const char* name); // -1 means not found

void set_uniform_1f(Shader* shader, const char* name, float f);
void set_uniform_1i(Shader* shader, const char* name, int i);
void set_uniform_vec2f(Shader* shader, const char* name, vec2 f);
void set_uniform_mat4(Shader* shader, const char* name, mat4 m);

#endif // !SHADER_H
