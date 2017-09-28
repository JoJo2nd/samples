$input a_position, a_normal
$output v_pos

#include "../../../common/common.sh"

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0) ).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );
}
