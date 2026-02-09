// VERTEX SHADER
//  For rendering icons
#version 310 es
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec4 aColor;

uniform mat4 u_projection;
uniform mat4 u_model;

out vec2 TexCoords;
out vec4 Color;

void main() {
    TexCoords = aTexCoords;
    Color = aColor;
    gl_Position = u_projection * u_model * vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
