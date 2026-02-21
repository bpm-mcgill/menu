#version 310 es
precision mediump float;

in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_tex0;
uniform float u_threshold;
uniform vec2 u_resolution;

void main() {
    vec2 texel_size = 1.0 / u_resolution;
    vec4 max_color = vec4(0.0);
    
    float radius = 2.0;
    
    for(float x = -radius; x <= radius; x++) {
        for(float y = -radius; y <= radius; y++) {
            vec2 offset = vec2(x, y) * texel_size;
            vec4 sample_color = texture(u_tex0, v_uv + offset);
            
            if(sample_color.a > max_color.a) {
                max_color = sample_color;
            }
        }
    }
    
    float brightness = dot(max_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    
    if (brightness > u_threshold) {
        FragColor = max_color;
    } else {
        FragColor = vec4(0.0);
    }
}
