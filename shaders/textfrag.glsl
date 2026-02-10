#version 310 es
precision mediump float;

in vec2 TexCoords;
in vec4 Color;
out vec4 fragColor;

uniform sampler2D u_tex;

void main() {
    // We used GL_R8, so look in the .r channel
    float mask = texture(u_tex, TexCoords).r;
    
    // Standard Bitmap alpha masking
    // (If you switch to true SDF, replace 'mask' with smoothstep logic)
    fragColor = vec4(Color.rgb, Color.a * mask);
    //fragColor = vec4(1.0f,1.0f,1.0f,1.0f);
    
    if (fragColor.a < 0.05) discard;
}
