$input a_position, a_normal
$output v_pos, v_view, v_normal, v_color0

/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "../common/common.sh"

uniform mat4  u_invViewXpose;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0) ).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );

    v_normal = mul((mat3)u_modelView, a_normal.xyz ).xyz;
    v_view = mul(u_modelView, vec4(a_position, 1.0) ).xyz;
    v_color0 = vec4(0.1, 0.1, 0.1, 1.0);
}