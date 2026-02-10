// FRAGMENT SHADER
//  For rendering directly to an FBO (NO POST-PROCESSING)
#version 310 es
precision mediump float;
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D u_texture;

void main() {
    vec4 color = texture(u_texture, TexCoords);
    FragColor = color;
}
