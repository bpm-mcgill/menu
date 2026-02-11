#version 310 es
precision mediump float;

in vec2 TexCoords;
in vec4 Color;
out vec4 fragColor;

uniform sampler2D u_tex;

void main() {
    // Used GL_R8, so data in .r channel
    float mask = texture(u_tex, TexCoords).r;
    
    // Bitmap alpha masking
    fragColor = vec4(Color.rgb, Color.a * mask);
    
    if (fragColor.a < 0.05) discard;
}
