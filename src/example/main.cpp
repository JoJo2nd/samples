

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

static float s_texelHalf = 0.0f;

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

struct vec3_t {
  union {
    float v[3];
    struct {
      float x, y, z;
    };
  };
};

void vec3_add(vec3_t* d, vec3_t const* a, vec3_t const* b) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] = a->v[i] + b->v[i];
  }
}

void vec3_add(vec3_t* d, vec3_t const* b) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] += b->v[i];
  }
}

void vec3_sub(vec3_t* d, vec3_t const* a, vec3_t const* b) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] = a->v[i] - b->v[i];
  }
}

void vec3_sub(vec3_t* d, vec3_t const* b) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] -= b->v[i];
  }
}

void vec3_scale(vec3_t* d, vec3_t const* ss, float s) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] = ss->v[i] * s;
  }
}

float vec3_dot(vec3_t const* a, vec3_t const* b) {
  float d = 0;
  for (uint32_t i = 0; i < 3; ++i) {
    d += a->v[i] * b->v[i];
  }
  return d;
}

void vec3_cross(vec3_t* d, vec3_t const* a, vec3_t const* b) {
  d->v[0] = ((a->v[1] * b->v[2]) - (a->v[2] * b->v[1]));
  d->v[1] = ((a->v[2] * b->v[0]) - (a->v[0] * b->v[2]));
  d->v[2] = ((a->v[0] * b->v[1]) - (a->v[1] * b->v[0]));
}

void vec3_norm(vec3_t* d, vec3_t* s) {
  float dd = 1.f / sqrtf(s->v[0] * s->v[0] + s->v[1] * s->v[1] + s->v[2] * s->v[2]);
  d->v[0] = s->v[0] * dd;
  d->v[1] = s->v[1] * dd;
  d->v[2] = s->v[2] * dd;
}

struct CameraParams {
  float  fovy, aspect, nearPlane, farPlane;
  float  x, y, z;
  float  viewProj[16];
  float  view[16];
  vec3_t up, right, forward;
};

struct plane_t {
  vec3_t n;
  float  d;
};

void plane_build(plane_t* dst, vec3_t const* a, vec3_t const* b, vec3_t const* c) {
  vec3_t t1, t2, t3;
  vec3_sub(&t1, b, a);
  vec3_sub(&t2, c, a);
  vec3_cross(&t3, &t1, &t2);
  vec3_norm(&dst->n, &t3);
  dst->d = vec3_dot(&dst->n, a);
}

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

  char const* fsDbgVSPath;
  char const* fsDbgPSPath;

  char const* lightCullCSPath;

  char const* description;

  bgfx::ProgramHandle solidProg;
  bgfx::ProgramHandle fullscreenProg;
  bgfx::ProgramHandle csLightCull;
  bgfx::ProgramHandle zfillProg;
};

RunParams ShaderParams[SOLID_PROGS] = {
  {VS_ASSET_PATH,
   FS_ASSET_PATH,
   nullptr,
   nullptr,
   nullptr,
   "CPU Z Bin, CPU Tile Light Cull",
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE},
  {VS_ASSET_PATH,
   FS_WCS_ASSET_PATH,
   nullptr,
   nullptr,
   CS_LIGHT_CULL_PATH_NO_OPT,
   "CPU Z Bin, GPU Tile Light Cull (Slow)",
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE},
  {VS_ASSET_PATH,
   FS_WCS_ASSET_PATH,
   nullptr,
   nullptr,
   CS_LIGHT_CULL_PATH,
   "CPU Z Bin, GPU Tile Light Cull",
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE},
  {VS_ASSET_PATH,
   FS_WCS_ASSET_PATH,
   VS_DEBUG_ASSET_PATH,
   FS_FSDEBUG_WCS_ASSET_PATH,
   CS_LIGHT_CULL_PATH,
   "CPU Z Bin, GPU Tile Light Cull (Debug Overlay)",
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE},
  {VS_ASSET_PATH,
   FS_ASSET_PATH,
   VS_DEBUG_ASSET_PATH,
   FS_FSDEBUG_WCS_ASSET_PATH,
   nullptr,
   "CPU Z Bin, CPU Tile Light Cull (Debug Overlay)",
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE},
  {VS_ASSET_PATH,
   FS_DEBUG01_ASSET_PATH,
   nullptr,
   nullptr,
   nullptr,
   "Z Bin light evals",
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE},
  {VS_ASSET_PATH,
   FS_DEBUG02_ASSET_PATH,
   nullptr,
   nullptr,
   nullptr,
   "Tiled light evals",
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE,
   BGFX_INVALID_HANDLE},
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
  util::MeshMaterials m_materials;

  bgfx::ProgramHandle logLuminanceAvProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle tonemapProg = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle luminancePixelProg = BGFX_INVALID_HANDLE;

  bgfx::UniformHandle timeUniform;
  bgfx::UniformHandle sunlightUniform;
  bgfx::UniformHandle lightingParamsUniform;
  bgfx::UniformHandle csViewPrams;
  bgfx::UniformHandle csLightData;
  bgfx::UniformHandle exposureParamsUniform;
  bgfx::UniformHandle offsetUniform;

  bgfx::UniformHandle baseTex;
  bgfx::UniformHandle normalTex;
  bgfx::UniformHandle roughTex;
  bgfx::UniformHandle metalTex;
  bgfx::UniformHandle maskTex;
  bgfx::UniformHandle luminanceTex;

  bgfx::DynamicVertexBufferHandle lightPositionBuffer;
  bgfx::DynamicVertexBufferHandle zBinBuffer;
  bgfx::DynamicVertexBufferHandle lightGridBuffer;
  bgfx::DynamicVertexBufferHandle lightListBuffer;
  bgfx::DynamicVertexBufferHandle lightGridFatBuffer;
  bgfx::DynamicVertexBufferHandle workingLuminanceBuffer;
  bgfx::DynamicVertexBufferHandle averageLuminanceBuffer;

  bgfx::FrameBufferHandle zPrePassTarget;
  bgfx::FrameBufferHandle mainTarget;
  bgfx::FrameBufferHandle luminanceTargets[16];

  bgfx::TextureHandle colourTarget;
  bgfx::TextureHandle luminanceMips[16];

  bgfx::Caps const* m_caps;

  LightInit* lights = nullptr; //[LIGHT_COUNT];
  uint16_t*  grid = nullptr;

  uint32_t frameID = 0;
  uint32_t numLuminanceMips = 0;

  float time = 0.f;

  float sunlightAngle[4] = {0.f, 0.f, 0.f, 1.f};
  float curExposure = .18f;
  float curExposureMin = .0001f;
  float curExposureMax = 5.f;

  int32_t dbgCell = 0;
  int32_t dbgBucket = 0;
  int32_t dbgShader = 2;

  bool dbgLightGrid = false;
  bool dbgLightGridFrustum = false;
  bool dbgUpdateFrustum = true;
  bool dbgDrawLights = false;
  bool dbgDrawLightsWire = false;
  bool dbgDrawZBin = false;
  bool dbgDebugStats = false;
  bool dbgDebugMoveLights = true;
  bool dbgPixelAvgLuminance = true;

  void unloadAssetData(RunParams* data, uint32_t data_count) {
    for (uint32_t i = 0, n = data_count; i < n; ++i) {
      if (bgfx::isValid(data[i].solidProg)) {
        bgfx::destroy(data[i].solidProg);
        data[i].solidProg = BGFX_INVALID_HANDLE;
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
  }

  void loadAssetData(RunParams* data, uint32_t data_count) {
    unloadAssetData(data, data_count);
    for (uint32_t i = 0, n = data_count; i < n; ++i) {
      if (data[i].meshVSPath && data[i].meshPSPath) {
        bgfx::Memory const* vs = loadMem(data[i].meshVSPath);
        bgfx::Memory const* ps = loadMem(data[i].meshPSPath);
        data[i].solidProg = bgfx::createProgram(bgfx::createShader(vs), bgfx::createShader(ps), true);
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

    bgfx::Memory const* cs = loadMem(LUMINANCE_AVG_ASSET_PATH);
    logLuminanceAvProg = bgfx::createProgram(bgfx::createShader(cs), true);
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
    if (bx::open(reader, SPONZA_MATERIAL_ASSET_PATH)) {
      util::material_load(&m_materials, reader);
    }
    mesh_bind_materials(&m_mesh, &m_materials);

    loadAssetData(ShaderParams, SOLID_PROGS);

    float ab_min[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float ab_max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (size_t i = 0, n = m_mesh.m_groups.size(); i < n; ++i) {
      for (uint32_t j = 0; j < 3; ++j) {
        ab_min[j] = MIN(m_mesh.m_groups[i].m_aabb.m_min[j] * GLOBAL_SCALE, ab_min[j]);
        ab_max[j] = MAX(m_mesh.m_groups[i].m_aabb.m_max[j] * GLOBAL_SCALE, ab_max[j]);
      }
    }

    timeUniform = bgfx::createUniform("u_gameTime", bgfx::UniformType::Vec4);
    sunlightUniform = bgfx::createUniform("u_sunlight", bgfx::UniformType::Vec4);
    lightingParamsUniform = bgfx::createUniform("u_bucketParams", bgfx::UniformType::Vec4);
    csViewPrams = bgfx::createUniform("u_params", bgfx::UniformType::Vec4, 2);
    csLightData = bgfx::createUniform("u_lightData", bgfx::UniformType::Vec4);
    exposureParamsUniform = bgfx::createUniform("u_exposureParams", bgfx::UniformType::Vec4);
    offsetUniform = bgfx::createUniform("u_offset", bgfx::UniformType::Vec4, 3);

    baseTex = bgfx::createUniform("s_base", bgfx::UniformType::Int1);
    normalTex = bgfx::createUniform("s_normal", bgfx::UniformType::Int1);
    roughTex = bgfx::createUniform("s_roughness", bgfx::UniformType::Int1);
    metalTex = bgfx::createUniform("s_metallic", bgfx::UniformType::Int1);
    maskTex = bgfx::createUniform("s_mask", bgfx::UniformType::Int1);
    luminanceTex = bgfx::createUniform("s_luminance", bgfx::UniformType::Int1);

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
      (ab_max[0] - ab_min[0]), (ab_max[1] - ab_min[1]), (ab_max[2] - ab_min[2]),
    };
    for (uint32_t i = 0; i < LIGHT_COUNT; ++i) {
      lights[i].p.x = (ab_min[0]) + ((float)rand() / RAND_MAX) * range[0];
      lights[i].p.y = (ab_min[1]) + ((float)rand() / RAND_MAX) * range[1];
      lights[i].p.z = (ab_min[2]) + ((float)rand() / RAND_MAX) * range[2];
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
    // BGFX can handle sRGB render targets, so use a 16bit float and resolve out end frame
    colourTarget =
      bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT);
    bgfx::Attachment main_attachments[2] = {{colourTarget, 0, 0}, {bgfx::getTexture(zPrePassTarget, 0), 0, 0}};
    mainTarget = bgfx::createFrameBuffer(2, main_attachments);

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

    // Set view 0 clear state.
    bgfx::setViewClear(RENDER_PASS_COMPUTE, BGFX_CLEAR_NONE, 0x303030ff, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_COMPUTE, "Compute Pass");
    bgfx::setViewMode(RENDER_PASS_COMPUTE, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(RENDER_PASS_ZPREPASS, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_ZPREPASS, "Z PrePass");
    bgfx::setViewMode(RENDER_PASS_ZPREPASS, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(RENDER_PASS_ZPREPASS, zPrePassTarget);
    bgfx::setViewClear(RENDER_PASS_SOLID, BGFX_CLEAR_COLOR, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_SOLID, "Solid Pass");
    bgfx::setViewMode(RENDER_PASS_SOLID, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(RENDER_PASS_SOLID, mainTarget);

    for (uint32_t i = 0; i < numLuminanceMips; ++i) {
      bgfx::setViewClear(RENDER_PASS_LUMINANCE_START + i, BGFX_CLEAR_NONE, 0, 1.0f, 0);
      bgfx::setViewName(RENDER_PASS_LUMINANCE_START + i, "Luminance Pass");
      bgfx::setViewMode(RENDER_PASS_LUMINANCE_START + i, bgfx::ViewMode::Sequential);
      bgfx::setViewFrameBuffer(RENDER_PASS_LUMINANCE_START + i, luminanceTargets[i]);
    }

    bgfx::setViewClear(RENDER_PASS_TONEMAP, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_TONEMAP, "Tonemap Pass");
    bgfx::setViewMode(RENDER_PASS_TONEMAP, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(RENDER_PASS_DEBUG, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_DEBUG, "Debug Pass");
    bgfx::setViewMode(RENDER_PASS_DEBUG, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(RENDER_PASS_DEBUG, mainTarget);
    bgfx::setViewClear(RENDER_PASS_2DDEBUG, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewName(RENDER_PASS_2DDEBUG, "2D Debug Pass");
    bgfx::setViewMode(RENDER_PASS_2DDEBUG, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(RENDER_PASS_2DDEBUG, mainTarget);

    ddInit();
    imguiCreate();
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
      return lhs->p.z - lhs->radius < rhs->p.z - rhs->radius ? -1 : 1;
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
      uint16_t min_z =
        (uint16_t)MIN(fabsf((viewSpaceLights[cur_l].p.z - viewSpaceLights[cur_l].radius) / z_step), Z_BUCKETS - 1);
      uint16_t max_z =
        (uint16_t)MIN(fabs((viewSpaceLights[cur_l].p.z + viewSpaceLights[cur_l].radius) / z_step), Z_BUCKETS - 1);
      for (uint16_t cz = min_z; cz <= max_z; ++cz) {
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

      if (ImGui::Begin("Debug Options")) {
        if (ImGui::Button("Reload Shaders")) {
          loadAssetData(ShaderParams, SOLID_PROGS);
        }
        if (ImGui::CollapsingHeader("Render Control")) {
          ImGui::SliderFloat3("Sunlight Angle", sunlightAngle, 0.f, 360.f);
          ImGui::SliderFloat("Sunlight Power", sunlightAngle + 3, 0.5f, 10.f);
          ImGui::SliderFloat("Exposure Key", &curExposure, 0.0001f, 10.f);
          ImGui::SliderFloat("Exposure Min", &curExposureMin, 0.0001f, 1.f);
          ImGui::SliderFloat("Exposure Max", &curExposureMax, curExposureMin, 10.f);
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
        }
      }
      ImGui::End();

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

      // setup sunlight
      float  sunlight[4] = {0.f, 0.f, 1.f, sunlightAngle[3]};
      vec3_t sunforward = {0.f, 0.f, 1.f}, sunforward2;
      bx::mtxRotateXYZ(mTmp, deg_to_rad(sunlightAngle[0]), deg_to_rad(sunlightAngle[1]), deg_to_rad(sunlightAngle[2]));
      bx::vec3MulMtx(sunforward2.v, sunforward.v, mTmp);

      // Set sunlight in view space (requires inverse view transpose)
      bx::mtxInverse(mTmp, view);
      vec3_norm(&cam.up, (vec3_t*)&mTmp[4]);
      vec3_norm(&cam.right, (vec3_t*)&mTmp[0]);
      vec3_norm(&cam.forward, (vec3_t*)&mTmp[8]);
      bx::mtxTranspose(iViewX, mTmp);

      bx::vec3MulMtx(sunforward.v, sunforward2.v, iViewX);
      // Set sunlight in view space
      vec3_norm(&sunforward2, &sunforward);
      sunlight[0] = sunforward2.x;
      sunlight[1] = sunforward2.y;
      sunlight[2] = sunforward2.z;
      bgfx::setUniform(sunlightUniform, sunlight);

      bx::mtxScale(mTmp, GLOBAL_SCALE, GLOBAL_SCALE, GLOBAL_SCALE);

      float proj[16], i_view[16];
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

      bgfx::dbgTextClear();
      ddBegin(RENDER_PASS_DEBUG);
      ddSetLod(2);

      static CameraParams saved_cam_params;
      if (dbgUpdateFrustum) {
        // save = false;
        saved_cam_params = cam;
      }
      bx::mtxInverse(i_view, saved_cam_params.view);

      LightInfo viewspaceLights[LIGHT_COUNT];
      uint32_t  zbins[Z_BUCKETS];
      uint16_t  dbg_light_list[LIGHT_COUNT + 1];
      float     zstep;
      zbinLights(viewspaceLights, &saved_cam_params, lights, LIGHT_COUNT, zbins, &zstep);

      float lightingParams[4] = {
        zstep, (float)GRID_SIZE, (float)((m_width + (GRID_SIZE_M_ONE)) / GRID_SIZE), (float)(m_height)};
      bgfx::setUniform(lightingParamsUniform, lightingParams);

      bool enableCPUUpdateLightGrid = !bgfx::isValid(ShaderParams[dbgShader].csLightCull);
      bool enableComputeLightCull = bgfx::isValid(ShaderParams[dbgShader].csLightCull);
      // use viewspaceLights which are sorted by distance from the camera
      if (enableCPUUpdateLightGrid) {
        updateLightingGrid(viewspaceLights, LIGHT_COUNT, &saved_cam_params, grid, dbg_light_list);
      }

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
      // update the bunny instances
      uint16_t instanceStride = 64;

      // Set view 0 default viewport.
      bgfx::setViewRect(RENDER_PASS_COMPUTE, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_COMPUTE, view, proj);
      bgfx::setViewRect(RENDER_PASS_ZPREPASS, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_ZPREPASS, view, proj);
      bgfx::setViewRect(RENDER_PASS_SOLID, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_SOLID, view, proj);

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
      bgfx::setViewRect(RENDER_PASS_DEBUG, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_DEBUG, view, proj);
      bgfx::setViewRect(RENDER_PASS_2DDEBUG, 0, 0, m_width, m_height);
      bgfx::setViewTransform(RENDER_PASS_2DDEBUG, idt, orthoProj);

      bgfx::setViewFrameBuffer(RENDER_PASS_ZPREPASS, zPrePassTarget);
      bgfx::setViewFrameBuffer(RENDER_PASS_SOLID, mainTarget);
      // The remaining passes render to the back buffer

      uint64_t state = 0;

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

      if (bgfx::isValid(ShaderParams[dbgShader].zfillProg)) {
        // Set instance data buffer.
        mesh_submit(&m_mesh,
                    RENDER_PASS_ZPREPASS,
                    ShaderParams[dbgShader].zfillProg,
                    mTmp,
                    BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_DEPTH_WRITE | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA,
                    handles,
                    buffer);
      }
      // Set instance data buffer.
      mesh_submit(&m_mesh,
                  RENDER_PASS_SOLID,
                  ShaderParams[dbgShader].solidProg,
                  mTmp,
                  BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_DEPTH_TEST_LEQUAL |
                    BGFX_STATE_DEPTH_WRITE | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA,
                  handles,
                  buffer);

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
      float exposure_params[4] = {curExposureMin, curExposureMax, curExposure, 0.f};
      bgfx::setUniform(exposureParamsUniform, exposure_params);
      bgfx::setState(BGFX_STATE_RGB_WRITE | BGFX_STATE_MSAA);
      screenSpaceQuad((float)m_width, (float)m_height, s_texelHalf, m_caps->originBottomLeft);
      bgfx::submit(RENDER_PASS_TONEMAP, tonemapProg);

      // Use debug font to print information about this example.
      if (dbgDebugStats) {
        bgfx::setDebug(BGFX_DEBUG_PROFILER | BGFX_DEBUG_STATS | BGFX_DEBUG_TEXT);
      } else {
        bgfx::setDebug(BGFX_DEBUG_TEXT);
      }

      uint32_t c = 0;
      bgfx::dbgTextPrintf(0, 0, 0x6f, "Current Mode: %s", ShaderParams[dbgShader].description);
      bgfx::dbgTextPrintf(0, 1, 0x6f, "Total Lights: %d", LIGHT_COUNT);
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
      bgfx::frame();
      ++frameID;

      return true;
    }

    return false;
  }

public:
  ExampleHelloWorld(const char* _name, const char* _description) : entry::AppI(_name, _description) {}
};

ENTRY_IMPLEMENT_MAIN(ExampleHelloWorld, "Example", "something something darkside");
