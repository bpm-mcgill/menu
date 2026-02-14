#include "shader_internal.h"
#include <string.h>

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
