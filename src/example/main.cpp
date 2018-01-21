

#pragma comment(lib, "ws2_32.lib")
#define RMT_USE_D3D11 (1)
#define ENTRY_CONFIG_IMPLEMENT_MAIN (1)

#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "camera.h"
#include "entry/input.h"
#include <stdio.h>
#include <math.h>
#include "../common/debugdraw/debugdraw.h"
#include "../common/debugdraw/debugdraw.cpp"
#include "../common/imgui/imgui.h"
//#include "../common/imgui/imgui.cpp"
#include <assert.h>
#include "common_util.h"
//#include "Remotery.h"
//#include "Remotery.c"

#include "common_defines.h"
#include "bx/readerwriter.h"
#include "bimg/bimg.h"
#include "bimg/decode.h"

//#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <windows.h>

static float s_texelHalf = 0.0f;
bool         dbgCaptureCubemap = 0; // when true, capturing cubemap faces
bool         dbgWaitingOnReadback = 0;

uint32_t dbgCaptureReadyFrame = 0;
uint32_t frameReturn;

bgfx::UniformHandle dynLightsUniform;
bgfx::UniformHandle cubeFaceUniform;
bgfx::UniformHandle environmentMapTex;
bgfx::UniformHandle irradianceMapTex;
bgfx::UniformHandle gridSizeUniform;
bgfx::UniformHandle gridOffsetUniform;
bgfx::UniformHandle specIrradianceAraryTex;
bgfx::UniformHandle irradianceAraryTex;
bgfx::UniformHandle bdrfTex;

bgfx::FrameBufferHandle cubemapFaceTarget[6];
bgfx::FrameBufferHandle irCubemapFaceTarget[6];
bgfx::FrameBufferHandle sirCubemapFaceTarget[6][SIR_MIP_COUNT];
bgfx::FrameBufferHandle bdrfConvolveTarget;

bgfx::TextureHandle defaultMask;
bgfx::TextureHandle cubemapDepthTexture;
bgfx::TextureHandle cubemapFaceTexture;
bgfx::TextureHandle cubemapFaceTextureRead[6];
bgfx::TextureHandle irCubemapFaceTexture;
bgfx::TextureHandle irCubemapFaceTextureRead[6];
bgfx::TextureHandle sirCubemapFaceTexture[SIR_MIP_COUNT];
bgfx::TextureHandle sirCubemapFaceTextureRead[6][SIR_MIP_COUNT];
bgfx::TextureHandle bdrfConvolveTexture;
bgfx::TextureHandle bdrfConvolveTextureRead;

bgfx::TextureHandle irCubemapArrayTexture = BGFX_INVALID_HANDLE;
bgfx::TextureHandle specIRCubemapArrayTexture = BGFX_INVALID_HANDLE;
bgfx::TextureHandle bdrfTexture = BGFX_INVALID_HANDLE;

float    totalBoundsMin[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
float    totalBoundsMax[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
uint32_t envMapGridDim[3];
uint32_t envMapCurrentCell[3];
uint32_t sirTotalWriteSize;
uint32_t sirMipLevelSizeBytes[SIR_MIP_COUNT];           //*6 to get faces
uint32_t sirMipFaceWriteOffsetLookUp[SIR_MIP_COUNT][6]; //
uint32_t totalProbes;
int32_t  curEditProbe;
uint32_t curCaptureProbe;

void* irCubemapFaceTextureReadCPUData;
void* cubemapFaceTextureReadCPUData;
void* sirCubemapFaceTextureReadCPUData;
void* bdrfTextureReadCPUData;

struct probe_t {
  vec3_t position;
  vec3_t halfWidths;
} * probeArray; //[totalProbes]
typedef struct probe_t probe_t;

struct PosTexCoord0Vertex {
  float m_x;
  float m_y;
  float m_z;
  float m_u;
  float m_v;

  static void init() {
    ms_decl.begin()
      .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
      .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
      .end();
  }

  static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl PosTexCoord0Vertex::ms_decl;

typedef uint16_t half_t;

inline void float_to_half(half_t* hfptr, float f) {
  union Data32 {
    uint32_t u32;
    float    f32;
  };

  Data32 d;
  d.f32 = f;

  uint32_t sign = d.u32 >> 31;
  uint32_t exponent = (d.u32 >> 23) & ((1 << 8) - 1);
  uint32_t mantissa = d.u32 & ((1 << 23) - 1);
  ;

  if (exponent == 0) {
    // zero or denorm -> zero
    mantissa = 0;

  } else if (exponent == 255 && mantissa != 0) {
    // nan -> infinity
    exponent = 31;
    mantissa = 0;

  } else if (exponent >= 127 - 15 + 31) {
    // overflow or infinity -> infinity
    exponent = 31;
    mantissa = 0;

  } else if (exponent <= 127 - 15) {
    // underflow -> zero
    exponent = 0;
    mantissa = 0;

  } else {
    exponent -= 127 - 15;
    mantissa >>= 13;
  }

  *hfptr = (half_t)((sign << 15) | (exponent << 10) | mantissa);
}

static const bgfx::Memory* loadMem(const char* filePath) {
  FILE* f = fopen(filePath, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    uint32_t size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    const bgfx::Memory* mem = bgfx::alloc(size + 1);
    fread(mem->data, 1, size, f);
    fclose(f);
    mem->data[mem->size - 1] = '\0';
    return mem;
  }

  DBG("Failed to load %s.", filePath);
  return NULL;
}

void screenSpaceQuad(float _textureWidth,
                     float _textureHeight,
                     float _texelHalf,
                     bool  _originBottomLeft,
                     float _width = 1.0f,
                     float _height = 1.0f) {
  if (bgfx::getAvailTransientVertexBuffer(3, PosTexCoord0Vertex::ms_decl)) {
    bgfx::TransientVertexBuffer vb;
    bgfx::allocTransientVertexBuffer(&vb, 3, PosTexCoord0Vertex::ms_decl);
    PosTexCoord0Vertex* vertex = (PosTexCoord0Vertex*)vb.data;

    const float minx = -_width;
    const float maxx = _width;
    const float miny = 0.0f;
    const float maxy = _height * 2.0f;

    const float texelHalfW = _texelHalf / _textureWidth;
    const float texelHalfH = _texelHalf / _textureHeight;
    const float minu = -1.0f + texelHalfW;
    const float maxu = 1.0f + texelHalfH;

    const float zz = 0.0f;

    float minv = texelHalfH;
    float maxv = 2.0f + texelHalfH;

    if (_originBottomLeft) {
      float temp = minv;
      minv = maxv;
      maxv = temp;

      minv -= 1.0f;
      maxv -= 1.0f;
    }

    vertex[0].m_x = minx;
    vertex[0].m_y = miny;
    vertex[0].m_z = zz;
    vertex[0].m_u = minu;
    vertex[0].m_v = minv;

    vertex[1].m_x = maxx;
    vertex[1].m_y = miny;
    vertex[1].m_z = zz;
    vertex[1].m_u = maxu;
    vertex[1].m_v = minv;

    vertex[2].m_x = maxx;
    vertex[2].m_y = maxy;
    vertex[2].m_z = zz;
    vertex[2].m_u = maxu;
    vertex[2].m_v = maxv;

    bgfx::setVertexBuffer(0, &vb);
  }
}

static void imageReleaseCb(void* _ptr, void* _userData)
{
  BX_UNUSED(_ptr);
  bimg::ImageContainer* imageContainer = (bimg::ImageContainer*)_userData;
  bimg::imageFree(imageContainer);
}

bgfx::TextureHandle loadTexture(void* data, uint32_t size, char const* name, uint32_t _flags = BGFX_TEXTURE_NONE, uint8_t _skip = 0, bgfx::TextureInfo* _info = NULL, bimg::Orientation::Enum* _orientation = NULL) {
  BX_UNUSED(_skip);
  bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;

  if (NULL != data)
  {
    bimg::ImageContainer* imageContainer = bimg::imageParse(entry::getAllocator(), data, size);

    if (NULL != imageContainer)
    {
      if (NULL != _orientation)
      {
        *_orientation = imageContainer->m_orientation;
      }

      const bgfx::Memory* mem = bgfx::makeRef(
        imageContainer->m_data
        , imageContainer->m_size
        , imageReleaseCb
        , imageContainer
      );
      //unload(data);
      bx::free(entry::getAllocator(), data);

      if (imageContainer->m_cubeMap)
      {
        handle = bgfx::createTextureCube(
          uint16_t(imageContainer->m_width)
          , 1 < imageContainer->m_numMips
          , imageContainer->m_numLayers
          , bgfx::TextureFormat::Enum(imageContainer->m_format)
          , _flags
          , mem
        );
      }
      else if (1 < imageContainer->m_depth)
      {
        handle = bgfx::createTexture3D(
          uint16_t(imageContainer->m_width)
          , uint16_t(imageContainer->m_height)
          , uint16_t(imageContainer->m_depth)
          , 1 < imageContainer->m_numMips
          , bgfx::TextureFormat::Enum(imageContainer->m_format)
          , _flags
          , mem
        );
      }
      else
      {
        handle = bgfx::createTexture2D(
          uint16_t(imageContainer->m_width)
          , uint16_t(imageContainer->m_height)
          , 1 < imageContainer->m_numMips
          , imageContainer->m_numLayers
          , bgfx::TextureFormat::Enum(imageContainer->m_format)
          , _flags
          , mem
        );
      }

      bgfx::setName(handle, name);

      if (NULL != _info)
      {
        bgfx::calcTextureSize(
          *_info
          , uint16_t(imageContainer->m_width)
          , uint16_t(imageContainer->m_height)
          , uint16_t(imageContainer->m_depth)
          , imageContainer->m_cubeMap
          , 1 < imageContainer->m_numMips
          , imageContainer->m_numLayers
          , bgfx::TextureFormat::Enum(imageContainer->m_format)
        );
      }
    }
  }

  return handle;
}

struct CameraParams {
  float  fovy, aspect, nearPlane, farPlane;
  float  x, y, z;
  float  viewProj[16];
  float  view[16];
  vec3_t up, right, forward;
};

struct LightInfo {
  vec3_t p;
  float  radius;
  vec3_t col;
  float  pad;
};

struct LightInit {
  vec3_t p;
  float  radius;
  vec3_t col;
  float  r[3];
};

struct LightGridRange {
  uint16_t start;
  uint16_t count;
};

struct RunParams {
  char const* meshVSPath;
  char const* meshPSPath;
  char const* meshPSMaskPath;

  char const* fsDbgVSPath;
  char const* fsDbgPSPath;

  char const* lightCullCSPath;

  char const* description;

  char const* meshPSNoTexturePath;

  bgfx::ProgramHandle solidProg;
  bgfx::ProgramHandle solidProgMask;
  bgfx::ProgramHandle solidProgNoTex;
  bgfx::ProgramHandle fullscreenProg;
  bgfx::ProgramHandle csLightCull;
  bgfx::ProgramHandle zfillProg;
};

#define SHADER_DBG(vs1, fs1, fsm1, cs, dbvs, dbfs, name)                                                               \
  {                                                                                                                    \
    vs1, fs1, fsm1, dbvs, dbvs, cs, name, FS_NO_TEX_ASSET_PATH, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,              \
      BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE                               \
  }

#define SHADER_C(vs1, fs1, fsm1, cs, name) SHADER_DBG(vs1, fs1, fsm1, cs, nullptr, nullptr, name)
#define SHADER(vs1, fs1, fsm1, name) SHADER_C(vs1, fs1, fsm1, nullptr, name)

RunParams ShaderParams[SOLID_PROGS] = {
  SHADER(VS_ASSET_PATH, FS_ASSET_PATH, FS_MASK_ASSET_PATH, "CPU Z Bin, CPU Tile Light Cull"),
  SHADER_C(VS_ASSET_PATH,
           FS_ASSET_PATH,
           FS_MASK_ASSET_PATH,
           CS_LIGHT_CULL_PATH_NO_OPT,
           "CPU ZBin, GPU Light Cull (Slow)"),
  SHADER_C(VS_ASSET_PATH, FS_ASSET_PATH, FS_MASK_ASSET_PATH, CS_LIGHT_CULL_PATH, "CPU Z Bin, GPU Tile Light Cull"),
  SHADER_DBG(VS_ASSET_PATH,
             FS_ASSET_PATH,
             FS_MASK_ASSET_PATH,
             CS_LIGHT_CULL_PATH,
             VS_DEBUG_ASSET_PATH,
             FS_FSDEBUG_WCS_ASSET_PATH,
             "CPU Z Bin, GPU Tile Light Cull (Debug Overlay)"),
  SHADER_DBG(VS_ASSET_PATH,
             FS_ASSET_PATH,
             FS_MASK_ASSET_PATH,
             nullptr,
             VS_DEBUG_ASSET_PATH,
             FS_FSDEBUG_WCS_ASSET_PATH,
             "CPU Z Bin, CPU Tile Light Cull (Debug Overlay)"),
  SHADER(VS_ASSET_PATH, FS_DEBUG01_ASSET_PATH, nullptr, "Z Bin light evals"),
  SHADER(VS_ASSET_PATH, FS_DEBUG02_ASSET_PATH, nullptr, "Tiled light evals"),
};

struct OrbParams {
  float  colour[4];
  vec3_t position;
  float  roughnes;
  bool   metal;
};

OrbParams Orbs[TOTAL_ORBS] = {
  // Red, Metal
  {{1.f, 0.f, 0.f, 1.f}, {-40.f, 1.f, 12.f}, 1.0f, true},
  {{1.f, 0.f, 0.f, 1.f}, {-32.f, 1.f, 12.f}, 0.9f, true},
  {{1.f, 0.f, 0.f, 1.f}, {-24.f, 1.f, 12.f}, 0.8f, true},
  {{1.f, 0.f, 0.f, 1.f}, {-16.f, 1.f, 12.f}, 0.7f, true},
  {{1.f, 0.f, 0.f, 1.f}, {-8.f, 1.f, 12.f}, 0.6f, true},
  {{1.f, 0.f, 0.f, 1.f}, {8.f, 1.f, 12.f}, 0.5f, true},
  {{1.f, 0.f, 0.f, 1.f}, {16.f, 1.f, 12.f}, 0.4f, true},
  {{1.f, 0.f, 0.f, 1.f}, {24.f, 1.f, 12.f}, 0.3f, true},
  {{1.f, 0.f, 0.f, 1.f}, {32.f, 1.f, 12.f}, 0.2f, true},
  {{1.f, 0.f, 0.f, 1.f}, {40.f, 1.f, 12.f}, 0.1f, true},
  // Red, Dieletric
  {{1.f, 0.f, 0.f, 1.f}, {-40.f, 1.f, 8.f}, 1.0f, false},
  {{1.f, 0.f, 0.f, 1.f}, {-32.f, 1.f, 8.f}, 0.9f, false},
  {{1.f, 0.f, 0.f, 1.f}, {-24.f, 1.f, 8.f}, 0.8f, false},
  {{1.f, 0.f, 0.f, 1.f}, {-16.f, 1.f, 8.f}, 0.7f, false},
  {{1.f, 0.f, 0.f, 1.f}, {-8.f, 1.f, 8.f}, 0.6f, false},
  {{1.f, 0.f, 0.f, 1.f}, {8.f, 1.f, 8.f}, 0.5f, false},
  {{1.f, 0.f, 0.f, 1.f}, {16.f, 1.f, 8.f}, 0.4f, false},
  {{1.f, 0.f, 0.f, 1.f}, {24.f, 1.f, 8.f}, 0.3f, false},
  {{1.f, 0.f, 0.f, 1.f}, {32.f, 1.f, 8.f}, 0.2f, false},
  {{1.f, 0.f, 0.f, 1.f}, {40.f, 1.f, 8.f}, 0.1f, false},

  // Green, Metal
  {{0.f, 1.f, 0.f, 1.f}, {-40.f, 1.f, 4.f}, 1.0f, true},
  {{0.f, 1.f, 0.f, 1.f}, {-32.f, 1.f, 4.f}, 0.9f, true},
  {{0.f, 1.f, 0.f, 1.f}, {-24.f, 1.f, 4.f}, 0.8f, true},
  {{0.f, 1.f, 0.f, 1.f}, {-16.f, 1.f, 4.f}, 0.7f, true},
  {{0.f, 1.f, 0.f, 1.f}, {-8.f, 1.f, 4.f}, 0.6f, true},
  {{0.f, 1.f, 0.f, 1.f}, {8.f, 1.f, 4.f}, 0.5f, true},
  {{0.f, 1.f, 0.f, 1.f}, {16.f, 1.f, 4.f}, 0.4f, true},
  {{0.f, 1.f, 0.f, 1.f}, {24.f, 1.f, 4.f}, 0.3f, true},
  {{0.f, 1.f, 0.f, 1.f}, {32.f, 1.f, 4.f}, 0.2f, true},
  {{0.f, 1.f, 0.f, 1.f}, {40.f, 1.f, 4.f}, 0.1f, true},
  // Green, Dieletric
  {{0.f, 1.f, 0.f, 1.f}, {-40.f, 1.f, 0.f}, 1.0f, false},
  {{0.f, 1.f, 0.f, 1.f}, {-32.f, 1.f, 0.f}, 0.9f, false},
  {{0.f, 1.f, 0.f, 1.f}, {-24.f, 1.f, 0.f}, 0.8f, false},
  {{0.f, 1.f, 0.f, 1.f}, {-16.f, 1.f, 0.f}, 0.7f, false},
  {{0.f, 1.f, 0.f, 1.f}, {-8.f, 1.f, 0.f}, 0.6f, false},
  {{0.f, 1.f, 0.f, 1.f}, {8.f, 1.f, 0.f}, 0.5f, false},
  {{0.f, 1.f, 0.f, 1.f}, {16.f, 1.f, 0.f}, 0.4f, false},
  {{0.f, 1.f, 0.f, 1.f}, {24.f, 1.f, 0.f}, 0.3f, false},
  {{0.f, 1.f, 0.f, 1.f}, {32.f, 1.f, 0.f}, 0.2f, false},
  {{0.f, 1.f, 0.f, 1.f}, {40.f, 1.f, 0.f}, 0.1f, false},

  // Blue, Metal
  {{0.f, 0.f, 1.f, 1.f}, {-40.f, 1.f, -4.f}, 1.0f, true},
  {{0.f, 0.f, 1.f, 1.f}, {-32.f, 1.f, -4.f}, 0.9f, true},
  {{0.f, 0.f, 1.f, 1.f}, {-24.f, 1.f, -4.f}, 0.8f, true},
  {{0.f, 0.f, 1.f, 1.f}, {-16.f, 1.f, -4.f}, 0.7f, true},
  {{0.f, 0.f, 1.f, 1.f}, {-8.f, 1.f, -4.f}, 0.6f, true},
  {{0.f, 0.f, 1.f, 1.f}, {8.f, 1.f, -4.f}, 0.5f, true},
  {{0.f, 0.f, 1.f, 1.f}, {16.f, 1.f, -4.f}, 0.4f, true},
  {{0.f, 0.f, 1.f, 1.f}, {24.f, 1.f, -4.f}, 0.3f, true},
  {{0.f, 0.f, 1.f, 1.f}, {32.f, 1.f, -4.f}, 0.2f, true},
  {{0.f, 0.f, 1.f, 1.f}, {40.f, 1.f, -4.f}, 0.1f, true},
  // Blue, Dieletric
  {{0.f, 0.f, 1.f, 1.f}, {-40.f, 1.f, -8.f}, 1.0f, false},
  {{0.f, 0.f, 1.f, 1.f}, {-32.f, 1.f, -8.f}, 0.9f, false},
  {{0.f, 0.f, 1.f, 1.f}, {-24.f, 1.f, -8.f}, 0.8f, false},
  {{0.f, 0.f, 1.f, 1.f}, {-16.f, 1.f, -8.f}, 0.7f, false},
  {{0.f, 0.f, 1.f, 1.f}, {-8.f, 1.f, -8.f}, 0.6f, false},
  {{0.f, 0.f, 1.f, 1.f}, {8.f, 1.f, -8.f}, 0.5f, false},
  {{0.f, 0.f, 1.f, 1.f}, {16.f, 1.f, -8.f}, 0.4f, false},
  {{0.f, 0.f, 1.f, 1.f}, {24.f, 1.f, -8.f}, 0.3f, false},
  {{0.f, 0.f, 1.f, 1.f}, {32.f, 1.f, -8.f}, 0.2f, false},
  {{0.f, 0.f, 1.f, 1.f}, {40.f, 1.f, -8.f}, 0.1f, false},
};

class ExampleHelloWorld : public entry::AppI {
private:
  uint32_t          m_width;
  uint32_t          m_height;
  uint32_t          m_debug;
  uint32_t          m_reset;
  entry::MouseState mouseState;
  float             cameraPos[4] = {0};

  util::Mesh          m_mesh;
  util::Mesh          m_orb;
  util::MeshMaterials m_materials;

  bgfx::ProgramHandle logLuminanceAvProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle tonemapProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle luminancePixelProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle copyProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle skyboxProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle irConvolveProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle sirConvolveProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle bdrfConvolveProg = BGFX_INVALID_HANDLE;

  bgfx::UniformHandle timeUniform;
  bgfx::UniformHandle sunlightUniform;
  bgfx::UniformHandle lightingParamsUniform;
  bgfx::UniformHandle csViewPrams;
  bgfx::UniformHandle csLightData;
  bgfx::UniformHandle exposureParamsUniform;
  bgfx::UniformHandle offsetUniform;
  bgfx::UniformHandle albedoUnform;
  bgfx::UniformHandle metalRoughUniform;
  bgfx::UniformHandle debugUniform;
  bgfx::UniformHandle ambientColourUniform;
  bgfx::UniformHandle cameraPosUniform;

  bgfx::UniformHandle baseTex;
  bgfx::UniformHandle normalTex;
  bgfx::UniformHandle roughTex;
  bgfx::UniformHandle metalTex;
  bgfx::UniformHandle maskTex;
  bgfx::UniformHandle luminanceTex;
  bgfx::UniformHandle colourLUTTex;

  bgfx::DynamicVertexBufferHandle lightPositionBuffer;
  bgfx::DynamicVertexBufferHandle zBinBuffer;
  bgfx::DynamicVertexBufferHandle lightGridBuffer;
  bgfx::DynamicVertexBufferHandle lightListBuffer;
  bgfx::DynamicVertexBufferHandle lightGridFatBuffer;
  bgfx::DynamicVertexBufferHandle workingLuminanceBuffer;
  bgfx::DynamicVertexBufferHandle averageLuminanceBuffer;

  bgfx::FrameBufferHandle zPrePassTarget;
  bgfx::FrameBufferHandle mainTarget;
  bgfx::FrameBufferHandle finalTarget;
  bgfx::FrameBufferHandle luminanceTargets[16];

  bgfx::TextureHandle colourTarget;
  bgfx::TextureHandle finalColourTarget;
  bgfx::TextureHandle luminanceMips[16];
  bgfx::TextureHandle colourGradeLUT;
  bgfx::TextureHandle skyboxTex = BGFX_INVALID_HANDLE;
  bgfx::TextureHandle skyboxTex2 = BGFX_INVALID_HANDLE;
  bgfx::TextureHandle skyboxIrrTex = BGFX_INVALID_HANDLE;


  half_t*           colourGradeSrc;
  half_t*           colourGradeDst;
  bgfx::Caps const* m_caps;

  LightInit* lights = nullptr; //[LIGHT_COUNT];
  uint16_t*  grid = nullptr;

  uint32_t frameID = 0;
  uint32_t numLuminanceMips = 0;

  float time = 0.f;

  float sunlightAngle[4] = {0.f, 0.f, 0.f, 1.13f};
  float ambientColour[4] = {1.f, 1.f, 1.f, .35f};
  float curExposure = .18f;
  float curExposureMin = .5f;
  float curExposureMax = 5.f;

  int32_t dbgCell = 0;
  int32_t dbgBucket = 0;
  int32_t dbgShader = 2;
  int32_t dbgVisMode = 0;

  bool dbgLightGrid = false;
  bool dbgLightGridFrustum = false;
  bool dbgUpdateFrustum = true;
  bool dbgDrawLights = false;
  bool dbgDrawLightsWire = false;
  bool dbgDrawZBin = false;
  bool dbgDebugStats = false;
  bool dbgDebugMoveLights = false;
  bool dbgPixelAvgLuminance = true;
  bool dbgShowReflectionProbes = false;

  template <typename t_ty>
  void read(t_ty* dst, FILE* f) {
    fread(dst, sizeof(t_ty), 1, f);
  }

  template <typename t_ty>
  void write(FILE* f, t_ty const& src) {
    fwrite(&src, sizeof(t_ty), 1, f);
  }

  static const uint32_t saveDataMagic = 0x80808335;
  static const uint32_t saveDataVer = 1;

  void loadData() {
    FILE* f = fopen("data/save.dat", "rb");
    if (f) {
      uint32_t save_data_magic, save_data_ver;
      read(&save_data_magic, f);
      read(&save_data_ver, f);
      if (save_data_magic != saveDataMagic) return;
      if (save_data_ver > saveDataVer) return;

      for (uint32_t i = 0; i < 4; ++i)
        read(&sunlightAngle[i], f);
      for (uint32_t i = 0; i < 4; ++i)
        read(&ambientColour[i], f);
      read(&curExposure, f);
      read(&curExposureMin, f);
      read(&curExposureMax, f);
      read(&dbgLightGrid, f);
      read(&dbgLightGridFrustum, f);
      read(&dbgUpdateFrustum, f);
      read(&dbgDrawLights, f);
      read(&dbgDrawLightsWire, f);
      read(&dbgDrawZBin, f);
      read(&dbgDebugStats, f);
      read(&dbgDebugMoveLights, f);
      read(&dbgPixelAvgLuminance, f);
      read(&dbgShowReflectionProbes, f);

      read(&totalProbes, f);
      probeArray = (probe_t*)malloc(sizeof(probe_t) * totalProbes);
      for (uint32_t i = 0, n = totalProbes; i < n; ++i) {
        for (uint32_t j = 0; j < 3; ++j)
          read(&probeArray[i].position.v[j], f);
        for (uint32_t j = 0; j < 3; ++j)
          read(&probeArray[i].halfWidths.v[j], f);
      }

      fclose(f);
    }
  }

  void saveData() {
    FILE* f = fopen("data/save.dat", "wb");
    if (f) {
      write(f, saveDataMagic);
      write(f, saveDataVer);

      for (uint32_t i = 0; i < 4; ++i)
        write(f, sunlightAngle[i]);
      for (uint32_t i = 0; i < 4; ++i)
        write(f, ambientColour[i]);
      write(f, curExposure);
      write(f, curExposureMin);
      write(f, curExposureMax);
      write(f, dbgLightGrid);
      write(f, dbgLightGridFrustum);
      write(f, dbgUpdateFrustum);
      write(f, dbgDrawLights);
      write(f, dbgDrawLightsWire);
      write(f, dbgDrawZBin);
      write(f, dbgDebugStats);
      write(f, dbgDebugMoveLights);
      write(f, dbgPixelAvgLuminance);
      write(f, dbgShowReflectionProbes);

      write(f, totalProbes);
      for (uint32_t i = 0, n = totalProbes; i < n; ++i) {
        for (uint32_t j = 0; j < 3; ++j)
          write(f, probeArray[i].position.v[j]);
        for (uint32_t j = 0; j < 3; ++j)
          write(f, probeArray[i].halfWidths.v[j]);
      }

      fclose(f);
    }
  }

  void unloadAssetData(RunParams* data, uint32_t data_count) {
    for (uint32_t i = 0, n = data_count; i < n; ++i) {
      if (bgfx::isValid(data[i].solidProg)) {
        bgfx::destroy(data[i].solidProg);
        data[i].solidProg = BGFX_INVALID_HANDLE;
      }
      if (bgfx::isValid(data[i].solidProgMask)) {
        bgfx::destroy(data[i].solidProgMask);
        data[i].solidProgMask = BGFX_INVALID_HANDLE;
      }
      if (bgfx::isValid(data[i].solidProgNoTex)) {
        bgfx::destroy(data[i].solidProgNoTex);
        data[i].solidProgNoTex = BGFX_INVALID_HANDLE;
      }
      if (bgfx::isValid(data[i].fullscreenProg)) {
        bgfx::destroy(data[i].fullscreenProg);
        data[i].fullscreenProg = BGFX_INVALID_HANDLE;
      }
      if (bgfx::isValid(data[i].csLightCull)) {
        bgfx::destroy(data[i].csLightCull);
        data[i].csLightCull = BGFX_INVALID_HANDLE;
      }
    }
    if (bgfx::isValid(tonemapProg)) {
      bgfx::destroy(tonemapProg);
      tonemapProg = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(logLuminanceAvProg)) {
      bgfx::destroy(logLuminanceAvProg);
      logLuminanceAvProg = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(luminancePixelProg)) {
      bgfx::destroy(luminancePixelProg);
      luminancePixelProg = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(copyProg)) {
      bgfx::destroy(copyProg);
      copyProg = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(skyboxProg)) {
      bgfx::destroy(skyboxProg);
      skyboxProg = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(irConvolveProg)) {
      bgfx::destroy(irConvolveProg);
      irConvolveProg = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(sirConvolveProg)) {
      bgfx::destroy(sirConvolveProg);
      sirConvolveProg = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(bdrfConvolveProg)) {
      bgfx::destroy(bdrfConvolveProg);
      bdrfConvolveProg = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(skyboxTex)) {
      bgfx::destroy(skyboxTex);
      skyboxTex = BGFX_INVALID_HANDLE; 
    }
    if (bgfx::isValid(skyboxIrrTex)) {
      bgfx::destroy(skyboxIrrTex);
      skyboxIrrTex = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(skyboxTex2)) {
      bgfx::destroy(skyboxTex2);
      skyboxTex2 = BGFX_INVALID_HANDLE;
    }
  }

  void loadAssetData(RunParams* data, uint32_t data_count) {
    unloadAssetData(data, data_count);
    for (uint32_t i = 0, n = data_count; i < n; ++i) {
      if (data[i].meshVSPath && data[i].meshPSPath) {
        bgfx::Memory const* vs = loadMem(data[i].meshVSPath);
        bgfx::Memory const* ps = loadMem(data[i].meshPSPath);
        data[i].solidProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);
      }
      if (data[i].meshVSPath && data[i].meshPSMaskPath) {
        bgfx::Memory const* vs = loadMem(data[i].meshVSPath);
        bgfx::Memory const* ps = loadMem(data[i].meshPSMaskPath);
        data[i].solidProgMask = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);
      }
      if (data[i].meshVSPath && data[i].meshPSNoTexturePath) {
        bgfx::Memory const* vs = loadMem(data[i].meshVSPath);
        bgfx::Memory const* ps = loadMem(data[i].meshPSNoTexturePath);
        data[i].solidProgNoTex = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);
      }
      /*if (data[i].meshVSPath)*/ {
        bgfx::Memory const* vs = loadMem(VS_Z_FILL);
        bgfx::Memory const* ps = loadMem(PS_Z_FILL);
        data[i].zfillProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);
      }
      if (data[i].fsDbgVSPath && data[i].fsDbgPSPath) {
        bgfx::Memory const* vs = loadMem(data[i].fsDbgVSPath);
        bgfx::Memory const* ps = loadMem(data[i].fsDbgPSPath);
        data[i].fullscreenProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);
      }
      if (data[i].lightCullCSPath) {
        bgfx::Memory const* cs = loadMem(data[i].lightCullCSPath);
        data[i].csLightCull = bgfx::createProgram(bgfx::createShader(cs), true);
      }
    }

    bgfx::Memory const* vs = loadMem(VS_DEBUG_ASSET_PATH);
    bgfx::Memory const* ps = loadMem(TONEMAP_ASSET_PATH);
    tonemapProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);

    vs = loadMem(VS_DEBUG_ASSET_PATH);
    ps = loadMem(FS_LUMINANCE_AVG_ASSET_PATH);
    luminancePixelProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);

    vs = loadMem(VS_DEBUG_ASSET_PATH);
    ps = loadMem(FS_COPY_ASSET_PATH);
    copyProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);

    vs = loadMem(VS_SKYBOX_ASSET_PATH);
    ps = loadMem(FS_SKYBOX_ASSET_PATH);
    skyboxProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);

    bgfx::Memory const* cs = loadMem(LUMINANCE_AVG_ASSET_PATH);
    logLuminanceAvProg = bgfx::createProgram(bgfx::createShader(cs), true);

    vs = loadMem(VS_DEBUG_ASSET_PATH);
    ps = loadMem(FS_IR_CONVOLVE_ASSET_PATH);
    irConvolveProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);

    vs = loadMem(VS_DEBUG_ASSET_PATH);
    ps = loadMem(FS_SIR_CONVOLVE_ASSET_PATH);
    sirConvolveProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);

    vs = loadMem(VS_DEBUG_ASSET_PATH);
    ps = loadMem(FS_BRDF_CONVOLVE_ASSET_PATH);
    bdrfConvolveProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);

    //bgfx::Memory const* cm_mem = loadMem(SKYBOX_ASSET_PATH);
    //skyboxTex = loadTexture(cm_mem->data, cm_mem->size, "EnvMap");
    skyboxTex = loadTexture(SKYBOX_ASSET_PATH);
    skyboxIrrTex = loadTexture(SKYBOX_IRR_ASSET_PATH);

    int x, y, n;
    float* hdr_data = stbi_loadf(HDR_SKYBOX_ASSET_PATH, &x, &y, &n, 3);
    half_t* hhdr = (half_t*)malloc(x*y*sizeof(half_t)*4);
    half_t* dest = hhdr;
    float* src = hdr_data;
    for (int i=0; i < x; ++i) {
      for (int j = 0; j < y; ++j) {
        float_to_half(dest++, *src); src++;
        float_to_half(dest++, *src); src++;
        float_to_half(dest++, *src); src++;
        // Skip Alpha 
        ++dest;
      }
    }

    skyboxTex2 = bgfx::createTexture2D(x, y, false, 1, bgfx::TextureFormat::RGBA16F, 0, bgfx::copy(hhdr, x*y*sizeof(half_t)*4));

    stbi_image_free(hdr_data);
    free(hhdr);
  }

  void renderCubemapInfluenceFaces() {
    // for each face
    //  build list of intersecting probes (up to 4)
    //   render probe influence (using https://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/)
  }

  void createIRCubemapArrays(uint32_t             map_dim[3],
                             bgfx::TextureHandle* ir_cma,
                             bgfx::TextureHandle* sir_cma,
                             bgfx::TextureHandle* bgfx_th) {
    char     tmp_fn[1024];
    uint32_t total_cms = map_dim[2] * map_dim[1] * map_dim[0];
    uint32_t sir_mip_size[SIR_MIP_COUNT];
    uint32_t sir_mip_write_offset[6][SIR_MIP_COUNT];
    uint32_t ir_cm_size = IR_MAP_DIM * IR_MAP_DIM * 6 * sizeof(half_t) * 4;
    uint32_t sir_total_size = 0;
    for (uint32_t i = 0; i < SIR_MIP_COUNT; ++i) {
      sir_mip_size[i] = (SIR_MAP_DIM >> i) * (SIR_MAP_DIM >> i) * sizeof(half_t) * 4;
      sir_total_size += sir_mip_size[i] * 6;
    }

    uint32_t tmp_offset = 0;
    for (uint32_t i = 0; i < 6; ++i) {
      for (uint32_t j = 0; j < SIR_MIP_COUNT; ++j) {
        sir_mip_write_offset[i][j] = tmp_offset;
        tmp_offset += sir_mip_size[j];
      }
    }

    bgfx::Memory const* ir_temp_alloc = bgfx::alloc(total_cms * IR_MAP_DIM * IR_MAP_DIM * 6 * sizeof(half_t) * 4);
    bgfx::Memory const* sir_temp_alloc = bgfx::alloc(total_cms * sir_total_size);
    bgfx::Memory const* bdrf_temp_alloc = bgfx::alloc(BDRF_DIM * BDRF_DIM * sizeof(half_t) * 2);
    void* ir_temp_mem = ir_temp_alloc->data;   // malloc(total_cms*IR_MAP_DIM*IR_MAP_DIM*6*sizeof(half_t)*4);
    void* sir_temp_mem = sir_temp_alloc->data; // malloc(total_cms*sir_total_size);
    void* bdrf_temp_mem = bdrf_temp_alloc->data;

    // zero it all, incase nothing can be read
    memset(ir_temp_mem, 0x0, total_cms * IR_MAP_DIM * IR_MAP_DIM * 6 * sizeof(half_t) * 4);
    memset(sir_temp_mem, 0x0, total_cms * sir_total_size);

    for (uint32_t z = 0, zn = map_dim[2]; z < zn; ++z) {
      for (uint32_t y = 0, yn = map_dim[1]; y < yn; ++y) {
        for (uint32_t x = 0, xn = map_dim[0]; x < xn; ++x) {
          uint32_t cm_idx = (z * yn * xn) + (y * xn) + x;
          sprintf(tmp_fn, "data/env/%dx%dx%d_ir.ktx", x, y, z);
          FILE* f = fopen(tmp_fn, "rb");
          if (f) {
            // seek past the header and image size
            fseek(f, sizeof ktxheader_t + sizeof uint32_t, SEEK_CUR);
            fread(((uint8_t*)ir_temp_mem) + cm_idx * ir_cm_size, 1, ir_cm_size, f);
            fclose(f);
          }
          sprintf(tmp_fn, "data/env/%dx%dx%d_sir.ktx", x, y, z);
          f = fopen(tmp_fn, "rb");
          if (f) {
            uint8_t* mip_ptr = ((uint8_t*)sir_temp_mem) + cm_idx * sir_total_size;
            fseek(f, sizeof ktxheader_t, SEEK_CUR);
            for (uint32_t m = 0, mn = SIR_MIP_COUNT; m < mn; ++m) {
              // skip mip size
              fseek(f, sizeof uint32_t, SEEK_CUR);
              for (uint32_t j = 0; j < 6; ++j) {
                // read the mip data
                fread(mip_ptr + sir_mip_write_offset[j][m], 1, sir_mip_size[m], f);
              }
              // mip_ptr += sir_mip_size[m];
            }
          }
          printf("%dx%dx%d - index = %d\n", x, y, z, cm_idx);
        }
      }
    }

    FILE* f = fopen("data/env/bdrf.ktx", "rb");
    if (f) {
      fseek(f, sizeof ktxheader_t + sizeof uint32_t, SEEK_CUR);
      fread(bdrf_temp_mem, 1, BDRF_DIM * BDRF_DIM * sizeof(half_t) * 2, f);
      fclose(f);
    }

    // create our textures
    *ir_cma = bgfx::createTextureCube(
      IR_MAP_DIM, false, total_cms, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_NONE/*, ir_temp_alloc*/);
    *sir_cma = bgfx::createTextureCube(
      SIR_MAP_DIM, true, total_cms, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_NONE/*, sir_temp_alloc*/);
    *bgfx_th = bgfx::createTexture2D(
      BDRF_DIM, BDRF_DIM, false, 1, bgfx::TextureFormat::RG16F, BGFX_TEXTURE_NONE, bdrf_temp_alloc);

    // free(ir_temp_mem);
    // free(sir_temp_mem);
  }

  void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) {
    Args args(_argc, _argv);

    cameraCreate();

    float cameraP[3] = {0.f, 0.f, -2.f};
    cameraSetPosition(cameraP);

    PosTexCoord0Vertex::init();

    m_width = _width;   // == ENTRY_DEFAULT_WIDTH; // This can't be changed
    m_height = _height; // == ENTRY_DEFAULT_HEIGHT; // This can't be changed
    m_debug = BGFX_DEBUG_TEXT;
    m_reset = BGFX_RESET_NONE; // BGFX_RESET_VSYNC;

    bgfx::init(bgfx::RendererType::Direct3D11, args.m_pciId);
    bgfx::reset(m_width, m_height, m_reset);

    const bgfx::RendererType::Enum renderer = bgfx::getRendererType();
    s_texelHalf = bgfx::RendererType::Direct3D9 == renderer ? 0.5f : 0.0f;

    // Get renderer capabilities info.
    m_caps = bgfx::getCaps();

    // Enable debug text.
    bgfx::setDebug(m_debug);

    bx::FileReaderI* reader = entry::getFileReader();
    if (bx::open(reader, SPONZA_MESH_ASSET_PATH)) {
      util::mesh_load(&m_mesh, reader);
      bx::close(reader);
    }
    if (bx::open(reader, ORB_MESH_ASSET_PATH)) {
      util::mesh_load(&m_orb, reader);
      bx::close(reader);
    }
    if (bx::open(reader, SPONZA_MATERIAL_ASSET_PATH)) {
      util::material_load(&m_materials, reader);
    }
    mesh_bind_materials(&m_mesh, &m_materials);

    loadAssetData(ShaderParams, SOLID_PROGS);

    for (size_t i = 0, n = m_mesh.m_groups.size(); i < n; ++i) {
      for (uint32_t j = 0; j < 3; ++j) {
        totalBoundsMin[j] = MIN(m_mesh.m_groups[i].m_aabb.m_min[j] * GLOBAL_SCALE, totalBoundsMin[j]);
        totalBoundsMax[j] = MAX(m_mesh.m_groups[i].m_aabb.m_max[j] * GLOBAL_SCALE, totalBoundsMax[j]);
      }
    }

    for (uint32_t i = 0; i < 3; ++i) {
      envMapGridDim[i] =
        (uint32_t)(((totalBoundsMax[i] - totalBoundsMin[i]) + (ENV_MAP_GRID_DIM - 1.f)) / ENV_MAP_GRID_DIM);
    }
    uint32_t total_grid_count = envMapGridDim[0] * envMapGridDim[1] * envMapGridDim[2];

    colourGradeSrc = new half_t[COLOUR_LUT_DIM * COLOUR_LUT_DIM * COLOUR_LUT_DIM * 4];
    colourGradeDst = new half_t[COLOUR_LUT_DIM * COLOUR_LUT_DIM * COLOUR_LUT_DIM * 4];

    uint32_t stride = (uint32_t)(sizeof(half_t) * 4);
    for (uint32_t z = 0; z < COLOUR_LUT_DIM; ++z) {
      for (uint32_t y = 0; y < COLOUR_LUT_DIM; ++y) {
        for (uint32_t x = 0; x < COLOUR_LUT_DIM; ++x) {
          uint32_t idx = (z * COLOUR_LUT_DIM * COLOUR_LUT_DIM * 4) + (y * COLOUR_LUT_DIM * 4) + x * 4;
          colourGradeSrc[idx + 0] = util::float_to_half(CLAMP((float)x / (float)COLOUR_LUT_DIM, 0.f, 1.f));
          colourGradeSrc[idx + 1] = util::float_to_half(CLAMP((float)y / (float)COLOUR_LUT_DIM, 0.f, 1.f));
          colourGradeSrc[idx + 2] = util::float_to_half(CLAMP((float)z / (float)COLOUR_LUT_DIM, 0.f, 1.f));
          colourGradeSrc[idx + 3] = util::float_to_half(1.0f);
        }
      }
    }

    memcpy(colourGradeDst, colourGradeSrc, COLOUR_LUT_DIM * COLOUR_LUT_DIM * COLOUR_LUT_DIM * stride);
    colourGradeLUT =
      bgfx::createTexture3D(COLOUR_LUT_DIM,
                            COLOUR_LUT_DIM,
                            COLOUR_LUT_DIM,
                            false,
                            bgfx::TextureFormat::RGBA16F,
                            BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP | BGFX_TEXTURE_W_CLAMP,
                            bgfx::copy(colourGradeDst, COLOUR_LUT_DIM * COLOUR_LUT_DIM * COLOUR_LUT_DIM * stride));

    timeUniform = bgfx::createUniform("u_gameTime", bgfx::UniformType::Vec4);
    sunlightUniform = bgfx::createUniform("u_sunlight", bgfx::UniformType::Vec4);
    lightingParamsUniform = bgfx::createUniform("u_bucketParams", bgfx::UniformType::Vec4);
    csViewPrams = bgfx::createUniform("u_params", bgfx::UniformType::Vec4, 2);
    csLightData = bgfx::createUniform("u_lightData", bgfx::UniformType::Vec4);
    exposureParamsUniform = bgfx::createUniform("u_exposureParams", bgfx::UniformType::Vec4);
    offsetUniform = bgfx::createUniform("u_offset", bgfx::UniformType::Vec4, 3);
    albedoUnform = bgfx::createUniform("u_albedo", bgfx::UniformType::Vec4);
    metalRoughUniform = bgfx::createUniform("u_metalrough", bgfx::UniformType::Vec4);
    debugUniform = bgfx::createUniform("u_debugMode", bgfx::UniformType::Vec4);
    ambientColourUniform = bgfx::createUniform("u_ambientColour", bgfx::UniformType::Vec4);
    dynLightsUniform = bgfx::createUniform("u_dynamicLights", bgfx::UniformType::Vec4);
    cubeFaceUniform = bgfx::createUniform("u_cubeFace", bgfx::UniformType::Vec4);
    gridSizeUniform = bgfx::createUniform("u_gridParams", bgfx::UniformType::Vec4);
    gridOffsetUniform = bgfx::createUniform("u_gridOffset", bgfx::UniformType::Vec4);
    cameraPosUniform = bgfx::createUniform("u_inverseProj", bgfx::UniformType::Mat4);

    baseTex = bgfx::createUniform("s_base", bgfx::UniformType::Int1);
    normalTex = bgfx::createUniform("s_normal", bgfx::UniformType::Int1);
    roughTex = bgfx::createUniform("s_roughness", bgfx::UniformType::Int1);
    metalTex = bgfx::createUniform("s_metallic", bgfx::UniformType::Int1);
    maskTex = bgfx::createUniform("s_mask", bgfx::UniformType::Int1);
    luminanceTex = bgfx::createUniform("s_luminance", bgfx::UniformType::Int1);
    colourLUTTex = bgfx::createUniform("s_colourLUT", bgfx::UniformType::Int1);
    environmentMapTex = bgfx::createUniform("s_environmentMap", bgfx::UniformType::Int1);
    irradianceMapTex = bgfx::createUniform("s_irradianceMap", bgfx::UniformType::Int1);
    irradianceAraryTex = bgfx::createUniform("s_irradiance", bgfx::UniformType::Int1);
    specIrradianceAraryTex = bgfx::createUniform("s_specIrradiance", bgfx::UniformType::Int1);
    bdrfTex = bgfx::createUniform("s_bdrfLUT", bgfx::UniformType::Int1);


    lights = new LightInit[LIGHT_COUNT];
    float  scale = 4.f;
    vec3_t colours[7] = {
      {1.f * scale, 0.f * scale, 0.f * scale},
      {0.f * scale, 1.f * scale, 0.f * scale},
      {1.f * scale, 1.f * scale, 0.f * scale},
      {0.f * scale, 0.f * scale, 1.f * scale},
      {1.f * scale, 0.f * scale, 1.f * scale},
      {0.f * scale, 1.f * scale, 1.f * scale},
      {1.f * scale, 1.f * scale, 1.f * scale},
    };
    float range[3] = {
      (totalBoundsMax[0] - totalBoundsMin[0]),
      (totalBoundsMax[1] - totalBoundsMin[1]),
      (totalBoundsMax[2] - totalBoundsMin[2]),
    };
    for (uint32_t i = 0; i < LIGHT_COUNT; ++i) {
      lights[i].p.x = (totalBoundsMin[0]) + ((float)rand() / RAND_MAX) * range[0];
      lights[i].p.y = (totalBoundsMin[1]) + ((float)rand() / RAND_MAX) * range[1];
      lights[i].p.z = (totalBoundsMin[2]) + ((float)rand() / RAND_MAX) * range[2];
      lights[i].radius = (((float)rand() / RAND_MAX) * 10.f) + 10.f;
      lights[i].col = colours[rand() % 7];
      lights[i].r[0] = (((float)rand() / RAND_MAX) * 5.f) + 5.f;
      lights[i].r[1] = (((float)rand() / RAND_MAX) * 5.f) + 5.f;
      lights[i].r[2] = (((float)rand() / RAND_MAX) * 5.f) + 5.f;
    }

    // Setup compute buffers
    bgfx::VertexDecl lightVertexDecl;
    lightVertexDecl.begin().add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float).end();
    bgfx::VertexDecl zBinVertexDecl;
    zBinVertexDecl.begin().add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16).end();
    bgfx::VertexDecl lightIndexVertexDecl;
    lightIndexVertexDecl.begin().add(bgfx::Attrib::TexCoord0, 1, bgfx::AttribType::Int16).end();

    // Magic numbers for structure sizes.
    lightPositionBuffer = bgfx::createDynamicVertexBuffer(LIGHT_COUNT * 2, lightVertexDecl, BGFX_BUFFER_COMPUTE_READ);
    zBinBuffer = bgfx::createDynamicVertexBuffer(Z_BUCKETS * sizeof(uint16_t) * 2,
                                                 zBinVertexDecl,
                                                 BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_COMPUTE_FORMAT_16x2 |
                                                   BGFX_BUFFER_COMPUTE_TYPE_UINT);
    lightListBuffer = bgfx::createDynamicVertexBuffer(LIGHT_COUNT * LIGHT_COUNT * sizeof(uint16_t),
                                                      lightIndexVertexDecl,
                                                      BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_COMPUTE_FORMAT_16x1 |
                                                        BGFX_BUFFER_COMPUTE_TYPE_UINT);

    // Each grid cell can have upto MAX_LIGHT_GRID_COUNT lights
    lightGridFatBuffer = bgfx::createDynamicVertexBuffer(
      LIGHT_GRID_SPACE * MAX_LIGHT_GRID_COUNT,
      lightIndexVertexDecl,
      BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_COMPUTE_FORMAT_16x1 | BGFX_BUFFER_COMPUTE_TYPE_UINT);
    lightGridBuffer = bgfx::createDynamicVertexBuffer(LIGHT_GRID_SPACE * MAX_LIGHT_GRID_COUNT,
                                                      lightIndexVertexDecl,
                                                      BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_COMPUTE_FORMAT_16x1 |
                                                        BGFX_BUFFER_COMPUTE_TYPE_UINT);

    grid = new uint16_t[LIGHT_GRID_SPACE * sizeof(uint16_t) * MAX_LIGHT_GRID_COUNT];

    zPrePassTarget = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Equal, bgfx::TextureFormat::D32F);
    // BGFX can't handle sRGB render targets, so use a 16bit float and resolve out end frame
    colourTarget =
      bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT);
    bgfx::Attachment main_attachments[2] = {{colourTarget, 0, 0}, {bgfx::getTexture(zPrePassTarget, 0), 0, 0}};
    mainTarget = bgfx::createFrameBuffer(2, main_attachments);

    finalColourTarget =
      bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT);
    bgfx::Attachment final_attachments[2] = {{finalColourTarget, 0, 0}, {bgfx::getTexture(zPrePassTarget, 0), 0, 0}};
    finalTarget = bgfx::createFrameBuffer(2, final_attachments);

    bgfx::VertexDecl floatVertexDecl;
    floatVertexDecl.begin().add(bgfx::Attrib::TexCoord0, 1, bgfx::AttribType::Float).end();

    uint32_t luminanceGroupdW = (m_width + SMALL_DISPATCH_WAVE - 1) / SMALL_DISPATCH_WAVE;
    uint32_t luminanceGroupdH = (m_height + SMALL_DISPATCH_WAVE - 1) / SMALL_DISPATCH_WAVE;
    workingLuminanceBuffer = bgfx::createDynamicVertexBuffer(
      luminanceGroupdW * luminanceGroupdH,
      floatVertexDecl,
      BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_COMPUTE_FORMAT_32x1 | BGFX_BUFFER_COMPUTE_TYPE_FLOAT);
    averageLuminanceBuffer = bgfx::createDynamicVertexBuffer(
      1,
      floatVertexDecl,
      BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_COMPUTE_FORMAT_32x1 | BGFX_BUFFER_COMPUTE_TYPE_FLOAT);

    bdrfConvolveTexture =
      bgfx::createTexture2D(BDRF_DIM, BDRF_DIM, false, 1, bgfx::TextureFormat::RG16F, BGFX_TEXTURE_RT);
    bdrfConvolveTextureRead = bgfx::createTexture2D(
      BDRF_DIM, BDRF_DIM, false, 1, bgfx::TextureFormat::RG16F, BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);
    bdrfConvolveTarget = bgfx::createFrameBuffer(1, &bdrfConvolveTexture);

    numLuminanceMips = 0;
    uint32_t luminance_w = m_width / 2, luminance_h = m_height / 2;

    while (luminance_w >= 1 || luminance_h >= 1) {
      luminanceMips[numLuminanceMips] =
        bgfx::createTexture2D(luminance_w, luminance_h, false, 1, bgfx::TextureFormat::R16F, BGFX_TEXTURE_RT);
      luminanceTargets[numLuminanceMips++] = bgfx::createFrameBuffer(1, luminanceMips + numLuminanceMips);
      if (luminance_w == 1 && luminance_h == 1) break;
      if (luminance_w > 1) luminance_w /= 2;
      if (luminance_h > 1) luminance_h /= 2;
    }

    bgfx::Memory const* mask_data_mem = bgfx::alloc(1);
    mask_data_mem->data[0] = 255;
    defaultMask = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::R8, BGFX_TEXTURE_NONE, mask_data_mem);

    // Create cubemap framebuffers
    cubemapDepthTexture = bgfx::createTexture2D(m_width, m_width, false, 1, bgfx::TextureFormat::D32F, BGFX_TEXTURE_RT);
    cubemapFaceTexture =
      bgfx::createTextureCube(m_width,
                              false,
                              1,
                              bgfx::TextureFormat::RGBA16F,
                              BGFX_TEXTURE_RT | BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP | BGFX_TEXTURE_W_CLAMP);
    irCubemapFaceTexture = bgfx::createTextureCube(IR_MAP_DIM, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT);
    for (uint32_t i = 0; i < 6; ++i) {
      bgfx::Attachment cm_att[2] = {{cubemapFaceTexture, 0, (uint16_t)i}, {cubemapDepthTexture, 0, 0}};
      cubemapFaceTarget[i] = bgfx::createFrameBuffer(2, cm_att);
      bgfx::Attachment ir_cm_att[2] = {{irCubemapFaceTexture, 0, (uint16_t)i}};
      irCubemapFaceTarget[i] = bgfx::createFrameBuffer(1, ir_cm_att);

      cubemapFaceTextureRead[i] = bgfx::createTexture2D(
        m_width, m_width, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);
      irCubemapFaceTextureRead[i] = bgfx::createTexture2D(
        IR_MAP_DIM, IR_MAP_DIM, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);

      for (uint32_t j = 0; j < SIR_MIP_COUNT; ++j) {
        if (i == 0)
          sirCubemapFaceTexture[j] =
            bgfx::createTextureCube(SIR_MAP_DIM >> j, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT);
        bgfx::Attachment sir_cm_att[2] = {{sirCubemapFaceTexture[j], (uint16_t)0, (uint16_t)i}};
        sirCubemapFaceTarget[i][j] = bgfx::createFrameBuffer(1, sir_cm_att);
        sirCubemapFaceTextureRead[i][j] = bgfx::createTexture2D(SIR_MAP_DIM >> j,
                                                                SIR_MAP_DIM >> j,
                                                                false,
                                                                1,
                                                                bgfx::TextureFormat::RGBA16F,
                                                                BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);
      }
    }

    // Set view 0 clear state.
    bgfx::setViewClear(RENDER_PASS_COMPUTE, BGFX_CLEAR_NONE, 0x303030ff, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_COMPUTE, "Compute Pass");
    bgfx::setViewMode(RENDER_PASS_COMPUTE, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(RENDER_PASS_ZPREPASS, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_ZPREPASS, "Z PrePass");
    bgfx::setViewMode(RENDER_PASS_ZPREPASS, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(RENDER_PASS_SOLID, BGFX_CLEAR_COLOR, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_SOLID, "Solid Pass");
    bgfx::setViewMode(RENDER_PASS_SOLID, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(RENDER_PASS_SKYBOX, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_SKYBOX, "Skybox Pass");
    bgfx::setViewMode(RENDER_PASS_SKYBOX, bgfx::ViewMode::Sequential);

    for (uint32_t i = 0; i < numLuminanceMips; ++i) {
      bgfx::setViewClear(RENDER_PASS_LUMINANCE_START + i, BGFX_CLEAR_NONE, 0, 1.0f, 0);
      bgfx::setViewName(RENDER_PASS_LUMINANCE_START + i, "Luminance Pass");
      bgfx::setViewMode(RENDER_PASS_LUMINANCE_START + i, bgfx::ViewMode::Sequential);
    }

    bgfx::setViewClear(RENDER_PASS_TONEMAP, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_TONEMAP, "Tonemap Pass");
    bgfx::setViewMode(RENDER_PASS_TONEMAP, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(RENDER_PASS_DEBUG, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_DEBUG, "Debug Pass");
    bgfx::setViewMode(RENDER_PASS_DEBUG, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(RENDER_POST_DEBUG_BLIT, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_POST_DEBUG_BLIT, "Post Debug Blit Pass");
    bgfx::setViewMode(RENDER_POST_DEBUG_BLIT, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(RENDER_PASS_2DDEBUG, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_2DDEBUG, "2D Debug Pass");
    bgfx::setViewMode(RENDER_PASS_2DDEBUG, bgfx::ViewMode::Sequential);

    static char const* pass_names[6] = {
      "Cubemap Face +X", "Cubemap Face -X", "Cubemap Face +Y", "Cubemap Face -Y", "Cubemap Face +Z", "Cubemap Face -Z",
    };
    static char const* ir_pass_names[6] = {
      "Ir Cubemap Conv +X",
      "Ir Cubemap Conv -X",
      "Ir Cubemap Conv +Y",
      "Ir Cubemap Conv -Y",
      "Ir Cubemap Conv +Z",
      "Ir Cubemap Conv -Z",
    };
    static char const* face_names[6] = {
      "+X", "-X", "+Y", "-Y", "+Z", "-Z",
    };
    for (uint32_t i = 0; i < 6; ++i) {
      bgfx::setViewClear(RENDER_PASS_CUBEMAP_FACE_PX + i, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
      bgfx::setViewName(RENDER_PASS_CUBEMAP_FACE_PX + i, pass_names[i]);
      bgfx::setViewMode(RENDER_PASS_CUBEMAP_FACE_PX + i, bgfx::ViewMode::Sequential);

      bgfx::setViewClear(RENDER_PASS_CUBEMAP_IR_CONV_FACE_PX + i, BGFX_CLEAR_NONE, 0, 1.0f, 0);
      bgfx::setViewName(RENDER_PASS_CUBEMAP_IR_CONV_FACE_PX + i, ir_pass_names[i]);
      bgfx::setViewMode(RENDER_PASS_CUBEMAP_IR_CONV_FACE_PX + i, bgfx::ViewMode::Sequential);

      for (uint32_t j = 0; j < SIR_MIP_COUNT; ++j) {
        char pass_name[1024];
        sprintf(pass_name, "Sir Cubemap Conv %s, mip %d", face_names[i], j);
        bgfx::setViewClear(RENDER_PASS_CUBEMAP_SIR_CONV_FIRST + (i * SIR_MIP_COUNT) + j, BGFX_CLEAR_NONE, 0, 1.0f, 0);
        bgfx::setViewName(RENDER_PASS_CUBEMAP_SIR_CONV_FIRST + (i * SIR_MIP_COUNT) + j, pass_name);
        bgfx::setViewMode(RENDER_PASS_CUBEMAP_SIR_CONV_FIRST + (i * SIR_MIP_COUNT) + j, bgfx::ViewMode::Sequential);
        sirMipLevelSizeBytes[j] = (SIR_MAP_DIM >> j) * (SIR_MAP_DIM >> j) * sizeof(half_t) * 4;
      }
    }

    sirTotalWriteSize = 0;
    for (uint32_t i = 0; i < SIR_MIP_COUNT; ++i) {
      sirTotalWriteSize += 4; // for the 4 bytes of size
      for (uint32_t j = 0; j < 6; ++j) {
        sirMipFaceWriteOffsetLookUp[i][j] = sirTotalWriteSize;
        sirTotalWriteSize += (sirMipLevelSizeBytes[i] + 3) & ~3; // to pad out to 4 bytes
      }
    }

    bgfx::setViewClear(RENDER_PASS_BDRF_CONVOLVE, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_BDRF_CONVOLVE, "BDRF Convolve Pass");
    bgfx::setViewMode(RENDER_PASS_BDRF_CONVOLVE, bgfx::ViewMode::Sequential);

    bgfx::setViewClear(RENDER_PASS_GPU_COPY_LAST, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_GPU_COPY_LAST, "GPU Copies");
    bgfx::setViewMode(RENDER_PASS_GPU_COPY_LAST, bgfx::ViewMode::Sequential);

    ddInit();
    imguiCreate();

    loadData();

    createIRCubemapArrays(envMapGridDim, &irCubemapArrayTexture, &specIRCubemapArrayTexture, &bdrfTexture);

    // IR_MAP_DIMxIR_MAP_DIMxIR_MAP_DIM of RGBA 16bit float data
    irCubemapFaceTextureReadCPUData = malloc(IR_MAP_DIM * IR_MAP_DIM * 6 * sizeof(half_t) * 4);
    cubemapFaceTextureReadCPUData = malloc(m_width * m_width * 6 * sizeof(half_t) * 4);
    sirCubemapFaceTextureReadCPUData = malloc(sirTotalWriteSize);
    bdrfTextureReadCPUData = malloc(BDRF_DIM * BDRF_DIM * sizeof(half_t) * 2);
  }

  virtual int shutdown() {
    imguiDestroy();
    cameraDestroy();

    bgfx::destroy(timeUniform);
    bgfx::destroy(lightingParamsUniform);
    bgfx::destroy(csViewPrams);
    bgfx::destroy(csLightData);
    bgfx::destroy(baseTex);
    bgfx::destroy(normalTex);
    bgfx::destroy(roughTex);
    bgfx::destroy(metalTex);
    bgfx::destroy(maskTex);
    bgfx::destroy(lightPositionBuffer);
    bgfx::destroy(zBinBuffer);
    bgfx::destroy(lightGridBuffer);
    bgfx::destroy(lightListBuffer);
    bgfx::destroy(lightGridFatBuffer);
    bgfx::destroy(workingLuminanceBuffer);
    bgfx::destroy(averageLuminanceBuffer);

    bgfx::destroy(zPrePassTarget);
    bgfx::destroy(mainTarget);

    bgfx::destroy(colourTarget);

    for (uint32_t i = 0; i < numLuminanceMips; ++i) {
      bgfx::destroy(luminanceTargets[i]);
      bgfx::destroy(luminanceMips[i]);
    }

    util::mesh_unload(&m_mesh);
    util::material_unload(&m_materials);
    unloadAssetData(ShaderParams, SOLID_PROGS);

    saveData();

    // Shutdown bgfx.
    bgfx::shutdown();

    return 0;
  }

  void zbinLights(LightInfo*          viewSpaceLights,
                  CameraParams const* cam,
                  LightInit*          lights,
                  uint32_t            light_count,
                  uint32_t*           zbin,
                  float*              zstep) {
    rmt_ScopedCPUSample(zBin, 0);
    // need linear depth
    float rot_mtx[16];
    if (dbgDebugMoveLights) {
      for (uint32_t i = 0, n = light_count; i < n; ++i) {
        vec3_t t = {0.f, 1.f, 0.f}, t2;
        bx::mtxRotateXYZ(rot_mtx, lights[i].r[0] + time, lights[i].r[1] + time, lights[i].r[2] + time);
        bx::vec3MulMtx(t2.v, t.v, rot_mtx);
        vec3_add(&t, &lights[i].p, &t2);
        bx::vec3MulMtx(viewSpaceLights[i].p.v, t.v, cam->view);
        viewSpaceLights[i].radius = lights[i].radius;
        viewSpaceLights[i].col = lights[i].col;
        viewSpaceLights[i].pad = 0;
      }
    } else {
      for (uint32_t i = 0, n = light_count; i < n; ++i) {
        bx::vec3MulMtx(viewSpaceLights[i].p.v, lights[i].p.v, cam->view);
        viewSpaceLights[i].radius = lights[i].radius;
        viewSpaceLights[i].col = lights[i].col;
      }
    }
    qsort(viewSpaceLights, light_count, sizeof(LightInfo), [](void const* vlhs, void const* vrhs) {
      LightInfo* lhs = (LightInfo*)vlhs;
      LightInfo* rhs = (LightInfo*)vrhs;
      // View space forward is down the +Z
      return lhs->p.z < rhs->p.z ? -1 : 1;
    });
    // get min & max depth
    float z_depth_min = MAX(viewSpaceLights[0].p.z, 0.f), z_depth_max = viewSpaceLights[light_count - 1].p.z;
    float z_range = z_depth_max;
    float z_step = z_range / Z_BUCKETS;
    float z_step_2 = z_step * .5f;
    *zstep = z_step;
    float z_itr = 0.f, z_itr2 = z_step;

    for (uint32_t b = 0, bn = Z_BUCKETS; b < bn; ++b) {
      zbin[b] = (Z_INVALID_BUCKET);
    }

    // Note that low & high are swapped. shaders appears to read them as (low word, high word)
    uint32_t cur_l = 0, end_l = light_count;
    for (; cur_l < end_l; ++cur_l) {
      if (viewSpaceLights[cur_l].p.z + viewSpaceLights[cur_l].radius < 0.f) continue;
      int32_t min_z =
        (int32_t)MIN(fabsf((viewSpaceLights[cur_l].p.z - viewSpaceLights[cur_l].radius) / z_step), Z_BUCKETS - 1);
      int32_t max_z =
        (int32_t)MIN(fabsf((viewSpaceLights[cur_l].p.z + viewSpaceLights[cur_l].radius) / z_step), Z_BUCKETS - 1);
      for (int32_t cz = min_z; cz <= max_z; ++cz) {
        if (cz < 0) continue;
        uint16_t hiword = (zbin[cz] & 0xFFFF0000) >> 16;
        uint16_t loword = zbin[cz] & 0xFFFF;
        if (hiword < cur_l) hiword = cur_l;
        if (loword > cur_l) loword = cur_l;
        zbin[cz] = ((hiword & 0xffff) << 16) | (loword & 0xffff);
      }
    }
  }

  void extractFrustumPlanes(CameraParams const* cam, plane_t frustum[6]) {
    vec3_t cam_pos = {cam->x, cam->y, cam->z};
    vec3_t vnear, vfar, vnup, vnright, vfup, vfright;

    // get the half widths for our near and far planes
    float vn_height = tanf(deg_to_rad(cam->fovy * .5f)) * cam->nearPlane;
    float vn_width = vn_height * cam->aspect;

    float vf_height = tanf(deg_to_rad(cam->fovy * .5f)) * cam->farPlane;
    float vf_width = vf_height * cam->aspect;

    vec3_scale(&vfar, &cam->forward, cam->farPlane);
    vec3_scale(&vnear, &cam->forward, cam->nearPlane);

    vec3_add(&vnear, &cam_pos);
    vec3_add(&vfar, &cam_pos);

    vec3_scale(&vnup, &cam->up, vn_height);
    vec3_scale(&vnright, &cam->right, vn_width);

    vec3_scale(&vfup, &cam->up, vf_height);
    vec3_scale(&vfright, &cam->right, vf_width);

    vec3_t ntl, ntr, nbl, nbr;
    vec3_t ftl, ftr, fbl, fbr;

    vec3_add(&ntl, &vnear, &vnup);
    vec3_sub(&ntl, &vnright);
    vec3_add(&ntr, &vnear, &vnup);
    vec3_add(&ntr, &vnright);
    vec3_sub(&nbl, &vnear, &vnup);
    vec3_sub(&nbl, &vnright);
    vec3_sub(&nbr, &vnear, &vnup);
    vec3_add(&nbr, &vnright);

    vec3_add(&ftl, &vfar, &vfup);
    vec3_sub(&ftl, &vfright);
    vec3_add(&ftr, &vfar, &vfup);
    vec3_add(&ftr, &vfright);
    vec3_sub(&fbl, &vfar, &vfup);
    vec3_sub(&fbl, &vfright);
    vec3_sub(&fbr, &vfar, &vfup);
    vec3_add(&fbr, &vfright);

    // for each cell, build a custom frustum to test against //CW
    plane_build(frustum + 0, &ntr, &ntl, &nbl); // near
    plane_build(frustum + 1, &ftl, &ftr, &fbl); // far
    plane_build(frustum + 2, &ftl, &nbl, &ntl); // left
    plane_build(frustum + 3, &ftr, &ntr, &nbr); // right
    plane_build(frustum + 4, &ntr, &ftr, &ftl); // top
    plane_build(frustum + 5, &nbr, &fbl, &fbr); // bottom
  }

  void updateLightingGrid(LightInfo*          lights,
                          uint32_t            light_count,
                          CameraParams const* cam,
                          uint16_t*           grid,
                          uint16_t*           debugLightList) {
    rmt_ScopedCPUSample(UpdateLightingGrid, 0);
    uint32_t pitch = (m_width + (GRID_SIZE_M_ONE)) / GRID_SIZE;

    uint32_t width_align = (m_width + GRID_SIZE_M_ONE) & ~GRID_SIZE_M_ONE;
    uint32_t height_align = (m_height + GRID_SIZE_M_ONE) & ~GRID_SIZE_M_ONE;
    float    grid_w = 1.f / (width_align / GRID_SIZE);
    float    grid_h = 1.f / (height_align / GRID_SIZE);
    float    grid_min = grid_w > grid_h ? grid_h * .5f : grid_w * .5f;

    // get the half widths for our near and far planes
    float vn_height = tanf(deg_to_rad(cam->fovy * .5f)) * cam->nearPlane;
    float vn_width = vn_height * cam->aspect;

    float vf_height = tanf(deg_to_rad(cam->fovy * .5f)) * cam->farPlane;
    float vf_width = vf_height * cam->aspect;

    vec3_t ws_cam_pos = {cam->x, cam->y, cam->z};
    vec3_t cam_pos = {0.f, 0.f, 0.f}; // Lights are in view space so this is always the case
    vec3_t vs_right = {1.f, 0.f, 0.f}, vs_up = {0.f, 1.f, 0.f}, vs_fwd = {0.f, 0.f, 1.f};
    vec3_t vnear, vnear2, vfar, vnup, vnright, vnup2, vnright2, vfup, vfright, vfup2, vfright2;
    vec3_t vt;

    ddPush();
    ddSetWireframe(true);
    ddSetColor(0xFFFFFFFF);

    vec3_scale(&vfar, &vs_fwd, cam->farPlane);
    vec3_scale(&vnear, &vs_fwd, cam->nearPlane);
    vec3_scale(&vnear2, &vs_fwd, cam->nearPlane + grid_min);

    vec3_scale(&vnup, &vs_up, vn_height * grid_h * 2.f);
    vec3_scale(&vnright, &vs_right, vn_width * grid_w * 2.f);
    vec3_scale(&vnup2, &vs_up, vn_height);
    vec3_scale(&vnright2, &vs_right, vn_width);

    vec3_scale(&vfup, &vs_up, vf_height * grid_h * 2.f);
    vec3_scale(&vfright, &vs_right, vf_width * grid_w * 2.f);
    vec3_scale(&vfup2, &vs_up, vf_height);
    vec3_scale(&vfright2, &vs_right, vf_width);

    vec3_t v_near_c = {0}, v_far_c = {0};

    float i_view[16];
    bx::mtxInverse(i_view, cam->view);

    uint16_t grid_idx = 0;
    for (uint32_t y = 0; y < m_height; y += GRID_SIZE) {
      uint32_t yy = y / GRID_SIZE;
      // Get to our initial starting position of the frustum.
      vec3_add(&v_near_c, &cam_pos, &vnear);
      vec3_sub(&v_near_c, &vnright2);
      vec3_sub(&v_near_c, &vnup2);
      vec3_scale(&vt, &vnup, (float)yy);
      vec3_add(&v_near_c, &vt);

      vec3_add(&v_far_c, &cam_pos, &vfar);
      vec3_sub(&v_far_c, &vfright2);
      vec3_sub(&v_far_c, &vfup2);
      vec3_scale(&vt, &vfup, (float)yy);
      vec3_add(&v_far_c, &vt);

      for (uint32_t x = 0; x < m_width; x += GRID_SIZE, ++grid_idx) {
        uint32_t xx = x / GRID_SIZE;
        uint16_t local_count = 0;
        grid[((yy * pitch + xx) * MAX_LIGHT_GRID_COUNT) + local_count] = 0xFFFF;

        plane_t frustum[6];

        vec3_t ntl, ntr, nbl = v_near_c, nbr;
        vec3_t ftl, ftr, fbl = v_far_c, fbr;

        vec3_add(&ntl, &v_near_c, &vnup);
        vec3_sub(&ntl, &vnright);
        vec3_add(&ntr, &v_near_c, &vnup);
        vec3_add(&ntr, &vnright);
        vec3_sub(&nbl, &v_near_c, &vnup);
        vec3_sub(&nbl, &vnright);
        vec3_sub(&nbr, &v_near_c, &vnup);
        vec3_add(&nbr, &vnright);

        vec3_add(&ftl, &v_far_c, &vfup);
        vec3_sub(&ftl, &vfright);
        vec3_add(&ftr, &v_far_c, &vfup);
        vec3_add(&ftr, &vfright);
        vec3_sub(&fbl, &v_far_c, &vfup);
        vec3_sub(&fbl, &vfright);
        vec3_sub(&fbr, &v_far_c, &vfup);
        vec3_add(&fbr, &vfright);

        // for each cell, build a custom frustum to test against //CW
        plane_build(frustum + 0, &ntr, &ntl, &nbl); // near
        plane_build(frustum + 1, &ftl, &ftr, &fbl); // far
        plane_build(frustum + 2, &ftl, &nbl, &ntl); // left
        plane_build(frustum + 3, &ftr, &ntr, &nbr); // right
        plane_build(frustum + 4, &ntr, &ftr, &ftl); // top
        plane_build(frustum + 5, &nbr, &fbl, &fbr); // bottom

        if (grid_idx == dbgCell) {
          // bgfx::dbgTextPrintf(0, 30, 0x6f, "%f, %f, %f", ntl.x, ntl.y, ntl.z);
          // bgfx::dbgTextPrintf(0, 31, 0x6f, "%f, %f, %f", ntr.x, ntr.y, ntr.z);
          // bgfx::dbgTextPrintf(0, 32, 0x6f, "%f, %f, %f", nbl.x, nbl.y, nbl.z);
          // bgfx::dbgTextPrintf(0, 33, 0x6f, "%f, %f, %f", nbr.x, nbr.y, nbr.z);
          //
          // bgfx::dbgTextPrintf(0, 35, 0x6f, "%f, %f, %f", ftl.x, ftl.y, ftl.z);
          // bgfx::dbgTextPrintf(0, 36, 0x6f, "%f, %f, %f", ftr.x, ftr.y, ftr.z);
          // bgfx::dbgTextPrintf(0, 37, 0x6f, "%f, %f, %f", fbl.x, fbl.y, fbl.z);
          // bgfx::dbgTextPrintf(0, 38, 0x6f, "%f, %f, %f", fbr.x, fbr.y, fbr.z);
        }

        for (uint32_t l = 0; l < light_count; ++l) {
          // do test
          bool outside = false;
          for (uint32_t p = 0; p < 6; ++p) {
            float dd = vec3_dot(&frustum[p].n, &lights[l].p) - frustum[p].d + (lights[l].radius);
            if (dd < 0.f) {
              outside = true;
              break;
            }
          }
          if (!outside) {
            grid[((yy * pitch + xx) * MAX_LIGHT_GRID_COUNT) + local_count] = l;
            ++local_count;
            grid[((yy * pitch + xx) * MAX_LIGHT_GRID_COUNT) + local_count] = 0xFFFF;

            if (dbgLightGridFrustum && grid_idx == dbgCell) {
              vec3_t vs_light_pos;
              bx::vec3MulMtx(vs_light_pos.v, lights[l].p.v, i_view);
              Sphere ll = {vs_light_pos.x, vs_light_pos.y, vs_light_pos.z, lights[l].radius};
              ddDraw(ll);
              *debugLightList = l;
              ++debugLightList;
            }
          }
        }

        if (dbgLightGridFrustum && grid_idx == dbgCell) {
          vec3_t wtl, wtr, wbl, wbr;
          vec3_t wtl2, wtr2, wbl2, wbr2;
          bx::vec3MulMtx(wtl.v, ntl.v, i_view);
          bx::vec3MulMtx(wtr.v, ntr.v, i_view);
          bx::vec3MulMtx(wbl.v, nbl.v, i_view);
          bx::vec3MulMtx(wbr.v, nbr.v, i_view);

          bx::vec3MulMtx(wtl2.v, ftl.v, i_view);
          bx::vec3MulMtx(wtr2.v, ftr.v, i_view);
          bx::vec3MulMtx(wbl2.v, fbl.v, i_view);
          bx::vec3MulMtx(wbr2.v, fbr.v, i_view);

          ddMoveTo(wtl.x, wtl.y, wtl.z);
          ddLineTo(wtl2.x, wtl2.y, wtl2.z);
          ddMoveTo(wtr.x, wtr.y, wtr.z);
          ddLineTo(wtr2.x, wtr2.y, wtr2.z);
          ddMoveTo(wbl.x, wbl.y, wbl.z);
          ddLineTo(wbl2.x, wbl2.y, wbl2.z);
          ddMoveTo(wbr.x, wbr.y, wbr.z);
          ddLineTo(wbr2.x, wbr2.y, wbr2.z);
        }

        // grid[yy*pitch + xx].start = grid_idx;

        vec3_add(&v_near_c, &vnright);
        vec3_add(&v_far_c, &vfright);
      }
      vec3_add(&v_near_c, &vnup);
      vec3_add(&v_far_c, &vfup);
    }

    ddPush();

    ddSetColor(0xff0000ff);
    ddDrawFrustum(cam->viewProj);
    ddSetColor(0x6000ff00);
    ddSetState(false, false, false);

    ddMoveTo(0.f, 0.f, 0.f);
    ddSetWireframe(false);

    vec3_scale(&vfar, &cam->forward, cam->farPlane);
    vec3_scale(&vnear, &cam->forward, cam->nearPlane);
    vec3_scale(&vnear2, &cam->forward, cam->nearPlane + grid_min);

    vec3_scale(&vnup, &cam->up, vn_height * grid_h * 2.f);
    vec3_scale(&vnright, &cam->right, vn_width * grid_w * 2.f);
    vec3_scale(&vnup2, &cam->up, vn_height);
    vec3_scale(&vnright2, &cam->right, vn_width);

    vec3_scale(&vfup, &cam->up, vf_height * grid_h * 2.f);
    vec3_scale(&vfright, &cam->right, vf_width * grid_w * 2.f);
    vec3_scale(&vfup2, &cam->up, vf_height);
    vec3_scale(&vfright2, &cam->right, vf_width);

    ddPop();

    ddPop();

    *debugLightList = 0xFFFF;
  }

  bool update() {
    rmt_ScopedCPUSample(Update, 0);
    static float colours[][4] = {
      {1.f, 0.f, 0.f, 1.f}, {0.f, 1.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f},
    };

    if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &mouseState)) {
      imguiBeginFrame(mouseState.m_mx,
                      mouseState.m_my,
                      (mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0) |
                        (mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0) |
                        (mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0),
                      mouseState.m_mz,
                      uint16_t(m_width),
                      uint16_t(m_height));

      float          uniformDataTmp[4] = {0};
      CameraParams   cam;
      int64_t        now = bx::getHPCounter();
      static int64_t last = now;
      const int64_t  frameTime = now - last;
      last = now;
      const double freq = double(bx::getHPFrequency());
      const double toMs = 1000.0 / freq;
      time = (float)((now) / double(bx::getHPFrequency()));
      const float deltaTime = float(frameTime / freq);

      float view[16], iViewX[16], mTmp[16];
      cameraUpdate(deltaTime, mouseState);
      cameraGetViewMtx(view);
      cameraGetPosition(cameraPos);

      float timeVec[4] = {time, 0.f, 0.f, 0.f};
      bgfx::setUniform(timeUniform, timeVec);

      float gridVec[4] = {
        (float)envMapGridDim[0], (float)envMapGridDim[1], (float)envMapGridDim[2], (float)ENV_MAP_GRID_DIM};
      bgfx::setUniform(gridSizeUniform, gridVec);
      gridVec[0] = totalBoundsMin[0];
      gridVec[1] = totalBoundsMin[1];
      gridVec[2] = totalBoundsMin[2];
      bgfx::setUniform(gridOffsetUniform, gridVec);

      // setup sunlight
      float  sunlight[4] = {0.f, 0.f, 1.f, sunlightAngle[3]};
      vec3_t sunforward = {0.f, 0.f, 1.f}, sunforward2;
      bx::mtxRotateXYZ(mTmp, deg_to_rad(sunlightAngle[0]), deg_to_rad(sunlightAngle[1]), deg_to_rad(sunlightAngle[2]));
      bx::vec3MulMtx(sunforward2.v, sunforward.v, mTmp);
      /*
      // Set sunlight in view space (requires inverse view transpose)
      bx::mtxInverse(mTmp, view);
      vec3_norm(&cam.up, (vec3_t*)&mTmp[4]);
      vec3_norm(&cam.right, (vec3_t*)&mTmp[0]);
      vec3_norm(&cam.forward, (vec3_t*)&mTmp[8]);
      bx::mtxTranspose(iViewX, mTmp);

      bx::vec3MulMtx(sunforward.v, sunforward2.v, iViewX);
      // Set sunlight in view space
      vec3_norm(&sunforward2, &sunforward);
      */
      sunlight[0] = sunforward2.x;
      sunlight[1] = sunforward2.y;
      sunlight[2] = sunforward2.z;

      bgfx::setUniform(ambientColourUniform, ambientColour);

      // debug Mode
      float debugModeVec[4] = {(float)dbgVisMode};
      bgfx::setUniform(debugUniform, debugModeVec);

      bx::mtxScale(mTmp, GLOBAL_SCALE, GLOBAL_SCALE, GLOBAL_SCALE);

      float proj[16], i_proj[16], i_view[16];
      cam.x = cameraPos[0];
      cam.y = cameraPos[1];
      cam.z = cameraPos[2];
      cam.fovy = 60.f;
      cam.aspect = float(m_width) / float(m_height);
      cam.nearPlane = .5f;
      cam.farPlane = 1000.0f;
      bx::mtxProj(proj, cam.fovy, cam.aspect, cam.nearPlane, cam.farPlane, false);
      memcpy(cam.view, view, sizeof(float) * 16);

      bx::mtxMul(cam.viewProj, view, proj);

      float orthoProj[16], idt[16];
      bx::mtxIdentity(idt);
      bx::mtxOrtho(orthoProj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.f, false);

      float vheight = tanf(cam.fovy) * cam.nearPlane;
      float vwidth = vheight * cam.aspect;

      bx::mtxInverse(i_proj, cam.viewProj);
      bgfx::setUniform(cameraPosUniform, i_proj);

      bgfx::dbgTextClear();
      ddBegin(RENDER_PASS_DEBUG);
      ddSetLod(2);

      static CameraParams saved_cam_params;
      if (dbgUpdateFrustum) {
        // save = false;
        saved_cam_params = cam;
      }
      bx::mtxInverse(i_view, saved_cam_params.view);

      if (ImGui::Begin("Debug Options") && !dbgCaptureCubemap) {
        if (ImGui::Button("Reload Shaders")) {
          loadAssetData(ShaderParams, SOLID_PROGS);
        }
        ImGui::SameLine();
        if (ImGui::Button("Capture Cubemaps (Slow)")) {
          dbgCaptureCubemap = true;
          curCaptureProbe = 1;
          for (uint32_t i = 0; i < 3; ++i)
            envMapCurrentCell[i] = envMapGridDim[i];
        }
        if (ImGui::CollapsingHeader("Render Control")) {
          ImGui::SliderFloat3("Sunlight Angle", sunlightAngle, 0.f, 360.f);
          ImGui::SliderFloat("Sunlight Power", sunlightAngle + 3, 0.5f, 10.f);
          ImGui::ColorEdit3("Ambient Colour", ambientColour, ImGuiColorEditFlags_RGB);
          ImGui::SliderFloat("Ambient Power", ambientColour + 3, 0.0f, 2.f);
          ImGui::SliderFloat("Exposure Key", &curExposure, 0.0001f, 10.f);
          ImGui::SliderFloat("Exposure Min", &curExposureMin, 0.0001f, 1.f);
          ImGui::SliderFloat("Exposure Max", &curExposureMax, curExposureMin, 10.f);
        }
        if (ImGui::CollapsingHeader("Vis Modes")) {
          ImGui::RadioButton("none", &dbgVisMode, 0);
          ImGui::RadioButton("albedo", &dbgVisMode, 1);
          ImGui::RadioButton("normal", &dbgVisMode, 2);
          ImGui::RadioButton("metalness", &dbgVisMode, 3);
          ImGui::RadioButton("roughness", &dbgVisMode, 4);
          ImGui::RadioButton("diffuse", &dbgVisMode, 5);
          ImGui::RadioButton("specular", &dbgVisMode, 6);
          ImGui::RadioButton("diffuse IBL", &dbgVisMode, 7);
          ImGui::RadioButton("specular IBL", &dbgVisMode, 8);
        }
        if (ImGui::CollapsingHeader("Debug")) {
          char const* shader_names[SOLID_PROGS] = {nullptr};
          for (uint32_t i = 0; i < SOLID_PROGS; ++i)
            shader_names[i] = ShaderParams[i].description;
          ImGui::ListBox("Current Shader", &dbgShader, shader_names, SOLID_PROGS);
          ImGui::Checkbox("Debug Light Grid", &dbgLightGrid);
          ImGui::Checkbox("Draw Light Grid Frustum", &dbgLightGridFrustum);
          ImGui::Checkbox("Enable Light View Updates", &dbgUpdateFrustum);
          ImGui::Checkbox("Draw Light", &dbgDrawLights);
          ImGui::Checkbox("Wireframe Lights", &dbgDrawLightsWire);
          ImGui::Checkbox("Draw Z Bins", &dbgDrawZBin);
          ImGui::Checkbox("Draw Debug Stats", &dbgDebugStats);
          ImGui::Checkbox("Enable Light Movement", &dbgDebugMoveLights);
          uint32_t max_cell = ((m_width / GRID_SIZE) * ((m_height + GRID_SIZE_M_ONE) / GRID_SIZE));
          ImGui::SliderInt("Debug Cell Info Index", &dbgCell, 0, max_cell);
          ImGui::SliderInt("Debug Z Bin Info Index", &dbgBucket, 0, Z_BUCKETS);
          ImGui::Checkbox("Use Pixel Shader Average Luminance", &dbgPixelAvgLuminance);
          ImGui::Checkbox("Show Reflection Probes", &dbgShowReflectionProbes);
        }
        if (ImGui::CollapsingHeader("CameraMatix")) {
          ImGui::Text("%f, %f, %f, %f", view[0], view[1], view[2], view[3]);
          ImGui::Text("%f, %f, %f, %f", view[4], view[5], view[6], view[7]);
          ImGui::Text("%f, %f, %f, %f", view[8], view[9], view[10], view[11]);
          ImGui::Text("%f, %f, %f, %f", view[12], view[13], view[14], view[15]);
        }
        if (ImGui::CollapsingHeader("Reflection Probes")) {
          if (ImGui::Button("Add New Probe")) {
            ++totalProbes;
            probeArray = (probe_t*)realloc(probeArray, sizeof(*probeArray) * totalProbes);
            probeArray[totalProbes - 1].position.x = 0.f;
            probeArray[totalProbes - 1].position.y = 0.f;
            probeArray[totalProbes - 1].position.z = 0.f;
            probeArray[totalProbes - 1].halfWidths.x = .5f;
            probeArray[totalProbes - 1].halfWidths.y = .5f;
            probeArray[totalProbes - 1].halfWidths.z = .5f;
          }
          if (totalProbes > 0) {
            ImGui::SliderInt("Edit Probe", &curEditProbe, 0, totalProbes - 1);

            float bounds_min = MIN(totalBoundsMin[0], totalBoundsMin[1]);
            bounds_min = MIN(totalBoundsMin[2], bounds_min);
            float bounds_max = MAX(totalBoundsMax[0], totalBoundsMax[1]);
            bounds_max = MAX(totalBoundsMax[2], bounds_max);
            ImGui::SliderFloat3("Probe Position", probeArray[curEditProbe].position.v, bounds_min, bounds_max);
            ImGui::SliderFloat3("Probe Size", probeArray[curEditProbe].halfWidths.v, 0.001f, 100.f);
          }
        }
        // ImGui::Checkbox("Capture IR test cubemap", &dbgCaptureCubemap);
        // static int32_t cubemapface = 0;
        // ImGui::SliderInt("Current Cubemap Face", &cubemapface, 0, 5);
        // ImGui::Image(cubemapFaceTexture[cubemapface], ImVec2(256, 256));
      }
      ImGui::End();

      LightInfo viewspaceLights[LIGHT_COUNT] = {0};
      uint32_t  zbins[Z_BUCKETS];
      uint16_t  dbg_light_list[LIGHT_COUNT + 1];
      float     zstep;
      if (!dbgCaptureCubemap) zbinLights(viewspaceLights, &saved_cam_params, lights, LIGHT_COUNT, zbins, &zstep);

      float lightingParams[4] = {
        zstep, (float)GRID_SIZE, (float)((m_width + (GRID_SIZE_M_ONE)) / GRID_SIZE), (float)(m_height)};
      bgfx::setUniform(lightingParamsUniform, lightingParams);

      bool enableCPUUpdateLightGrid = !bgfx::isValid(ShaderParams[dbgShader].csLightCull);
      bool enableComputeLightCull = bgfx::isValid(ShaderParams[dbgShader].csLightCull);
      // use viewspaceLights which are sorted by distance from the camera
      if (enableCPUUpdateLightGrid && !dbgCaptureCubemap) {
        updateLightingGrid(viewspaceLights, LIGHT_COUNT, &saved_cam_params, grid, dbg_light_list);
      }

      // Setup the rendering passes.
      bgfx::setViewRect(RENDER_PASS_COMPUTE, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_COMPUTE, view, proj);

      bgfx::setViewRect(RENDER_PASS_GPU_COPY_LAST, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_GPU_COPY_LAST, view, proj);

      bgfx::setViewRect(RENDER_PASS_ZPREPASS, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_ZPREPASS, view, proj);
      bgfx::setViewFrameBuffer(RENDER_PASS_ZPREPASS, zPrePassTarget);

      bgfx::setViewRect(RENDER_PASS_SOLID, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_SOLID, view, proj);
      bgfx::setViewFrameBuffer(RENDER_PASS_SOLID, mainTarget);

      bgfx::setViewRect(RENDER_PASS_SKYBOX, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_SKYBOX, idt, orthoProj);
      bgfx::setViewFrameBuffer(RENDER_PASS_SKYBOX, mainTarget);

      uint32_t luminance_w = m_width / 2, luminance_h = m_height / 2;
      for (uint32_t i = 0; i < numLuminanceMips; ++i) {
        bgfx::setViewRect(RENDER_PASS_LUMINANCE_START + i, 0, 0, luminance_w, luminance_h);
        bgfx::setViewTransform(RENDER_PASS_LUMINANCE_START + i, idt, orthoProj);
        if (luminance_w > 1) luminance_w /= 2;
        if (luminance_h > 1) luminance_h /= 2;
        bgfx::setViewFrameBuffer(RENDER_PASS_LUMINANCE_START + i, luminanceTargets[i]);
      }

      bgfx::setViewRect(RENDER_PASS_TONEMAP, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_TONEMAP, idt, orthoProj);
      bgfx::setViewFrameBuffer(RENDER_PASS_TONEMAP, finalTarget);

      bgfx::setViewRect(RENDER_PASS_DEBUG, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_DEBUG, view, proj);
      bgfx::setViewFrameBuffer(RENDER_PASS_DEBUG, finalTarget);

      bgfx::setViewRect(RENDER_POST_DEBUG_BLIT, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_POST_DEBUG_BLIT, idt, orthoProj);

      bgfx::setViewRect(RENDER_PASS_2DDEBUG, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_2DDEBUG, idt, orthoProj);

      bgfx::setViewRect(RENDER_PASS_BDRF_CONVOLVE, 0, 0, BDRF_DIM, BDRF_DIM);
      bgfx::setViewTransform(RENDER_PASS_BDRF_CONVOLVE, idt, orthoProj);
      bgfx::setViewFrameBuffer(RENDER_PASS_BDRF_CONVOLVE, bdrfConvolveTarget);

      bgfx::UniformHandle handles[MATERIAL_MAX];
      handles[MATERIAL_BASE] = baseTex;
      handles[MATERIAL_NORMAL] = normalTex;
      handles[MATERIAL_ROUGHNESS] = roughTex;
      handles[MATERIAL_METALLIC] = metalTex;
      handles[MATERIAL_MASK] = maskTex;

      bgfx::DynamicVertexBufferHandle buffer[5] = {
        zBinBuffer,
        lightGridBuffer,
        lightListBuffer,
        lightPositionBuffer,
        enableCPUUpdateLightGrid ? lightGridBuffer : lightGridFatBuffer,
      };

      util::Texture env_texs[] = {
        {5, bdrfTex, bdrfTexture},
        {6, environmentMapTex, skyboxTex},
        {7, irradianceMapTex, skyboxIrrTex},
        {8, irradianceAraryTex, irCubemapArrayTexture},
        {9, specIrradianceAraryTex, specIRCubemapArrayTexture},
      };

      if (!dbgCaptureCubemap) {
        // enable lights, enable ibl
        uniformDataTmp[0] = 1.f;
        uniformDataTmp[1] = 1.f;
        bgfx::setUniform(dynLightsUniform, uniformDataTmp);
        bgfx::setUniform(sunlightUniform, sunlight);

        // Update the lighting buffers.
        bgfx::updateDynamicVertexBuffer(lightPositionBuffer, 0, bgfx::copy(viewspaceLights, sizeof(viewspaceLights)));
        bgfx::updateDynamicVertexBuffer(zBinBuffer, 0, bgfx::copy(zbins, sizeof(uint32_t) * Z_BUCKETS));
        if (enableCPUUpdateLightGrid) {
          bgfx::updateDynamicVertexBuffer(
            lightGridBuffer, 0, bgfx::copy(grid, LIGHT_GRID_SPACE * sizeof(uint16_t) * MAX_LIGHT_GRID_COUNT));
        }

        bgfx::touch(RENDER_PASS_COMPUTE);

        // Do the compute shader version.
        if (enableComputeLightCull) {
          float viewParams[8] = {saved_cam_params.nearPlane,
                                 saved_cam_params.farPlane,
                                 saved_cam_params.fovy,
                                 saved_cam_params.aspect,
                                 (float)LIGHT_COUNT,
                                 (float)m_width,
                                 (float)m_height,
                                 0};
          bgfx::setUniform(csViewPrams, viewParams, 2);
          bgfx::setBuffer(0, lightPositionBuffer, bgfx::Access::Read); // input
          bgfx::setBuffer(1, lightGridFatBuffer, bgfx::Access::Write); // ouput
          bgfx::dispatch(RENDER_PASS_COMPUTE,
                         ShaderParams[dbgShader].csLightCull,
                         ((m_width + (GRID_SIZE_M_ONE)) / GRID_SIZE),
                         ((m_height + (GRID_SIZE_M_ONE)) / GRID_SIZE),
                         1);
        }


        uint64_t state = 0;

        if (bgfx::isValid(ShaderParams[dbgShader].zfillProg)) {
          // Set instance data buffer.
          mesh_submit(&m_mesh,
                      RENDER_PASS_ZPREPASS,
                      ShaderParams[dbgShader].zfillProg,
                      mTmp,
                      BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_DEPTH_WRITE | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA,
                      handles,
                      buffer,
                      env_texs,
                      5);
          for (uint32_t o = 0; o < TOTAL_ORBS; ++o) {
            float model[16];
            bx::mtxTranslate(model, Orbs[o].position.x, Orbs[o].position.y, Orbs[o].position.z);
            bgfx::setUniform(albedoUnform, Orbs[o].colour);
            float mr[4] = {Orbs[o].metal ? 1.f : 0.f, Orbs[o].roughnes};
            bgfx::setUniform(metalRoughUniform, mr);
            bgfx::setBuffer(10, buffer[0], bgfx::Access::Read);
            bgfx::setBuffer(11, buffer[1], bgfx::Access::Read);
            bgfx::setBuffer(12, buffer[2], bgfx::Access::Read);
            bgfx::setBuffer(13, buffer[3], bgfx::Access::Read);
            bgfx::setBuffer(14, buffer[4], bgfx::Access::Read);
            mesh_submit(&m_orb,
                        RENDER_PASS_ZPREPASS,
                        ShaderParams[dbgShader].zfillProg,
                        model,
                        BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_DEPTH_WRITE | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA);
          }
        }
        // Set instance data buffer.
        mesh_submit(&m_mesh,
                    RENDER_PASS_SOLID,
                    ShaderParams[dbgShader].solidProg,
                    defaultMask,
                    mTmp,
                    BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_DEPTH_TEST_LEQUAL |
                      BGFX_STATE_DEPTH_WRITE | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA,
                    handles,
                    buffer,
                    env_texs,
                    5);
        for (uint32_t o = 0; o < TOTAL_ORBS; ++o) {
          float model[16];
          bx::mtxScale(model, 2.5f);
          model[12] = Orbs[o].position.x;
          model[13] = Orbs[o].position.y;
          model[14] = Orbs[o].position.z;
          bgfx::setUniform(albedoUnform, Orbs[o].colour);
          float mr[4] = {Orbs[o].metal ? 1.f : 0.f, Orbs[o].roughnes};
          bgfx::setUniform(metalRoughUniform, mr);
          bgfx::setBuffer(10, buffer[0], bgfx::Access::Read);
          bgfx::setBuffer(11, buffer[1], bgfx::Access::Read);
          bgfx::setBuffer(12, buffer[2], bgfx::Access::Read);
          bgfx::setBuffer(13, buffer[3], bgfx::Access::Read);
          bgfx::setBuffer(14, buffer[4], bgfx::Access::Read);
          mesh_submit(&m_orb,
                      RENDER_PASS_SOLID,
                      ShaderParams[dbgShader].solidProgNoTex,
                      model,
                      BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_DEPTH_TEST_LEQUAL |
                        BGFX_STATE_DEPTH_WRITE | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA,
                      handles,
                      buffer,
                      env_texs,
                      5);
        }

        //Render SKYBOX
        bgfx::setTexture(0, baseTex, skyboxTex2);
        bgfx::setTexture(6, environmentMapTex, skyboxTex);
        bgfx::setTexture(7, irradianceMapTex, skyboxIrrTex);
        bgfx::setState(0 | BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_DEPTH_TEST_LEQUAL |
          BGFX_STATE_DEPTH_WRITE | BGFX_STATE_MSAA);
        screenSpaceQuad((float)m_width, (float)m_height, s_texelHalf, m_caps->originBottomLeft);
        bgfx::submit(RENDER_PASS_SKYBOX, skyboxProg);


        bool enableFullscreenQuad = bgfx::isValid(ShaderParams[dbgShader].fullscreenProg);
        if (enableFullscreenQuad) {
          bgfx::setBuffer(10, zBinBuffer, bgfx::Access::Read);
          bgfx::setBuffer(11, lightGridBuffer, bgfx::Access::Read);
          bgfx::setBuffer(12, lightListBuffer, bgfx::Access::Read);
          bgfx::setBuffer(13, lightPositionBuffer, bgfx::Access::Read);
          bgfx::setBuffer(14, enableCPUUpdateLightGrid ? lightGridBuffer : lightGridFatBuffer, bgfx::Access::Read);

          bgfx::setState(0 | BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_BLEND_ADD |
                         BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD) | BGFX_STATE_MSAA);
          screenSpaceQuad((float)m_width, (float)m_height, s_texelHalf, m_caps->originBottomLeft);
          bgfx::submit(RENDER_PASS_2DDEBUG, ShaderParams[dbgShader].fullscreenProg);
        }

        // Average Luminance
        if (dbgPixelAvgLuminance) {
          uint32_t luminance_w = m_width / 2, luminance_h = m_height / 2;
          uint32_t luminance_mip = 0;
          for (uint32_t luminance_mip = 0; luminance_mip < numLuminanceMips; ++luminance_mip) {
            bgfx::setState(BGFX_STATE_RGB_WRITE | BGFX_STATE_MSAA);
            float uv_offsets[12] = {0.5f / ((float)luminance_w),
                                    0.f,
                                    -.5f / ((float)luminance_w),
                                    0.f,
                                    0.f,
                                    0.5f / ((float)luminance_h),
                                    0.f,
                                    -.5f / ((float)luminance_h),
                                    luminance_mip == 0 ? 1.f : 0.f,
                                    0.f,
                                    0.f,
                                    0.f};
            bgfx::setUniform(offsetUniform, uv_offsets, 3);
            screenSpaceQuad((float)luminance_w, (float)luminance_h, s_texelHalf, m_caps->originBottomLeft);
            bgfx::setTexture(1, luminanceTex, luminance_mip ? luminanceMips[luminance_mip - 1] : colourTarget);
            bgfx::submit(RENDER_PASS_LUMINANCE_START + luminance_mip, luminancePixelProg);

            if (luminance_w > 1) luminance_w /= 2;
            if (luminance_h > 1) luminance_h /= 2;
          }
        }

        bgfx::setTexture(0, baseTex, colourTarget);
        bgfx::setTexture(1, luminanceTex, luminanceMips[numLuminanceMips - 1]);
        bgfx::setBuffer(1, workingLuminanceBuffer, bgfx::Access::ReadWrite); // input
        bgfx::setBuffer(2, averageLuminanceBuffer, bgfx::Access::ReadWrite); // ouput
        float viewParams[8] = {0.f,
                               0.f,
                               0.f,
                               (float)m_width * m_height,
                               (float)(m_width + SMALL_DISPATCH_WAVE - 1) / SMALL_DISPATCH_WAVE,
                               (float)(m_height + SMALL_DISPATCH_WAVE - 1) / SMALL_DISPATCH_WAVE,
                               (float)0,
                               (float)(((m_width + SMALL_DISPATCH_WAVE - 1) / SMALL_DISPATCH_WAVE) *
                                       ((m_height + SMALL_DISPATCH_WAVE - 1) / SMALL_DISPATCH_WAVE))};
        bgfx::setUniform(csViewPrams, viewParams, 2);
        bgfx::dispatch(RENDER_PASS_TONEMAP,
                       logLuminanceAvProg,
                       ((m_width + (SMALL_DISPATCH_WAVE - 1)) / SMALL_DISPATCH_WAVE),
                       ((m_height + (SMALL_DISPATCH_WAVE - 1)) / SMALL_DISPATCH_WAVE),
                       1);

        // Tonemap pass
        bgfx::setTexture(0, baseTex, colourTarget);
        bgfx::setTexture(1, luminanceTex, luminanceMips[numLuminanceMips - 1]);
        bgfx::setTexture(5, colourLUTTex, colourGradeLUT);
        float exposure_params[4] = {curExposureMin, curExposureMax, curExposure, 0.f};
        bgfx::setUniform(exposureParamsUniform, exposure_params);
        bgfx::setState(BGFX_STATE_RGB_WRITE | BGFX_STATE_MSAA);
        screenSpaceQuad((float)m_width, (float)m_height, s_texelHalf, m_caps->originBottomLeft);
        bgfx::submit(RENDER_PASS_TONEMAP, tonemapProg);


        bgfx::setTexture(0, baseTex, finalColourTarget);
        bgfx::setState(BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_MSAA);
        screenSpaceQuad((float)m_width, (float)m_height, s_texelHalf, m_caps->originBottomLeft);
        bgfx::submit(RENDER_POST_DEBUG_BLIT, copyProg);
      } else if (dbgWaitingOnReadback) {
        if (frameReturn >= dbgCaptureReadyFrame) {
          bool const capturingCustomProbes = envMapCurrentCell[0] >= envMapGridDim[0] &&
                                             envMapCurrentCell[1] >= envMapGridDim[1] &&
                                             envMapCurrentCell[2] >= envMapGridDim[2];
          char dstFile[1024];

          sprintf(
            dstFile, "data/env/%dx%dx%d_ir.ktx", envMapCurrentCell[0], envMapCurrentCell[1], envMapCurrentCell[2]);
          if (capturingCustomProbes) {
            sprintf(dstFile, "data/env/custom_%d_ir.ktx", curCaptureProbe - 1);
          }
          char        ktx_id[12] = KTX_FILE_IDENT;
          ktxheader_t h;
          memcpy(h.identifier, ktx_id, sizeof h.identifier);
          h.endianness = KTX_ENDIANNESS_CHECK;
          h.glType = KTX_HALF_FLOAT;
          h.glTypeSize = 2;
          h.glFormat = KTX_RGBA;
          h.glInternalFormat = KTX_RGBA16F;
          h.glBaseInternalFormat = KTX_RGBA;
          h.pixelWidth = IR_MAP_DIM;
          h.pixelHeight = IR_MAP_DIM;
          h.pixelDepth = 0;
          h.numberOfArrayElements = 0;
          h.numberOfFaces = 6;
          h.numberOfMipmapLevels = 1;
          h.bytesOfKeyValueData = 0;

          FILE* f = fopen(dstFile, "wb");
          if (f) {
            uint32_t facesize = IR_MAP_DIM * IR_MAP_DIM * sizeof(half_t) * 4;
            fwrite(&h, sizeof ktxheader_t, 1, f);
            fwrite(&facesize, sizeof uint32_t, 1, f);
            fwrite(irCubemapFaceTextureReadCPUData, IR_MAP_DIM * IR_MAP_DIM * 6 * sizeof(half_t) * 4, 1, f);
            fclose(f);
          }

          sprintf(
            dstFile, "data/env/%dx%dx%d_cm.ktx", envMapCurrentCell[0], envMapCurrentCell[1], envMapCurrentCell[2]);
          if (capturingCustomProbes) {
            sprintf(dstFile, "data/env/custom_%d_cm.ktx", curCaptureProbe - 1);
          }
          memset(&h, 0, sizeof h);
          memcpy(h.identifier, ktx_id, sizeof h.identifier);
          h.endianness = KTX_ENDIANNESS_CHECK;
          h.glType = KTX_HALF_FLOAT;
          h.glTypeSize = 2;
          h.glFormat = KTX_RGBA;
          h.glInternalFormat = KTX_RGBA16F;
          h.glBaseInternalFormat = KTX_RGBA;
          h.pixelWidth = m_width;
          h.pixelHeight = m_width;
          h.pixelDepth = 0;
          h.numberOfArrayElements = 0;
          h.numberOfFaces = 6;
          h.numberOfMipmapLevels = 1;
          h.bytesOfKeyValueData = 0;
          f = fopen(dstFile, "wb");
          if (f) {
            uint32_t facesize = m_width * m_width * sizeof(half_t) * 4;
            fwrite(&h, sizeof ktxheader_t, 1, f);
            fwrite(&facesize, sizeof uint32_t, 1, f);
            fwrite(cubemapFaceTextureReadCPUData, m_width * m_width * 6 * sizeof(half_t) * 4, 1, f);
            fclose(f);
          }

          sprintf(
            dstFile, "data/env/%dx%dx%d_sir.ktx", envMapCurrentCell[0], envMapCurrentCell[1], envMapCurrentCell[2]);
          if (capturingCustomProbes) {
            sprintf(dstFile, "data/env/custom_%d_sir.ktx", curCaptureProbe - 1);
          }
          memset(&h, 0, sizeof h);
          memcpy(h.identifier, ktx_id, sizeof h.identifier);
          h.endianness = KTX_ENDIANNESS_CHECK;
          h.glType = KTX_HALF_FLOAT;
          h.glTypeSize = 2;
          h.glFormat = KTX_RGBA;
          h.glInternalFormat = KTX_RGBA16F;
          h.glBaseInternalFormat = KTX_RGBA;
          h.pixelWidth = SIR_MAP_DIM;
          h.pixelHeight = SIR_MAP_DIM;
          h.pixelDepth = 0;
          h.numberOfArrayElements = 0;
          h.numberOfFaces = 6;
          h.numberOfMipmapLevels = SIR_MIP_COUNT;
          h.bytesOfKeyValueData = 0;
          f = fopen(dstFile, "wb");
          if (f) {
            uint32_t facesize = m_width * m_width * sizeof(half_t) * 4;
            fwrite(&h, sizeof ktxheader_t, 1, f);
            // Update the mip sizes
            for (uint32_t i = 0; i < SIR_MIP_COUNT; ++i) {
              uint32_t size_write_offset = sirMipFaceWriteOffsetLookUp[i][0] - 4;
              // *6 for each cubemap face
              *(uint32_t*)((uint8_t*)sirCubemapFaceTextureReadCPUData + size_write_offset) = sirMipLevelSizeBytes[i];
            }
            fwrite(sirCubemapFaceTextureReadCPUData, sirTotalWriteSize, 1, f);
            fclose(f);
          }

          if (envMapCurrentCell[0] == 0 && envMapCurrentCell[1] == 0 && envMapCurrentCell[2] == 0) {
            sprintf(dstFile, "data/env/bdrf.ktx");
            memset(&h, 0, sizeof h);
            memcpy(h.identifier, ktx_id, sizeof h.identifier);
            h.endianness = KTX_ENDIANNESS_CHECK;
            h.glType = KTX_HALF_FLOAT;
            h.glTypeSize = 2;
            h.glFormat = KTX_RG;
            h.glInternalFormat = KTX_RG16F;
            h.glBaseInternalFormat = KTX_RG;
            h.pixelWidth = BDRF_DIM;
            h.pixelHeight = BDRF_DIM;
            h.pixelDepth = 0;
            h.numberOfArrayElements = 0;
            h.numberOfFaces = 0;
            h.numberOfMipmapLevels = 1;
            h.bytesOfKeyValueData = 0;
            f = fopen(dstFile, "wb");
            if (f) {
              uint32_t facesize = BDRF_DIM * BDRF_DIM * sizeof(half_t) * 2;
              fwrite(&h, sizeof ktxheader_t, 1, f);
              fwrite(&facesize, sizeof uint32_t, 1, f);
              fwrite(bdrfTextureReadCPUData, facesize, 1, f);
              fclose(f);
            }
          }

		  if (!curCaptureProbe)
		  {
			  ++envMapCurrentCell[0];
			  if (envMapCurrentCell[0] >= envMapGridDim[0]) {
				  envMapCurrentCell[0] = 0;
				  ++envMapCurrentCell[1];
			  }
			  if (envMapCurrentCell[1] >= envMapGridDim[1]) {
				  envMapCurrentCell[1] = 0;
				  ++envMapCurrentCell[2];
			  }
		  }
          if (envMapCurrentCell[2] >= envMapGridDim[2]) {
            ++curCaptureProbe;
            if (!(curCaptureProbe <= totalProbes)) {
              dbgCaptureCubemap = false;
              bgfx::destroy(irCubemapArrayTexture);
              bgfx::destroy(specIRCubemapArrayTexture);
              bgfx::destroy(bdrfTexture);
              createIRCubemapArrays(envMapGridDim, &irCubemapArrayTexture, &specIRCubemapArrayTexture, &bdrfTexture);
            }
          }
          dbgWaitingOnReadback = false;
        }
      } else {
        if (envMapCurrentCell[0] == 0 && envMapCurrentCell[1] == 0 && envMapCurrentCell[2] == 0) {
          bgfx::setState(BGFX_STATE_RGB_WRITE | BGFX_STATE_MSAA);
          screenSpaceQuad(BDRF_DIM, BDRF_DIM, s_texelHalf, m_caps->originBottomLeft);
          bgfx::submit(RENDER_PASS_BDRF_CONVOLVE, bdrfConvolveProg);
        }

        static vec3_t r_dir[6] = {
          {0.f, 0.f, -1.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f},
        };
        static vec3_t up_dir[6] = {
          {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
        };
        float      cm_view[6][16];
        bool const capturingCustomProbes = envMapCurrentCell[0] >= envMapGridDim[0] &&
                                           envMapCurrentCell[1] >= envMapGridDim[1] &&
                                           envMapCurrentCell[2] >= envMapGridDim[2];
        vec3_t cur_cam_pos;

        cur_cam_pos.x = totalBoundsMin[0] + (envMapCurrentCell[0] * ENV_MAP_GRID_DIM) + ENV_MAP_GRID_DIM * .5f;
        cur_cam_pos.y = totalBoundsMin[1] + (envMapCurrentCell[1] * ENV_MAP_GRID_DIM) + ENV_MAP_GRID_DIM * .5f;
        cur_cam_pos.z = totalBoundsMin[2] + (envMapCurrentCell[2] * ENV_MAP_GRID_DIM) + ENV_MAP_GRID_DIM * .5f;

        if (capturingCustomProbes) {
          // capturing custom probes.
          cur_cam_pos.x = probeArray[curCaptureProbe - 1].position.x;
          cur_cam_pos.y = probeArray[curCaptureProbe - 1].position.y;
          cur_cam_pos.z = probeArray[curCaptureProbe - 1].position.z;
        }

        for (uint32_t i = 0; i < 6; ++i) {
          float  cm_proj[16], v_mtx_tmp[16] = {0};
          vec3_t r = r_dir[i], up = up_dir[i], fw;

          // TODO: ensure this projects is correct for the grid cell. Currently won't match the faces of the cell
          if (capturingCustomProbes) {
            float fovy;
            if (i == 0 || i == 1) { // +X, -X
              fovy =
                rad_to_deg(atan2(probeArray[curCaptureProbe - 1].halfWidths.y, probeArray[curCaptureProbe - 1].halfWidths.x)) * 2.f;
              bx::mtxProj(cm_proj,
                          fovy,
                          probeArray[curCaptureProbe - 1].halfWidths.z / probeArray[curCaptureProbe - 1].halfWidths.y,
                          0.001f,
                          probeArray[curCaptureProbe - 1].halfWidths.x,
                          false);
            } else if (i == 2 || i == 3) { // +Y, -Y
              fovy =
                rad_to_deg(atan2(probeArray[curCaptureProbe - 1].halfWidths.z, probeArray[curCaptureProbe - 1].halfWidths.y)) * 2.f;
              bx::mtxProj(cm_proj,
                          fovy,
                          probeArray[curCaptureProbe - 1].halfWidths.x / probeArray[curCaptureProbe - 1].halfWidths.z,
                          0.001f,
                          probeArray[curCaptureProbe - 1].halfWidths.y,
                          false);
            } else if (i == 4 || i == 5) { // +Z, -Z
              fovy =
                rad_to_deg(atan2(probeArray[curCaptureProbe - 1].halfWidths.y, probeArray[curCaptureProbe - 1].halfWidths.z)) * 2.f;
              bx::mtxProj(cm_proj,
                          fovy,
                          probeArray[curCaptureProbe - 1].halfWidths.x / probeArray[curCaptureProbe - 1].halfWidths.y,
                          0.001f,
                          probeArray[curCaptureProbe - 1].halfWidths.z,
                          false);
            }
          } else {
            bx::mtxProj(cm_proj, 90.f, 1.f, cam.nearPlane, cam.farPlane, false);
          }
          vec3_cross(&fw, &r, &up);
          v_mtx_tmp[0] = r.x;
          v_mtx_tmp[1] = r.y;
          v_mtx_tmp[2] = r.z;
          v_mtx_tmp[3] = 0.f;
          v_mtx_tmp[4] = up.x;
          v_mtx_tmp[5] = up.y;
          v_mtx_tmp[6] = up.z;
          v_mtx_tmp[7] = 0.f;
          v_mtx_tmp[8] = fw.x;
          v_mtx_tmp[9] = fw.y;
          v_mtx_tmp[10] = fw.z;
          v_mtx_tmp[11] = 0.f;
          v_mtx_tmp[12] = cur_cam_pos.x;
          v_mtx_tmp[13] = cur_cam_pos.y;
          v_mtx_tmp[14] = cur_cam_pos.z;
          v_mtx_tmp[15] = 1.f;
          bx::mtxInverse(cm_view[i], v_mtx_tmp);

          bgfx::setViewRect(RENDER_PASS_CUBEMAP_FACE_PX + i, 0, 0, m_width, m_width);
          bgfx::setViewTransform(RENDER_PASS_CUBEMAP_FACE_PX + i, cm_view[i], cm_proj);
          bgfx::setViewFrameBuffer(RENDER_PASS_CUBEMAP_FACE_PX + i, cubemapFaceTarget[i]);

          bgfx::setViewRect(RENDER_PASS_CUBEMAP_IR_CONV_FACE_PX + i, 0, 0, IR_MAP_DIM, IR_MAP_DIM);
          bgfx::setViewTransform(RENDER_PASS_CUBEMAP_IR_CONV_FACE_PX + i, idt, orthoProj);
          bgfx::setViewFrameBuffer(RENDER_PASS_CUBEMAP_IR_CONV_FACE_PX + i, irCubemapFaceTarget[i]);

          for (uint32_t j = 0; j < SIR_MIP_COUNT; ++j) {
            uint32_t mip_dims = SIR_MAP_DIM >> j;
            bgfx::setViewRect(RENDER_PASS_CUBEMAP_SIR_CONV_FIRST + (i * SIR_MIP_COUNT) + j, 0, 0, mip_dims, mip_dims);
            bgfx::setViewTransform(RENDER_PASS_CUBEMAP_SIR_CONV_FIRST + (i * SIR_MIP_COUNT) + j, idt, orthoProj);
            bgfx::setViewFrameBuffer(RENDER_PASS_CUBEMAP_SIR_CONV_FIRST + (i * SIR_MIP_COUNT) + j,
                                     sirCubemapFaceTarget[i][j]);
          }
        }

        // submit any cubemap stuff
        // disable lights, disable ibl
        uniformDataTmp[0] = 0.f;
        uniformDataTmp[1] = 0.f;
        bgfx::setUniform(dynLightsUniform, uniformDataTmp);

        for (uint32_t i = 0; i < 6; ++i) {
          mesh_submit(&m_mesh,
                      RENDER_PASS_CUBEMAP_FACE_PX + i,
                      ShaderParams[dbgShader].solidProg,
                      defaultMask,
                      mTmp,
                      BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_DEPTH_TEST_LEQUAL |
                        BGFX_STATE_DEPTH_WRITE | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA,
                      handles,
                      buffer,
                      env_texs,
                      2);
          for (uint32_t o = 0; o < TOTAL_ORBS; ++o) {
            float model[16];
            bx::mtxScale(model, 2.5f);
            model[12] = Orbs[o].position.x;
            model[13] = Orbs[o].position.y;
            model[14] = Orbs[o].position.z;
            bgfx::setUniform(albedoUnform, Orbs[o].colour);
            float mr[4] = {Orbs[o].metal ? 1.f : 0.f, Orbs[o].roughnes};
            bgfx::setUniform(metalRoughUniform, mr);
            bgfx::setBuffer(10, buffer[0], bgfx::Access::Read);
            bgfx::setBuffer(11, buffer[1], bgfx::Access::Read);
            bgfx::setBuffer(12, buffer[2], bgfx::Access::Read);
            bgfx::setBuffer(13, buffer[3], bgfx::Access::Read);
            bgfx::setBuffer(14, buffer[4], bgfx::Access::Read);
            mesh_submit(&m_orb,
                        RENDER_PASS_CUBEMAP_FACE_PX + i,
                        ShaderParams[dbgShader].solidProgNoTex,
                        model,
                        BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_DEPTH_TEST_LEQUAL |
                          BGFX_STATE_DEPTH_WRITE | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA);
          }
        }

        float faceUniform[4];
        // irradiance
        for (uint32_t i = 0; i < 6; ++i) {
          faceUniform[0] = (float)i;
          bgfx::setState(BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_MSAA);
          bgfx::setUniform(cubeFaceUniform, faceUniform);
          bgfx::setTexture(6, environmentMapTex, cubemapFaceTexture);
          screenSpaceQuad(IR_MAP_DIM, IR_MAP_DIM, s_texelHalf, m_caps->originBottomLeft);
          bgfx::submit(RENDER_PASS_CUBEMAP_IR_CONV_FACE_PX + i, irConvolveProg);
        }

        // specular irradiance
        for (uint32_t i = 0; i < 6; ++i) {
          faceUniform[0] = (float)i;
          for (uint32_t j = 0; j < SIR_MIP_COUNT; ++j) {
            faceUniform[1] = j / (float)SIR_MIP_COUNT; // set this mip's roughness
            bgfx::setState(BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_MSAA);
            bgfx::setUniform(cubeFaceUniform, faceUniform);
            bgfx::setTexture(6, environmentMapTex, cubemapFaceTexture);
            screenSpaceQuad(SIR_MAP_DIM, SIR_MAP_DIM, s_texelHalf, m_caps->originBottomLeft);
            bgfx::submit(RENDER_PASS_CUBEMAP_SIR_CONV_FIRST + (i * SIR_MIP_COUNT) + j, sirConvolveProg);
          }
        }

        // copy to non-vram
        bgfx::blit(RENDER_PASS_GPU_COPY_LAST, bdrfConvolveTextureRead, 0, 0, 0, 0, bdrfConvolveTexture, 0, 0, 0, 0);
        dbgCaptureReadyFrame = bgfx::readTexture(bdrfConvolveTextureRead, (uint8_t*)bdrfTextureReadCPUData, 0);
        for (uint32_t i = 0; i < 6; ++i) {
          bgfx::blit(RENDER_PASS_GPU_COPY_LAST, cubemapFaceTextureRead[i], 0, 0, 0, 0, cubemapFaceTexture, 0, 0, 0, i);
          bgfx::blit(
            RENDER_PASS_GPU_COPY_LAST, irCubemapFaceTextureRead[i], 0, 0, 0, 0, irCubemapFaceTexture, 0, 0, 0, i);
          // no mip chian to worry about
          dbgCaptureReadyFrame =
            bgfx::readTexture(irCubemapFaceTextureRead[i],
                              (half_t*)irCubemapFaceTextureReadCPUData + i * (IR_MAP_DIM * IR_MAP_DIM * 4),
                              0);
          dbgCaptureReadyFrame = bgfx::readTexture(
            cubemapFaceTextureRead[i], (half_t*)cubemapFaceTextureReadCPUData + i * (m_width * m_width * 4), 0);
          for (uint32_t j = 0; j < SIR_MIP_COUNT; ++j) {
            bgfx::blit(RENDER_PASS_GPU_COPY_LAST,
                       sirCubemapFaceTextureRead[i][j],
                       0,
                       0,
                       0,
                       0,
                       sirCubemapFaceTexture[j],
                       0,
                       0,
                       0,
                       i);
            dbgCaptureReadyFrame =
              bgfx::readTexture(sirCubemapFaceTextureRead[i][j],
                                (uint8_t*)sirCubemapFaceTextureReadCPUData + sirMipFaceWriteOffsetLookUp[j][i],
                                0);
          }
        }

        dbgWaitingOnReadback = true;
      }

      // Use debug font to print information about this example.
      if (dbgDebugStats) {
        bgfx::setDebug(BGFX_DEBUG_PROFILER | BGFX_DEBUG_STATS | BGFX_DEBUG_TEXT);
      } else {
        bgfx::setDebug(BGFX_DEBUG_TEXT);
      }

      uint32_t c = 0;
      if (dbgCaptureCubemap) {
        uint32_t current = (envMapCurrentCell[2] * envMapGridDim[1] * envMapGridDim[0]) +
                           (envMapCurrentCell[1] * envMapGridDim[0]) + envMapCurrentCell[0];
        uint32_t total = envMapGridDim[2] * envMapGridDim[1] * envMapGridDim[0];
        bgfx::dbgTextPrintf(0,
                            0,
                            0x6f,
                            "Capturing Cubemap (%d, %d, %d)",
                            envMapCurrentCell[0],
                            envMapCurrentCell[1],
                            envMapCurrentCell[2]);
        bgfx::dbgTextPrintf(0, 1, 0x6f, "%d of %d", current, total);
      } else {
        bgfx::dbgTextPrintf(0, 0, 0x6f, "Current Mode: %s", ShaderParams[dbgShader].description);
        bgfx::dbgTextPrintf(0,
                            1,
                            0x6f,
                            "Total Lights: %d. Cells (%d,%d,%d)",
                            LIGHT_COUNT,
                            envMapGridDim[0],
                            envMapGridDim[1],
                            envMapGridDim[2]);
      }
      if (!enableComputeLightCull) {
        bgfx::dbgTextPrintf(0, 2, 0x6f, "Current Cell %d. Debug Lights:", dbgCell);
        uint16_t* cur_db_l = dbg_light_list;
        while (*cur_db_l != 0xffff) {
          bgfx::dbgTextPrintf(c * 4, 3, 0x6f, "%3d,", *cur_db_l);
          c++;
          cur_db_l++;
        }
      }

      plane_t planes[6];
      extractFrustumPlanes(&saved_cam_params, planes);

      if (dbgDrawLights) {
        ddPush();
        if (dbgDrawLightsWire)
          ddSetWireframe(true);
        else
          ddSetWireframe(false);
        ddSetColor(0xFFFF00FF);
        float rot_mtx[16];
        if (dbgDebugMoveLights) {
          for (uint32_t i = 0, n = LIGHT_COUNT; i < n; ++i) {
            vec3_t t = {0.f, 1.f, 0.f}, t2;
            bx::mtxRotateXYZ(rot_mtx, lights[i].r[0] + time, lights[i].r[1] + time, lights[i].r[2] + time);
            bx::vec3MulMtx(t2.v, t.v, rot_mtx);
            vec3_add(&t, &lights[i].p, &t2);
            Sphere ll = {t.x, t.y, t.z, lights[i].radius};
            ddDraw(ll);
          }
        } else {
          for (uint32_t i = 0, n = LIGHT_COUNT; i < n; ++i) {
            Sphere ll = {lights[i].p.x, lights[i].p.y, lights[i].p.z, lights[i].radius};
            ddDraw(ll);
          }
        }
        ddPop();
      }

      if (dbgShowReflectionProbes) {
        ddPush();
        ddSetWireframe(true);
        ddSetColor(0xFFFF00FF);
        for (uint32_t i = 0, n = totalProbes; i < n; ++i) {
          Aabb p = {
            probeArray[i].position.x - probeArray[i].halfWidths.x,
            probeArray[i].position.y - probeArray[i].halfWidths.y,
            probeArray[i].position.z - probeArray[i].halfWidths.z,
            probeArray[i].position.x + probeArray[i].halfWidths.x,
            probeArray[i].position.y + probeArray[i].halfWidths.y,
            probeArray[i].position.z + probeArray[i].halfWidths.z,
          };
          ddDraw(p);
        }
        ddPop();
      }

      if (dbgDrawZBin) {
        ddSetWireframe(false);
        ddMoveTo(0.f, 0.f, 0.f);
        ddSetColor(0x7F00FFFF);
        vec3_t cam_pos = {saved_cam_params.x, saved_cam_params.y, saved_cam_params.z};
        float  z = 0.f;
        for (uint32_t i = 0; i < Z_BUCKETS; ++i, z += zstep) {
          if (dbgBucket == i) {
            vec3_t ff, cc;
            vec3_scale(&ff, &saved_cam_params.forward, z);
            vec3_add(&cc, &cam_pos, &ff);
            float size = 2.f * tanf(deg_to_rad(saved_cam_params.fovy * .5f)) * z * saved_cam_params.aspect;
            ddDrawQuad(saved_cam_params.forward.v, cc.v, size);

            size = 2.f * tanf(deg_to_rad(saved_cam_params.fovy * .5f)) * (z + zstep) * saved_cam_params.aspect;
            vec3_scale(&ff, &saved_cam_params.forward, z + zstep);
            vec3_add(&cc, &cam_pos, &ff);
            ddDrawQuad(saved_cam_params.forward.v, cc.v, size);

            uint16_t hiword = (zbins[i] & 0xFFFF0000) >> 16;
            uint16_t loword = zbins[i] & 0xFFFF;

            ddSetWireframe(true);
            ddSetColor(0x7F0000FF);
            c = 0;
            bgfx::dbgTextPrintf(0, 4, 0x6f, "Z bin");
            for (uint16_t zb = loword; zb <= hiword; ++zb) {
              vec3_t vs_lp;
              bx::vec3MulMtx(vs_lp.v, viewspaceLights[zb].p.v, i_view);
              Sphere ll = {vs_lp.x, vs_lp.y, vs_lp.z, viewspaceLights[zb].radius};
              ddDraw(ll);
              bgfx::dbgTextPrintf(c * 4, 5, 0x6f, "%3d,", zb);
              c++;
            }

            break;
          }
        }
      }

      ddEnd();
      imguiEndFrame();

      // Advance to next frame. Rendering thread will be kicked to
      // process submitted rendering primitives.
      frameReturn = bgfx::frame();
      ++frameID;

      return true;
    }

    return false;
  }

public:
  ExampleHelloWorld(const char* _name, const char* _description) : entry::AppI(_name, _description) {}
};

ENTRY_IMPLEMENT_MAIN(ExampleHelloWorld, "Example", "something something darkside");
