$input v_texcoord0

/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "common/common.sh"

SAMPLER2D(s_src, 0);

void main()
{
    vec4 a  = texture2D(s_src, v_texcoord0);
    gl_FragColor = vec4(a.rgb, 1);
}
