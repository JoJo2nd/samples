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

// colour LUT or Skybox
SAMPLERCUBE(s_environmentMap, 6);
SAMPLERCUBE(s_irradianceMap, 7);

// IBL
SAMPLER2D(s_bdrfLUT, 5);

struct BgfxSamplerCubeArray {
    SamplerState m_sampler;
    TextureCubeArray m_texture;
};

#define SAMPLERCUBEARRAY(_name, _reg) \
    uniform SamplerState _name ## Sampler : REGISTER(s, _reg); \
    uniform TextureCube _name ## Texture : REGISTER(t, _reg); \
    static BgfxSamplerCubeArray _name = { _name ## Sampler, _name ## Texture }

#define textureCubeArrayLod(sampler, coord, mip) bgfxTextureCubeLod(sampler, coord, mip)

vec4 bgfxTextureCubeLod(BgfxSamplerCubeArray _sampler, vec4 _coord, uint _level)
{
    return _sampler.m_texture.SampleLevel(_sampler.m_sampler, _coord, _level);
}

SAMPLERCUBEARRAY(s_irradiance, 8);
SAMPLERCUBEARRAY(s_specIrradiance, 9);
