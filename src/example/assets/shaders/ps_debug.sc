$input v_texcoord0

#include "../../../common/common.sh"
#include "../../common_defines.h"

uniform vec4 u_gameTime;
uniform vec4 u_bucketParams; // z bin bucket size, light grid size, light grid pitch
 

BUFFER_RO(zBin,         uvec2, 10); // start:uint16; end:uint16;
BUFFER_RO(lightGrid,    uvec2, 11); // start:uint16; count:uint16;
BUFFER_RO(lightList,    uint, 12);  // lightIndex:uint16;
BUFFER_RO(lightData,    vec4, 13);  // lightPos:float[3], radius:float, +1 color
BUFFER_RO(lightGridFat, uint, 14); // start:uint16

uint light_grid_offset(vec2 screen_pos) {
    uint gridPos = (uint)((floor((u_bucketParams.w-screen_pos.y) / u_bucketParams.y)) * u_bucketParams.z) + (uint)(screen_pos.x / u_bucketParams.y);
    return gridPos;
}

uint2 light_grid_data(vec2 screen_pos) {
    uint gridPos = (uint)((floor((u_bucketParams.w-screen_pos.y) / u_bucketParams.y)) * u_bucketParams.z) + (uint)(screen_pos.x / u_bucketParams.y);
    return lightGrid[gridPos];
}

uint2 read_z_bin(float vs_depth) {
    return zBin[(uint)(vs_depth/u_bucketParams.x)];
}

float fresnel_schlick(vec3 col, vec3 eye, vec3 half) {
    float exp = pow(1 - dot(eye, half), 5.0);
    return saturate(col + (1 - col) * exp).x;
}

vec3 encode_vec(vec3 pos) {
    return (pos + 1) * 0.5;
}

float light_distance_attenuation(float light_radius_inverse, float dist_to_light, float light_falloff_power)
{
    float attFactorLinear  = saturate(dist_to_light * light_radius_inverse);
    //attFactorLinear = pow(abs( 1 - attFactorLinear * attFactorLinear ), light_falloff_power);
    // linear attenuation
    float attFactor = 1 - attFactorLinear;
    // squared attenuation
    //float attFactor = 1 - (attFactorLinear * attFactorLinear);
    return attFactor;
}

#if CS_LIGHT_CULL
void main() {
    gl_FragColor = vec4(1, 0, 0, 1);

    uint2 grid_info = light_grid_data(gl_FragCoord.xy);

    gl_FragColor.xyz = vec3(0, 0, 0);
    // use grid assignments
    uint light_count = 0;
    uint startOffset = light_grid_offset(gl_FragCoord.xy)*MAX_LIGHT_COUNT;
    for (uint light_idx = 0, light_n = MAX_LIGHT_COUNT; light_idx < light_n; ++light_idx, ++light_count) {
      uint real_light_idx = lightGridFat[startOffset + light_idx];
      if (real_light_idx == 0xffff) break;
    }

    gl_FragColor.r = light_count/100.0;
    gl_FragColor.a = 1.0;

}
#endif

#if !CS_LIGHT_CULL
void main() {
    gl_FragColor = vec4(1, 0, 0, 1);

    uint2 grid_info = light_grid_data(gl_FragCoord.xy);

    gl_FragColor.xyz = vec3(0, 0, 0);
    // use grid assignments
    for (uint light_idx = grid_info.x, light_n = grid_info.x+grid_info.y; light_idx < light_n; ++light_idx) {
        uint real_light_idx = lightList[light_idx];
        gl_FragColor.r += 1;       
    }

    gl_FragColor.r /= 100;
    gl_FragColor.w = 1.0;

}
#endif
