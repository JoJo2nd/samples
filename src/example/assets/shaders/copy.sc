$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "ACES.sh"

// Do ACES tonemap
void main() {
    vec2 uv = vec2(v_texcoord0.x, v_texcoord0.y);
    // base is in gamma space with no sRGB read so just raw copy
    vec4 base = texture2D(s_base, uv);

    gl_FragColor.rgb = base.rgb;
    gl_FragColor.w = 1.0;
}
