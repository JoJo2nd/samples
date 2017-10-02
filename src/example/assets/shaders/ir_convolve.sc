$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"

uniform vec4 u_cubeFace;

void main()
{       
    vec2 uv = (vec2(v_texcoord0.x, v_texcoord0.y) * 2) - 1;
    vec3 normal, up;

    if (u_cubeFace.x == 0) {
        normal = normalize( vec3(1, -uv.y, -uv.x) );
        up = vec3(0, 1, 0);
    } else if (u_cubeFace.x == 1) {
        normal = normalize( vec3(-1, -uv.y, uv.x) );
        up = vec3(0, 1, 0);
    } else if (u_cubeFace.x == 2) {
        normal = normalize( vec3(uv.x, 1, uv.y) );
        up = vec3(0, 0, 1);
    } else if (u_cubeFace.x == 3) {
        normal = normalize( vec3(uv.x, -1, -uv.y) );
        up = vec3(0, 0, 1);
    } else if (u_cubeFace.x == 4) {
        normal = normalize( vec3(uv.x, -uv.y, 1) );
        up = vec3(0, 1, 0);
    } else if (u_cubeFace.x == 5) {
        normal = normalize( vec3(-uv.x, -uv.y, -1) );
        up = vec3(0, 1, 0);
    }

    vec3 irradiance = vec3(0.0, 0.0, 0.0);  
    vec3 right = cross(up, normal);
    up         = cross(normal, right);

    float sampleDelta = 0.025;
    float sampleDelta2 = 0.025;
    float nrSamples = 0.0; 
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta2)
        {
            // spherical to cartesian (in tangent space)
            //vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            //vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 
            //irradiance += textureCube(s_environmentMap, sampleVec).rgb * cos(theta) * sin(theta);

            vec3 temp = cos(phi) * right + sin(phi) * up;
            vec3 sampleVector = cos(theta) * normal + sin(theta) * temp;
            irradiance += clamp(textureCubeLod( s_environmentMap, sampleVector, 0 ).rgb, 0, 10) *  cos(theta) * sin(theta);

            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    //irradiance = textureCube(s_environmentMap, normal).rgb;

    gl_FragColor = vec4(irradiance, 1.0);
}
