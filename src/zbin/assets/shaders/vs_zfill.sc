$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3
$output v_pos

#include "../../../common/common.sh"

void main()
{
    mat4 model;
    model[0] = i_data0;
    model[1] = i_data1;
    model[2] = i_data2;
    model[3] = i_data3;

    vec3 wpos = mul(model, vec4(a_position, 1.0) ).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );
}
