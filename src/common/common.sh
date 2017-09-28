/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include <bgfx_shader.sh>
#include <bgfx_compute.sh>
#include "shaderlib.sh"

// albedo in forward pass, main input for most other shaders
SAMPLER2D(s_base, 0);
// metal value (0 or 1)
SAMPLER2D(s_metallic, 1);
// tangent space normals
SAMPLER2D(s_normal, 2);
// roughness
SAMPLER2D(s_roughness, 3);
// alpha mask
SAMPLER2D(s_mask, 4);

// colour LUT
SAMPLER3D(s_colourLUT, 5);
SAMPLERCUBE(s_environmentMap, 6);
