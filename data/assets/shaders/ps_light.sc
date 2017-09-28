$input v_pos

#include "common/common.sh"

uniform vec4 u_lightPos;

SAMPLER2D(s_albedo, 0);
SAMPLER2D(s_normal, 1);
SAMPLER2D(s_depth, 2);

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

vec3 decodeNormal(vec3 n) {
    return (n - .5f) * 2.f;
}

float decodeDepth(vec3 idepth) {
    //return f16tof32(idepth.x);
    return idepth.x;
}

void main()
{
    vec2 screenUV = (gl_FragCoord.xy / vec2(1280, 720));//* 2.0 - 1.0;
    vec4 norDepth = texture2D(s_normal, screenUV).xyzw;
    vec3 normal = normalize(decodeNormal(norDepth.xyz));
    float depth = norDepth.w;//decodeDepth(texture2D(s_depth, screenUV).rgb);
    vec3 albedo = texture2D(s_albedo, screenUV).rgb;
    screenUV = screenUV*2-1;
    vec4 posVS = mul(u_invProj, vec4(screenUV.x, -screenUV.y, depth, 1));
    posVS = posVS/posVS.w;
    posVS = mul(u_invView, vec4(posVS.xyz, 1));
    vec3 lightVS = u_lightPos.xyz;//mul(u_view, vec4(u_lightPos.xyz,1));
    vec3 lightVec = lightVS.xyz-posVS.xyz;
    vec3 lightDir = normalize(lightVec);
    vec3 view = normalize(u_invView[3].xyz-posVS.xyz);
    vec2 bln = blinn(lightDir, normal, view);
    vec4 lc = lit(bln.x, bln.y, 1.0);
    float fres = fresnel(bln.x, 0.2, 5.0);
    float attn = pow(saturate(1-(length(lightVec)/20.f)), 2);

    gl_FragColor.xyz = vec3(0.07, 0.06, 0.08) + albedo*lc.y + fres*pow(lc.z, 128.0);
    gl_FragColor.xyz *= attn;
    gl_FragColor.w = 1.0;
}