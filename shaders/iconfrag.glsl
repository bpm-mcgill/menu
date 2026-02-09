#version 310 es
precision mediump float;
out vec4 FragColor;

in vec2 TexCoords;
in vec4 Color;

uniform sampler2D u_tex;

void main() {
    vec4 texcolor = texture(u_tex, TexCoords);
    FragColor = texcolor;
}
