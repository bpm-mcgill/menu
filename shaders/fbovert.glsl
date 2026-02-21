// VERTEX SHADER
//  For rendering directly to an FBO
#version 310 es
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec4 aColor;

out vec2 v_uv;
out vec4 v_color;

void main() {
    v_uv = aTexCoords;
    v_color = aColor;
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
