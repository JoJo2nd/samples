$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "ACES.sh"

uniform mat4 u_inverseProj;

vec2 cartesian_to_polar(vec3 v) {
  // Assumes an r of 1
  return vec2(acos(v.z)/(3.1415*2), atan2(v.y, v.x)/(3.1415*2));
}

// Do ACES tonemap
void main() {
    vec2 screen_pos = v_texcoord0.xy * .5 + .5;
    screen_pos.y = screen_pos.y-1;
    vec4 vs = mul(u_inverseProj, vec4(v_texcoord0.xy, 1, 1));
    vs /= vs.w;
    vec3 uv = normalize(vs.xyz);
    vec2 puv = cartesian_to_polar(uv);
    // base is in gamma space with no sRGB read so just raw copy
    //vec4 base = textureCube(s_environmentMap, uv);
    vec4 base = texture2D(s_base, puv);

    gl_FragColor.rgb = vec3(screen_pos.xy, 0);
    gl_FragColor.rgb = uv;
    gl_FragColor.rgb = base.rgb;
    gl_FragColor.w = 1.0;
}
