#version 330 core
layout(origin_upper_left) in vec4 gl_FragCoord;

uniform vec2 gridSize;
uniform vec2 screenSize;
uniform samplerBuffer gridSampler;

out vec4 color;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
    ivec2 cellCoord = ivec2(gl_FragCoord.xy / screenSize * gridSize);
    vec4 cell = texelFetch(gridSampler, int(cellCoord.x * gridSize.x) + cellCoord.y);
    
    float freq = log2(cell.y) / (log2(22000.0) - log2(20.0));
    float amp = cell.x;
    
    vec3 hsv = hsv2rgb(vec3(freq, 0.75, 1.0));
    color = vec4(hsv, amp);
}