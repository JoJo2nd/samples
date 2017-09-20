/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include <bgfx_shader.sh>
#include <bgfx_compute.sh>
#include "shaderlib.sh"

SAMPLER2D(s_base, 0);
SAMPLER2D(s_metallic, 1);
SAMPLER2D(s_normal, 2);
SAMPLER2D(s_roughness, 3);
SAMPLER2D(s_mask, 4);
