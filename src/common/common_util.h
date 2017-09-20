
#pragma once

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <vector>
#include "bx/readerwriter.h"

namespace util {

struct MeshMaterials;

struct Aabb
{
    float m_min[3];
    float m_max[3];
};

struct Obb
{
    float m_mtx[16];
};

struct Sphere
{
    float m_center[3];
    float m_radius;
};

struct Primitive
{
    char     m_material[64];
    int32_t  m_materialIndex; // -1 when non
    uint32_t m_startIndex;
    uint32_t m_numIndices;
    uint32_t m_startVertex;
    uint32_t m_numVertices;

    Sphere m_sphere;
    Aabb m_aabb;
    Obb m_obb;
};

typedef std::vector<Primitive> PrimitiveArray;

struct Group
{
    Group()
    {
        reset();
    }

    void reset()
    {
        m_vbh.idx = bgfx::kInvalidHandle;
        m_ibh.idx = bgfx::kInvalidHandle;
        m_prims.clear();
    }

    bgfx::VertexBufferHandle m_vbh;
    bgfx::IndexBufferHandle m_ibh;
    Sphere m_sphere;
    Aabb m_aabb;
    Obb m_obb;
    PrimitiveArray m_prims;
};

typedef std::vector<Group> GroupArray;

struct Mesh
{
    bgfx::VertexDecl m_decl;
    GroupArray m_groups;
    MeshMaterials* materials;
};

void mesh_load(Mesh* m, bx::ReaderSeekerI* _reader);
void mesh_bind_materials(Mesh* m, MeshMaterials* mats);
void mesh_unload(Mesh* m);
void mesh_submit(Mesh* m, uint8_t _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state, bgfx::UniformHandle* textures, bgfx::DynamicVertexBufferHandle* buffers);

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
    uint32_t flags;
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

}
