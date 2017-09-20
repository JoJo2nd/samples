$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "ACES.sh"

// Do ACES tonemap
void main() {
    vec2 uv = vec2(v_texcoord0.x, v_texcoord0.y);
    vec4 base = texture2D(s_base, uv);

    // rough gamma curve convert. ACESFitted expects an sRGB input
    base.rgb = toGamma(base.rgb);

    gl_FragColor.rgb = ACESFitted(base.rgb);
    gl_FragColor.w = 1.0;

}
