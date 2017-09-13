$input v_pos, v_view, v_normal, v_texcoord0

#include "common/common.sh"

uniform vec4 u_texSize;
SAMPLER2D(s_albedo, 0);
SAMPLER2D(s_bump, 1);
//SAMPLER2D(s_spec, 0);

vec3 encodeNormal(vec3 n) {
    return (n * .5f) + .5f;
}

vec3 getBumpNormal(vec2 uv) {
    vec2 texel = vec2(1, 1)/u_texSize.xy;
    float x1 = texture2D(s_bump, uv+(texel*vec2(1, 0))).x;
    float x2 = texture2D(s_bump, uv+(texel*vec2(-1, 0))).x;
    float y1 = texture2D(s_bump, uv+(texel*vec2(0, 1))).x;
    float y2 = texture2D(s_bump, uv+(texel*vec2(0, -1))).x;
    return vec3(x2-x1, y2-y1, 2);
}

vec3 encodeDepth(float depth) {
    return vec3(f32tof16(depth), f32tof16(depth), f32tof16(depth));
}

float decodeDepth(vec3 idepth) {
    return f16tof32(idepth.x);
}

void main() {
    vec3 n = mul(u_model[0], vec4((v_normal).xyz, 0.0)).xyz;
    gl_FragData[0] = texture2D(s_albedo, v_texcoord0).rgba;
    gl_FragData[1] = vec4(encodeNormal(normalize(n)), gl_FragCoord.z);
}