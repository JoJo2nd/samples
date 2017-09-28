$input a_position, a_color0
$output v_color0

/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "../common/common.sh"

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0) ).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );
    v_color0 = a_color0;//a_color0.r > 0 ? vec4(1, 0, 0, 1) : vec4(0, 1, 0, 1);
}