#version 310 es
precision mediump float;

in vec2 v_texcoords;
in vec4 v_color;
out vec4 FragColor;

uniform sampler2D u_tex;      // The MSDF Atlas
uniform float u_pxrange;     // The 'pxrange' value from the font bin header
uniform float u_softness;

// Median function to find the true distance field value from RGB channels
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    // 1. Sample the MSDF texture
    vec3 msd = texture(u_tex, v_texcoords).rgb;
    
    // 2. Extract the signed distance value
    float sd = median(msd.r, msd.g, msd.b);
    
    // This converts the distance from atlas-space to screen-pixel space.
    float screen_px_distance = u_pxrange * (sd - 0.5);

    float edge_width = 0.5 + u_softness;
    
    // 4. Calculate Opacity
    float opacity = smoothstep(-edge_width, edge_width, screen_px_distance);
    
    // 5. Final color output
    float final_alpha = v_color.a * opacity;
    vec3 final_rgb = v_color.rgb * final_alpha; // Premultiply rgb by alpha
    FragColor = vec4(final_rgb, final_alpha);
    
    // Discard pixels that are basically transparent
    if (FragColor.a < 0.0001) discard;
}
