#version 310 es
precision mediump float;
out vec4 FragColor;

in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_tex;

void main() {
    vec4 texcolor = texture(u_tex, v_uv);
    float final_alpha = texcolor.a * v_color.a;
    vec3 final_rgb = (texcolor.rgb + v_color.rgb) * final_alpha;
    FragColor = vec4(final_rgb, final_alpha);
}
