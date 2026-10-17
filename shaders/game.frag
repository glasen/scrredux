#version 450
layout(location = 0) in vec3 wpos;
layout(location = 1) in vec4 col;
layout(location = 0) out vec4 outc;
layout(set = 2, binding = 0) uniform sampler2DShadow shNear;
layout(set = 2, binding = 1) uniform sampler2DShadow shFar;
layout(set = 3, binding = 0) uniform F {
    vec4 campos;   // xyz
    vec4 fogcol;   // rgb
    vec4 prm;      // x = fog start, y = fog end
    vec4 light;    // xyz direction to the sun
    vec4 nm0, nm1, nm2, nm3;   // near cascade world -> light clip rows
    vec4 fm0, fm1, fm2, fm3;   // far cascade
    vec4 sp;       // x = near texel, y = far texel, z = near z bias, w = far z bias
    vec4 paint;    // rgb = body paint colour for alpha~0.5 vertices
};

float pcf(sampler2DShadow s, vec3 uvz, float tx) {
    float sum = 0.0;
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
            sum += texture(s, vec3(uvz.xy + vec2(float(i), float(j)) * tx, uvz.z));
    return sum / 9.0;
}

float sunlight(vec3 p, vec3 n) {
    vec4 pn = vec4(p + n * 0.08, 1.0);
    vec3 c = vec3(dot(nm0, pn), dot(nm1, pn), dot(nm2, pn));
    vec2 uv = vec2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
    if (uv.x > 0.03 && uv.x < 0.97 && uv.y > 0.03 && uv.y < 0.97 && c.z > 0.0 && c.z < 1.0)
        return pcf(shNear, vec3(uv, c.z - sp.z), sp.x);
    vec4 pf = vec4(p + n * 0.6, 1.0);
    c = vec3(dot(fm0, pf), dot(fm1, pf), dot(fm2, pf));
    uv = vec2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
    if (uv.x > 0.01 && uv.x < 0.99 && uv.y > 0.01 && uv.y < 0.99 && c.z > 0.0 && c.z < 1.0)
        return pcf(shFar, vec3(uv, c.z - sp.w), sp.y);
    return 1.0;
}

void main() {
    if (col.a < 0.25) { outc = vec4(col.rgb, 1.0); return; }  // unlit (sky, lamps, HUD)
    vec3 base = col.rgb;
    if (abs(col.a - 0.502) < 0.06) base = col.rgb * paint.rgb;  // body paint
    vec3 n = normalize(cross(dFdx(wpos), dFdy(wpos)));
    vec3 toc = campos.xyz - wpos;
    if (dot(n, toc) < 0.0) n = -n;
    float ndl = max(0.0, dot(n, light.xyz));
    float lit = ndl > 0.0 ? sunlight(wpos, n) : 1.0;
    float sky = 0.5 + 0.5 * n.y;                               // hemisphere fill from the sky
    float inten = min(0.46 + 0.14 * sky + 0.62 * ndl * lit, 1.12);
    inten = floor(inten * 7.0 + 0.5) / 7.0;                    // banded retro lighting
    float fog = clamp((length(toc) - prm.x) / (prm.y - prm.x), 0.0, 1.0);
    fog = floor(fog * 8.0) / 8.0;                              // banded fog
    outc = vec4(mix(base * inten, fogcol.rgb, fog), 1.0);
}
