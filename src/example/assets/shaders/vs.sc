$input a_position, a_normal, a_tangent, a_texcoord0
$output v_pos, v_wpos, v_view, v_normal, v_tangent, v_bitangent, v_texcoord0

#include "../../../common/common.sh"

void main()
{
    v_wpos = mul(u_model[0], vec4(a_position, 1.0) ).xyz;
    gl_Position = mul(u_viewProj, vec4(v_wpos, 1.0) );

    v_texcoord0 = a_texcoord0;
    v_normal = mul((mat3)u_model[0], a_normal.xyz).xyz;
    v_tangent = mul((mat3)u_model[0], a_tangent.xyz).xyz;
    
    v_normal = mul((mat3)u_view, v_normal.xyz).xyz;
    v_tangent = mul((mat3)u_view, v_tangent.xyz).xyz;
    // a_tangent.w does flipping, depending on triangle winding order
    v_bitangent = cross(v_normal, v_tangent) * a_tangent.w;

    v_view = mul(u_view, vec4(v_wpos.xyz, 1)).xyz;
}
