$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "convolve_common.sh"

uniform vec4 u_cubeFace;

#define u_roughness (u_cubeFace.y)

void main()
{       
    vec2 uv = (vec2(v_texcoord0.x, v_texcoord0.y) * 2) - 1;
    vec3 normal;

    if (u_cubeFace.x == 0) {
        normal = normalize( vec3(1, -uv.y, -uv.x) );
    } else if (u_cubeFace.x == 1) {
        normal = normalize( vec3(-1, -uv.y, uv.x) );
    } else if (u_cubeFace.x == 2) {
        normal = normalize( vec3(uv.x, 1, uv.y) );
    } else if (u_cubeFace.x == 3) {
        normal = normalize( vec3(uv.x, -1, -uv.y) );
    } else if (u_cubeFace.x == 4) {
        normal = normalize( vec3(uv.x, -uv.y, 1) );
    } else if (u_cubeFace.x == 5) {
        normal = normalize( vec3(-uv.x, -uv.y, -1) );
    }

    vec3 N = normalize(normal);    
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;   
    vec3 prefilteredColor = vec3(0, 0, 0);     
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, u_roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            vec3 cc = textureCube(s_environmentMap, L).rgb;
            prefilteredColor += cc * NdotL;
            totalWeight      += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    gl_FragColor = vec4(prefilteredColor, 1.0);
}
