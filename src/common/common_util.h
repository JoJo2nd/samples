
#pragma once

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <vector>
#include "bx/readerwriter.h"
#include "ktx.h"

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)
#define CLAMP(v, a, b) (v < a ? a : v > b ? b : v)

typedef uint16_t half_t;

namespace util {

struct MeshMaterials;

struct Aabb {
  float m_min[3];
  float m_max[3];
};

struct Obb {
  float m_mtx[16];
};

struct Sphere {
  float m_center[3];
  float m_radius;
};

struct Primitive {
  char     m_material[64];
  int32_t  m_materialIndex; // -1 when non
  uint32_t m_startIndex;
  uint32_t m_numIndices;
  uint32_t m_startVertex;
  uint32_t m_numVertices;

  Sphere m_sphere;
  Aabb   m_aabb;
  Obb    m_obb;
};

typedef std::vector<Primitive> PrimitiveArray;

struct Group {
  Group() { reset(); }

  void reset() {
    m_vbh.idx = bgfx::kInvalidHandle;
    m_ibh.idx = bgfx::kInvalidHandle;
    m_prims.clear();
  }

  bgfx::VertexBufferHandle m_vbh;
  bgfx::IndexBufferHandle  m_ibh;
  Sphere                   m_sphere;
  Aabb                     m_aabb;
  Obb                      m_obb;
  PrimitiveArray           m_prims;
};

typedef std::vector<Group> GroupArray;

struct Mesh {
  bgfx::VertexDecl m_decl;
  GroupArray       m_groups;
  MeshMaterials*   materials;
};

struct Texture {
  uint32_t            stage;
  bgfx::UniformHandle uniform;
  bgfx::TextureHandle tex;
};

void mesh_load(Mesh* m, bx::ReaderSeekerI* _reader);
void mesh_bind_materials(Mesh* m, MeshMaterials* mats);
void mesh_unload(Mesh* m);
void mesh_submit(Mesh*                            m,
                 uint8_t                          _id,
                 bgfx::ProgramHandle              _program,
                 const float*                     _mtx,
                 uint64_t                         _state,
                 bgfx::UniformHandle*             textures,
                 bgfx::DynamicVertexBufferHandle* buffers,
                 Texture*                         extraTex,
                 uint32_t                         extraTexCount);
void mesh_submit(Mesh*                            m,
                 uint8_t                          _id,
                 bgfx::ProgramHandle              _program,
                 bgfx::TextureHandle              defaultTex,
                 const float*                     _mtx,
                 uint64_t                         _state,
                 bgfx::UniformHandle*             textures,
                 bgfx::DynamicVertexBufferHandle* buffers,
                 Texture*                         extraTex,
                 uint32_t                         extraTexCount);
void mesh_submit(Mesh* m, uint8_t _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state);

#define MATERIAL_HEADER_MAGIC (('M') | ('a' << 8) | ('t' << 16) | ('_' << 24))

#define MATERIAL_BASE (0)
#define MATERIAL_METALLIC (1)
#define MATERIAL_NORMAL (2)
#define MATERIAL_ROUGHNESS (3)
#define MATERIAL_MASK (4)
#define MATERIAL_MAX (5)

#define MATERIAL_BASE_FLAG (1 << MATERIAL_BASE)
#define MATERIAL_METALLIC_FLAG (1 << MATERIAL_METALLIC)
#define MATERIAL_NORMAL_FLAG (1 << MATERIAL_NORMAL)
#define MATERIAL_ROUGHNESS_FLAG (1 << MATERIAL_ROUGHNESS)
#define MATERIAL_MASK_FLAG (1 << MATERIAL_MASK)

struct MaterialHeader {
  uint32_t magic;
  uint32_t nMaterials;
  uint32_t strTableOffset;
  uint32_t pad;
};

struct MaterialIO {
  uint32_t flags; //
  uint32_t nameOffset;
  uint32_t baseOffset;
  uint32_t normalOffset;
  uint32_t roughnesOffset;
  uint32_t metallicOffset;
  uint32_t maskOffset;
};

struct MaterialData {
  char const* name;
  uint32_t    flags;
  union {
    struct {
      bgfx::TextureHandle base;
      bgfx::TextureHandle metallic;
      bgfx::TextureHandle normal;
      bgfx::TextureHandle roughnes;
      bgfx::TextureHandle mask;
    };
    bgfx::TextureHandle all[MATERIAL_MAX];
  };
};

struct MeshMaterials {
  char*         stringTable;
  uint32_t      materialsCount;
  MaterialData* materials;
};

void material_load(MeshMaterials* m, bx::FileReaderI* reader);
void material_unload(MeshMaterials* m);

inline half_t float_to_half(float f) {
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

  return ((half_t)((sign << 15) | (exponent << 10) | mantissa));
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

typedef struct vec3_t vec3_t;

inline void vec3_add(vec3_t* d, vec3_t const* a, vec3_t const* b) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] = a->v[i] + b->v[i];
  }
}

inline void vec3_add(vec3_t* d, vec3_t const* b) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] += b->v[i];
  }
}

inline void vec3_sub(vec3_t* d, vec3_t const* a, vec3_t const* b) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] = a->v[i] - b->v[i];
  }
}

inline void vec3_sub(vec3_t* d, vec3_t const* b) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] -= b->v[i];
  }
}

inline void vec3_scale(vec3_t* d, vec3_t const* ss, float s) {
  for (uint32_t i = 0; i < 3; ++i) {
    d->v[i] = ss->v[i] * s;
  }
}

inline float vec3_dot(vec3_t const* a, vec3_t const* b) {
  float d = 0;
  for (uint32_t i = 0; i < 3; ++i) {
    d += a->v[i] * b->v[i];
  }
  return d;
}

inline void vec3_cross(vec3_t* d, vec3_t const* a, vec3_t const* b) {
  d->v[0] = ((a->v[1] * b->v[2]) - (a->v[2] * b->v[1]));
  d->v[1] = ((a->v[2] * b->v[0]) - (a->v[0] * b->v[2]));
  d->v[2] = ((a->v[0] * b->v[1]) - (a->v[1] * b->v[0]));
}

inline void vec3_norm(vec3_t* d, vec3_t* s) {
  float dd = 1.f / sqrtf(s->v[0] * s->v[0] + s->v[1] * s->v[1] + s->v[2] * s->v[2]);
  d->v[0] = s->v[0] * dd;
  d->v[1] = s->v[1] * dd;
  d->v[2] = s->v[2] * dd;
}

inline uint8_t toGammaAccurate(float color_channel) {
  float lo = color_channel * 12.92f;
  float hi = powf(fabsf(color_channel), (1.0f / 2.4f)) * 1.055f - 0.055f;
  float rgb = color_channel < 0.0031308f ? hi : lo;
  return (uint8_t)CLAMP(rgb * 255, 0.f, 255.f);
}

struct plane_t {
  vec3_t n;
  float  d;
};

inline void plane_build(plane_t* dst, vec3_t const* a, vec3_t const* b, vec3_t const* c) {
  vec3_t t1, t2, t3;
  vec3_sub(&t1, b, a);
  vec3_sub(&t2, c, a);
  vec3_cross(&t3, &t1, &t2);
  vec3_norm(&dst->n, &t3);
  dst->d = vec3_dot(&dst->n, a);
}
