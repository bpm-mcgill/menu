// FRAGMENT SHADER
//  Renders the XMB style waves
#version 310 es
precision mediump float;

out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;

const vec3 top = vec3(0.318, 0.831, 1.0);
const vec3 bottom = vec3(0.094, 0.141, 0.424);
const float widthFactor = 1.5;

vec3 calcSine(
    vec2 uv,
    float speed,
    float frequency,
    float amplitude,
    float shift,
    float offset,
    vec3 color,
    float width,
    float exponent,
    bool dir
) {
    float angle = u_time * speed * frequency * -1.0 + (shift + uv.x) * 2.0;

    float y = sin(angle) * amplitude + offset;
    float diffY = y - uv.y;
    float dsqr = abs(diffY);

    if (dir && diffY > 0.0) {
        dsqr *= 4.0;
    } else if (!dir && diffY < 0.0) {
        dsqr *= 4.0;
    }

    float scale = pow(
        smoothstep(width * widthFactor, 0.0, dsqr),
        exponent
    );

    return min(color * scale, color);
}

void main(){
    vec2 uv = gl_FragCoord.xy / u_resolution;

    vec3 color = mix(bottom, top, uv.y);

    color += calcSine(uv, 0.2, 0.20, 0.2, 0.0, 0.5, vec3(0.3), 0.1, 15.0, false);
    color += calcSine(uv, 0.4, 0.40, 0.15,0.0, 0.5, vec3(0.3), 0.1, 17.0, false);
    color += calcSine(uv, 0.3, 0.60, 0.15,0.0, 0.5, vec3(0.3), 0.05,23.0, false);

    color += calcSine(uv, 0.1, 0.26, 0.07,0.0, 0.3, vec3(0.3), 0.1, 17.0, true);
    color += calcSine(uv, 0.3, 0.36, 0.07,0.0, 0.3, vec3(0.3), 0.1, 17.0, true);
    color += calcSine(uv, 0.5, 0.46, 0.07,0.0, 0.3, vec3(0.3), 0.05,23.0, true);
    color += calcSine(uv, 0.2, 0.58, 0.05,0.0, 0.3, vec3(0.3), 0.2, 15.0, true);

    FragColor = vec4(color, 1.0);
}
