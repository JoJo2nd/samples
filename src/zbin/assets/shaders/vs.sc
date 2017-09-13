$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3
$output v_pos, v_wpos, v_view, v_normal, v_color0

#include "../../../common/common.sh"

void main()
{
    mat4 model;
    model[0] = i_data0;
    model[1] = i_data1;
    model[2] = i_data2;
    model[3] = i_data3;

    v_wpos = mul(model, vec4(a_position, 1.0) ).xyz;
    gl_Position = mul(u_viewProj, vec4(v_wpos, 1.0) );

    v_normal = mul((mat3)model, a_normal.xyz).xyz;
    v_normal = mul((mat3)u_view, v_normal.xyz).xyz;
    v_view = mul(u_view, vec4(v_wpos.xyz, 1)).xyz;
    v_color0 = vec4(0.1, 0.1, 0.1, 1.0);
}
