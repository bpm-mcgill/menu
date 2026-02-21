#version 310 es
precision mediump float;

in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_tex0;
uniform vec2 u_resolution;
uniform vec2 u_direction; // (1.0, 0.0) for horizontal, (0.0, 1.0) for vertical
uniform float u_intensity;

void main() {
    vec2 texel_size = 1.0 / u_resolution;
    
    // 9-Tap Gaussian weights for a wide, smooth falloff
    float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

    vec4 result = texture(u_tex0, v_uv) * weight[0];

    for(int i = 1; i < 5; ++i) {
        vec2 offset = u_direction * texel_size * float(i);
        result += texture(u_tex0, v_uv + offset) * weight[i];
        result += texture(u_tex0, v_uv - offset) * weight[i];
    }

    float multiplier = (u_direction.y > 0.5) ? u_intensity : 1.0;
    FragColor = vec4(result.rgb * multiplier, result.a * multiplier);
}
