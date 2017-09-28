$input v_color0

#include "../common/common.sh"

void main() {
    gl_FragColor.xyz = v_color0.rgb;
    gl_FragColor.w = 1.0;
}