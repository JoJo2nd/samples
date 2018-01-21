$input v_pos, v_wpos, v_view, v_normal, v_tangent, v_bitangent, v_texcoord0

#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "ACES.sh"

#define DEBUG_MODES (1)

uniform vec4 u_gameTime;
uniform vec4 u_bucketParams; // z bin bucket size, light grid size, light grid pitch
uniform vec4 u_sunlight;
uniform vec4 u_ambientColour;
uniform vec4 u_debugMode;
uniform vec4 u_dynamicLights;
uniform vec4 u_gridParams;
uniform vec4 u_gridOffset; // 
#if NO_TEXTURES
uniform vec4 u_albedo; 
uniform vec4 u_metalrough; // x: metal y: rough
#endif

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

vec3 fresnel_schlick_roughness(vec3 spec_col, vec3 eye, vec3 norm, float gloss) {
    return spec_col + (max(gloss, spec_col) - spec_col) + pow(1-saturate(dot(eye, norm)), 5);
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
    Uses the GGX / Trowbridge-Reitz
*/
float specular_D(float roughness, float NdotH) {
    float a2 = pow(roughness, 4);
    float d = (NdotH * a2 - NdotH) * NdotH + 1;
    return a2 / (PI*d*d);
}

/*
*/
float specular_VisImplicit() {
    // basically no vis term
    return 0.25f;
}

float specular_VisSchlick(float roughness, float NdotV, float NdotL) {
    float k = (roughness*roughness) * .5f;
    float v = NdotV * (1 - k) + k;
    float l = NdotL * (1 - k) + k;
    return 0.25 / (v * l);
}

float specular_VisSmith(float roughness, float NdotV, float NdotL) {
    float a2 = pow(roughness, 4);
    float v = NdotV + sqrt(NdotV * (NdotV - NdotV * a2) + a2);
    float l = NdotL + sqrt(NdotL * (NdotL - NdotL * a2) + a2);
    return 1.0/(v*l);
}

//#define specular_Vis(roughness, NdotV, NdotL) specular_VisImplicit()
//#define specular_Vis(roughness, NdotV, NdotL) specular_VisSchlick(roughness, NdotV, NdotL)
#define specular_Vis(roughness, NdotV, NdotL) specular_VisSmith(roughness, NdotV, NdotL)

/*
    Classic Schlick
    Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
*/
vec3 specular_F(vec3 spec_colour, float VdotH) {
    float fc = pow(1 - VdotH, 5);
    return saturate (50.0 * spec_colour.g) * fc + (1 - fc) * spec_colour;
}

/*

*/
vec3 calculateSpecular(vec3 norm, vec3 half, vec3 view, vec3 light, float roughness, vec3 spec_colour) {
    float d = specular_D(roughness, saturate(dot(norm, half)));
    float v = specular_Vis(roughness, saturate(dot(norm, view)), saturate(dot(norm, light)));
    vec3 f = specular_F(spec_colour, saturate(dot(view, half)));
    vec3 spec = (d * v) * f;
    if (u_debugMode.x != 0.0 && u_debugMode.x != 6.0) {
        spec *= 0;
    }
    return spec;
}

vec3 calculateDiffuse(vec3 diffuse_colour) {
    // Lambert
    vec3 diff = diffuse_colour * ONE_OVER_PI;
    if (u_debugMode.x != 0.0 && u_debugMode.x != 5.0) {
        diff *= 0;
    }
    return diff;
}

/*
    Returns the normal from the normal map in view space. 
    Assumining that norm, tang & bitang have been transformed into model/world space.
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
    normal = mul(fromTangentSpace, normal);
    //normal = mul(u_view, normal);
    return normal;
}

vec3 calculateIBL(vec3 pos_ws, vec3 to_eye, vec3 normal, vec3 albedo, float roughness, float metal) {
    vec3 loc_v = floor((pos_ws-u_gridOffset.xyz)/u_gridParams.w);
    //float loc_i = loc_v.z*u_gridParams[0]*u_gridParams[1] + loc_v.y*u_gridParams[0] + loc_v.x;
    float loc_i = dot(vec3(1, u_gridParams[0], u_gridParams[0]*u_gridParams[1]), loc_v);

    vec3 kS = fresnel_schlick_roughness(metal ? albedo : SHADER_NONMETAL_SPEC_COLOUR, to_eye, normal, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metal;

    //vec3 irr = textureCubeArrayLod(s_irradiance, vec4(normal.xyz, loc_i), 0);
    vec3 irr = textureCubeLod(s_irradianceMap, vec3(normal.xyz), 0);
    vec3 diffuse = albedo * irr;

    if (u_debugMode.x == 7.0) {
        return kD * diffuse;
    }

    vec3 r = reflect(to_eye, normal);
    //vec3 spec_irr = textureCubeArrayLod(s_specIrradiance, vec4(r.xyz, loc_i), floor(roughness*10)).rgb;
    vec3 spec_irr = textureCubeLod(s_environmentMap, vec3(r.xyz), floor(roughness*10)).rgb;
    vec2 env_bdrf = texture2D(s_bdrfLUT, vec2(max(dot(normal, to_eye), 0), roughness));
    vec3 spec_c = spec_irr * (kS * env_bdrf.x + env_bdrf.y);

    if (u_debugMode.x == 8.0) {
        return spec_c;
    }

    return (kD * diffuse + spec_c);

    //return to_eye * 5. + .5;
    //return (loc_i/72.0).xxx;
    //return spec_irr * loc_v / vec3(6,3,4);
    //return spec_irr;
    //return irr + spec_irr;
    //return irr;
    //return spec_c;
    //return vec3(env_bdrf.xy, 0);
}

void main() {
    vec3 sunlight_vs = normalize(mul((mat3)u_view, u_sunlight.xyz));
    uint2 grid_info = light_grid_data(gl_FragCoord.xy);
    uint2 z_bin = read_z_bin(v_view.z);

    vec2 uv = vec2(v_texcoord0.x, 1-v_texcoord0.y);

#if !NO_TEXTURES
//#if ALPHA_MASK
    float mask = texture2D(s_mask, uv);
    if (mask < 0.5)
        discard;
//#endif
    vec4 albedo = texture2D(s_base, uv);
    //vec3 normal_ws = normalize(v_normal);
    vec3 normal_ws = readNormalMap(v_normal, v_tangent, v_bitangent, uv);
    vec3 normal = mul(u_view, normal_ws);
    vec3 metal_value = texture2D(s_metallic, uv);
    bool metal = metal_value > 0;
    vec4 rough = texture2D(s_roughness, uv);
#else
    vec4 albedo = u_albedo;
    vec3 normal_ws = normalize(v_normal);
    vec3 normal = normalize(mul(u_view,v_normal));
    vec3 metal_value = u_metalrough.xxx;
    bool metal = u_metalrough.x > 0;
    vec4 rough = u_metalrough.yyyy;
#endif

#if DEBUG_MODES
    if (u_debugMode.x > 0) {
        gl_FragColor.w = 1.0;
        if (u_debugMode.x == 1.0) {
            gl_FragColor.rgb = albedo.rgb;
            return;
        }
        if (u_debugMode.x == 2.0) {
            gl_FragColor.rgb = normal_ws.rgb * .5 + .5;
            return;
        }
        if (u_debugMode.x == 3.0) {
            gl_FragColor.rgb = metal_value.rrr;
            return;
        }
        if (u_debugMode.x == 4.0) {
            gl_FragColor.rgb = rough.rrr;
            return;
        }
    }
#endif
    gl_FragColor.w = 1.0;

    gl_FragColor.rgb = vec3(0, 0, 0);
    if (u_dynamicLights.x > 0)  {
        vec3 ibl = calculateIBL(v_wpos, normalize(mul(u_invView, normalize(v_view))), normal_ws, albedo, rough, metal);
        if (u_debugMode.x > 6.0) {
            gl_FragColor.rgb = ibl;
            return;
        } else if (u_debugMode.x == 0.0) {
            gl_FragColor.rgb = ibl;
        }
    }

    // Ambient
    gl_FragColor.rgb += (u_ambientColour.rgb * albedo.rgb) * u_ambientColour.a;
    if (u_debugMode.x != 0.0 && u_debugMode.x != 5.0) {
        gl_FragColor.rgb *= 0;
    }

    // Sunlight
    {
        vec3 lightDir = -(sunlight_vs.xyz);
        vec3 ndotl = saturate(dot(lightDir, normal));    
        vec3 toEye = -normalize(v_view);
        vec3 half = normalize(toEye + lightDir);
        // Sunlight is white so the PI goes into the maths raw
        gl_FragColor.rgb += ndotl * (
            ((calculateDiffuse(albedo.rgb) * PI * u_sunlight.w)*(1-metal)) +
            calculateSpecular(normal, half, toEye, lightDir, rough.r, metal ? albedo.rgb : SHADER_NONMETAL_SPEC_COLOUR)
        );
    }

    // dynamic Lights are discarded from cubemap captures.
    if (u_dynamicLights.x == 0) {
        return;
    }

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
            vec3 light_c = lightData[real_light_idx+1].rgb * PI;
            vec3 pos_to_light = light_pos - v_view;

            float attenuation = light_distance_attenuation(1/light_r, length(pos_to_light), LIGHT_FALLOFF);
            vec3 lightDir = normalize(pos_to_light);
            vec3 toEye = -normalize(v_view);
            vec3 half = normalize(toEye + lightDir);
            vec3 ndotl = saturate(dot(lightDir, normal));            
#if DEBUG_LIGHT_EVAL
            gl_FragColor.r += 1;       
#else
            gl_FragColor.xyz += (ndotl * attenuation) * (
                (light_c * calculateDiffuse(albedo.rgb)) +
                calculateSpecular(normal, half, toEye, lightDir, rough.r, metal ? albedo.rgb : SHADER_NONMETAL_SPEC_COLOUR)
            );
#endif
#if !DEBUG_NO_BINS
        }
#endif
    }

#if DEBUG_LIGHT_EVAL
    gl_FragColor.r /= 20;
#endif
}
