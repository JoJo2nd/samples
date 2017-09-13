$input v_pos, v_view, v_normal, v_color0, v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_albedo, 0);

vec2 blinn(vec3 _lightDir, vec3 _normal, vec3 _viewDir)
{
    float ndotl = dot(_normal, _lightDir);
    vec3 reflected = _lightDir - 2.0*ndotl*_normal; // reflect(_lightDir, _normal);
    float rdotv = dot(reflected, _viewDir);
    return vec2(ndotl, rdotv);
}

float fresnel(float _ndotl, float _bias, float _pow)
{
    float facing = (1.0 - _ndotl);
    return max(_bias + (1.0 - _bias) * pow(facing, _pow), 0.0);
}

vec4 lit(float _ndotl, float _rdotv, float _m)
{
    float diff = max(0.0, _ndotl);
    float spec = step(0.0, _ndotl) * max(0.0, _rdotv * _m);
    return vec4(1.0, diff, spec, 1.0);
}

void main()
{
    vec3 lightDir = normalize(vec3(0, 0, -1.f));
    vec3 normal = normalize(v_normal);
    vec3 view = normalize(v_view);
    vec2 bln = blinn(lightDir, normal, view);
    vec4 lc = lit(bln.x, bln.y, 1.0);
    float fres = fresnel(bln.x, 0.2, 5.0);
    float ndotl = dot(normal, lightDir);

    vec4 albedo = texture2D(s_albedo, v_texcoord0.xy);
    gl_FragColor.xyz = vec3(0.07, 0.06, 0.08) + albedo.rgb*lc.y + fres*pow(lc.z, 128.0);
    gl_FragColor.w = albedo.a;
}