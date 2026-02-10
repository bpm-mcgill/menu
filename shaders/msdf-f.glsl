#version 310 es
precision mediump float;

in vec2 TexCoords;
in vec4 Color;
out vec4 fragColor;

uniform sampler2D u_tex;      // The MSDF Atlas
uniform float u_pxRange;     // The 'pxrange' value from the font bin header

// Median function to find the true distance field value from RGB channels
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    // 1. Sample the MSDF texture
    vec3 msd = texture(u_tex, TexCoords).rgb;
    
    // 2. Extract the signed distance value
    float sd = median(msd.r, msd.g, msd.b);
    
    // This converts the distance from atlas-space to screen-pixel space.
    float screenPxDistance = u_pxRange * (sd - 0.5);

    float edge_width = 0.5 + 0.1;
    
    // 4. Calculate Opacity
    // clamp(distance + 0.5) gives us a perfect 1-pixel wide anti-aliased edge.
    float opacity = smoothstep(-edge_width, edge_width, screenPxDistance);
    
    // 5. Final color output
    float finalAlpha = Color.a * opacity;
    vec3 finalRGB = Color.rgb * finalAlpha; // Premultiply rgb by alpha
    fragColor = vec4(finalRGB, finalAlpha);
    
    // Performance optimization: discard pixels that are basically transparent
    if (fragColor.a < 0.00001) discard;
}
