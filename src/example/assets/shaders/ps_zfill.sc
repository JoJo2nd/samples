$input v_pos, v_texcoord0

#include "../../../common/common.sh"
#include "../../common_defines.h"


void main() {
//    vec2 uv = vec2(v_texcoord0.x, 1-v_texcoord0.y);
//    float mask = texture2D(s_mask, uv);
//    if (mask < 0.5)
//        discard;
// Colour write should be disabled.
    gl_FragColor = vec4(1, 1, 1, 1);
}
