$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"

SAMPLER2D(s_luminance, 1);

uniform vec4 u_offset[3];

// Do ACES tonemap
void main() {
    vec2 uv = vec2(v_texcoord0.x, v_texcoord0.y);
    float pixel_luma;
    if (u_offset[2].x) {
        pixel_luma  = log(luma(texture2D(s_luminance, uv + u_offset[0].xy)).r);
        pixel_luma += log(luma(texture2D(s_luminance, uv + u_offset[0].zw)).r);
        pixel_luma += log(luma(texture2D(s_luminance, uv + u_offset[1].xy)).r);
        pixel_luma += log(luma(texture2D(s_luminance, uv + u_offset[1].zw)).r);
    } else {
        pixel_luma  = luma(texture2D(s_luminance, uv + u_offset[0].xy)).r;
        pixel_luma += luma(texture2D(s_luminance, uv + u_offset[0].zw)).r;
        pixel_luma += luma(texture2D(s_luminance, uv + u_offset[1].xy)).r;
        pixel_luma += luma(texture2D(s_luminance, uv + u_offset[1].zw)).r;
    }
    gl_FragColor.rgb = pixel_luma.xxx / 4.0;
    gl_FragColor.w = 1.0;

}
