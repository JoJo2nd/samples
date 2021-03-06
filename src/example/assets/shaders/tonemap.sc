$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "ACES.sh"

uniform vec4 u_exposureParams;

#define u_exposureMin (u_exposureParams.x)
#define u_exposureMax (u_exposureParams.y)
#define u_exposureKey (u_exposureParams.z)

SAMPLER2D(s_luminance, 1);
SAMPLER3D(s_colourLUT, 5);// colour LUT
BUFFER_RO(averageLuma,             float, 2);

// Do ACES tonemap
void main() {
    vec2 uv = vec2(v_texcoord0.x, v_texcoord0.y);
    // base is in linear space
    vec4 base = texture2D(s_base, uv);

    //float avg = averageLuma[0];
    //float avg = exp2(texture2D(s_luminance, vec2(.5,.5)));
    float avg = 0.6;
    // 0.18 is middle grey
    float exp_scale = u_exposureKey / (clamp(avg, u_exposureMin, u_exposureMax));
    base.rgb *= exp_scale;

    // rough gamma curve convert. ACESFitted expects an sRGB input
    base.rgb = toGamma(base.rgb);
    base.rgb = ACESFitted(base.rgb);

    //colour grade
    base.rgb = toLinear(base.rgb);
    base.rgb = texture3DLod(s_colourLUT, /*vec3(0,1,0)/*/base.rgb, 0).rgb;
    base.rgb = toGamma(base.rgb);

    gl_FragColor.rgb = base.rgb;
    gl_FragColor.w = 1.0;
}
