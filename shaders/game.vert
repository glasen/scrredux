#version 450
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_col;
layout(location = 0) out vec3 wpos;
layout(location = 1) out vec4 col;
layout(set = 1, binding = 0) uniform U { vec4 r0; vec4 r1; vec4 r2; vec4 r3; };   // world -> clip rows
layout(set = 1, binding = 1) uniform M { vec4 m0; vec4 m1; vec4 m2; };            // model -> world rows
void main() {
    vec4 p = vec4(in_pos, 1.0);
    vec4 w = vec4(dot(m0, p), dot(m1, p), dot(m2, p), 1.0);
    gl_Position = vec4(dot(r0, w), dot(r1, w), dot(r2, w), dot(r3, w));
    wpos = w.xyz;
    col = in_col;
}
