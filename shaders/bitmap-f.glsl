#version 310 es
precision mediump float;

in vec2 v_texcoords;
in vec4 v_color;
out vec4 fragColor;

uniform sampler2D u_tex;

void main() {
    // Used GL_R8, so data in .r channel
    float mask = texture(u_tex, v_texcoords).r;
    
    // Bitmap alpha masking
    fragColor = vec4(v_color.rgb, v_color.a * mask);
    
    if (fragColor.a < 0.05) discard;
}
