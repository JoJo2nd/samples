$input v_pos, v_wpos, v_view, v_normal, v_tangent, v_bitangent, v_texcoord0

#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "ACES.sh"

uniform vec4 u_gameTime;
uniform vec4 u_bucketParams; // z bin bucket size, light grid size, light grid pitch
uniform vec4 u_sunlight;

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

float light_distance_attenuation(float light_radius_inverse, float dist_to_light, float light_falloff_power) {
    float attFactorLinear  = saturate(dist_to_light * light_radius_inverse);
    //attFactorLinear = pow(abs( 1 - attFactorLinear * attFactorLinear ), light_falloff_power);
    // linear attenuation
    float attFactor = 1 - attFactorLinear;
    // squared attenuation
    //float attFactor = 1 - (attFactorLinear * attFactorLinear);
    return attFactor;
}

/*
    Returns the normal from the normal map in view space. 
    Assumining that norm, tang & bitang have been transformed into view space.
*/
vec3 readNormalMap(vec3 v_norm, vec3 v_tang, vec3 v_bitang, vec2 uv) {
    mat3 fromTangentSpace = mat3(
                normalize(vec3(v_tang.x, v_bitang.x, v_norm.x)),
                normalize(vec3(v_tang.y, v_bitang.y, v_norm.y)),
                normalize(vec3(v_tang.z, v_bitang.z, v_norm.z))
                );

    vec3 normal;
    normal.xy = texture2D(s_normal, uv).xy * 2.0 - 1.0;
    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy) );
    return mul(fromTangentSpace, normal);
}

void main() {
    uint2 grid_info = light_grid_data(gl_FragCoord.xy);
    uint2 z_bin = read_z_bin(v_view.z);

    vec2 uv = vec2(v_texcoord0.x, 1-v_texcoord0.y);

    vec4 albedo = texture2D(s_base, uv);
    vec3 normal = readNormalMap(v_normal, v_tangent, v_bitangent, uv);
    vec4 metal = texture2D(s_metallic, uv);
    vec4 rough = texture2D(s_roughness, uv);



    {
        vec3 lightDir = -normalize(u_sunlight.xyz);
        vec3 ndotl = saturate(dot(lightDir, normalize(normal)));     
        gl_FragColor.xyz = ndotl.xxx * albedo * u_sunlight.w;
        //gl_FragColor.xyz = encodeNormalUint(normal);
    }

    //vec3 force_use = normal * metal * rough * normal * 0.00001;
    // use grid assignments
    uint startOffset = light_grid_offset(gl_FragCoord.xy)*MAX_LIGHT_COUNT;
    for (uint light_idx = 0, light_n = MAX_LIGHT_COUNT; light_idx < light_n; ++light_idx) {
      uint real_light_idx = lightGridFat[startOffset + light_idx];
      if (real_light_idx == 0xffff) break;
#if !DEBUG_NO_BINS
        if (real_light_idx >= z_bin.x && real_light_idx < z_bin.y) {
#endif
            real_light_idx *= 2;
            vec3 light_pos = lightData[real_light_idx].xyz;
            vec3 light_r = lightData[real_light_idx].w;
            vec3 light_c = lightData[real_light_idx+1].rgb;
            vec3 pos_to_light = light_pos - v_view;

            float attenuation = light_distance_attenuation(1/light_r, length(pos_to_light), LIGHT_FALLOFF);
            vec3 lightDir = normalize(pos_to_light);
            vec3 toEye = -normalize(v_view);
            vec3 half = normalize(toEye + lightDir);
            vec3 ndotl = saturate(dot(lightDir, normalize(normal)));            
#if DEBUG_LIGHT_EVAL
            gl_FragColor.r += 1;       
#else
            gl_FragColor.xyz += (ndotl * attenuation) * light_c * albedo.rgb;
#endif
#if !DEBUG_NO_BINS
        }
#endif
    }

#if DEBUG_LIGHT_EVAL
    gl_FragColor.r /= 20;
#endif

    gl_FragColor.w = 1.0;

    //gl_FragColor.x = ((z_bin.y - z_bin.x)/10.0);
}
