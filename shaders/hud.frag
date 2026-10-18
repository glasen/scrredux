#version 450
layout(location = 0) in vec3 wpos;
layout(location = 1) in vec4 col;
layout(location = 0) out vec4 outc;
void main() { outc = col; }   // HUD: flat colour, alpha blended
