$input a_position, a_texcoord0
$output v_texcoord0

#include "../../../common/common.sh"


void main()
{
    gl_Position = mul(u_viewProj, vec4(a_position, 1.0));
    gl_Position.z = 1;
    v_texcoord0 = gl_Position.xy/gl_Position.w;
}
