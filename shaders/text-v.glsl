// VERTEX SHADER
//  For bitmap and MSDL text rendering
#version 310 es
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec4 aColor;

uniform mat4 u_projection;
uniform mat4 u_model;

out vec2 v_texcoords;
out vec4 v_color;

void main() {
    v_texcoords = aTexCoords;
    v_color = aColor;
    gl_Position = u_projection * u_model * vec4(aPos.x, aPos.y, 1.0, 1.0);
}
