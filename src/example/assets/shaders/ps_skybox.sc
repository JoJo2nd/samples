$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "ACES.sh"

//#define PI (3.14159265358979323846264338327)

uniform mat4 u_inverseProj;


vec2 ToSphericalCoords(float3 coords) {
    vec3 normalizedCoords = normalize(coords);
    float latitude = acos(-normalizedCoords.y);
    float longitude = atan2(normalizedCoords.z, normalizedCoords.x);
    vec2 sphereCoords = float2(longitude, latitude) * float2(0.5/PI, 1.0/PI);
    return float2(0.5,1.0) - sphereCoords;
}

// Do ACES tonemap
void main() {
    vec2 screen_pos = v_texcoord0.xy * .5 + .5;
    screen_pos.y = screen_pos.y-1;
    vec4 vs = mul(u_inverseProj, vec4(v_texcoord0.xy, 1, 1));
    vs /= vs.w;
    vec3 uv = normalize(vs.xyz);
    vec2 puv = ToSphericalCoords(uv);
    // base is in gamma space with no sRGB read so just raw copy
    //vec4 base = textureCube(s_environmentMap, uv);
    vec4 base = texture2D(s_base, puv);

    gl_FragColor.rgb = vec3(screen_pos.xy, 0);
    gl_FragColor.rgb = uv;
    gl_FragColor.rgb = base.rgb;
    gl_FragColor.w = 1.0;
}
