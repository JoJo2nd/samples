
#include "common_util.h"
#include <string>
#include "../../external/bgfx/3rdparty/ib-compress/indexbufferdecompression.h"
#include "bx/error.h"
#include "bx/readerwriter.h"

namespace bgfx
{
	int32_t read(bx::ReaderI* _reader, bgfx::VertexDecl& _decl, bx::Error* _err = NULL);
}

namespace util {

void mesh_load(Mesh* m, bx::ReaderSeekerI* _reader)
{
#define BGFX_CHUNK_MAGIC_VB  BX_MAKEFOURCC('V', 'B', ' ', 0x1)
#define BGFX_CHUNK_MAGIC_IB  BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_IBC BX_MAKEFOURCC('I', 'B', 'C', 0x0)
#define BGFX_CHUNK_MAGIC_PRI BX_MAKEFOURCC('P', 'R', 'I', 0x0)

    using namespace bx;
    using namespace bgfx;

    m->materials = nullptr;

    Group group;

    uint32_t chunk;
    bx::Error err;
    while (4 == bx::read(_reader, chunk, &err)
    &&     err.isOk() )
    {
        switch (chunk)
        {
        case BGFX_CHUNK_MAGIC_VB:
            {
                read(_reader, group.m_sphere);
                read(_reader, group.m_aabb);
                read(_reader, group.m_obb);

                read(_reader, m->m_decl);

                uint16_t stride = m->m_decl.getStride();

                uint16_t numVertices;
                read(_reader, numVertices);
                const bgfx::Memory* mem = bgfx::alloc(numVertices*stride);
                read(_reader, mem->data, mem->size);

                group.m_vbh = bgfx::createVertexBuffer(mem, m->m_decl);
            }
            break;

        case BGFX_CHUNK_MAGIC_IB:
            {
                uint32_t numIndices;
                read(_reader, numIndices);
                const bgfx::Memory* mem = bgfx::alloc(numIndices*2);
                read(_reader, mem->data, mem->size);
                group.m_ibh = bgfx::createIndexBuffer(mem);
            }
            break;

        case BGFX_CHUNK_MAGIC_IBC:
            {
                uint32_t numIndices;
                bx::read(_reader, numIndices);

                const bgfx::Memory* mem = bgfx::alloc(numIndices*2);

                uint32_t compressedSize;
                bx::read(_reader, compressedSize);

                void* compressedIndices = malloc(compressedSize);

                bx::read(_reader, compressedIndices, compressedSize);

                ReadBitstream rbs( (const uint8_t*)compressedIndices, compressedSize);
                DecompressIndexBuffer( (uint16_t*)mem->data, numIndices / 3, rbs);

                free(compressedIndices);

                group.m_ibh = bgfx::createIndexBuffer(mem);
            }
            break;

        case BGFX_CHUNK_MAGIC_PRI:
            {
                uint16_t len;
                read(_reader, len);

                std::string material;
                material.resize(len);
                read(_reader, const_cast<char*>(material.c_str() ), len);

                uint16_t num;
                read(_reader, num);

                for (uint32_t ii = 0; ii < num; ++ii)
                {
                    read(_reader, len);

                    std::string name;
                    name.resize(len);
                    read(_reader, const_cast<char*>(name.c_str() ), len);

                    Primitive prim;
                    read(_reader, prim.m_startIndex);
                    read(_reader, prim.m_numIndices);
                    read(_reader, prim.m_startVertex);
                    read(_reader, prim.m_numVertices);
                    read(_reader, prim.m_sphere);
                    read(_reader, prim.m_aabb);
                    read(_reader, prim.m_obb);

                    bx::memSet(prim.m_material, 0, 64);
                    bx::memCopy(prim.m_material, name.c_str(), len < 63 ? len : 63);

                    group.m_prims.push_back(prim);
                }

                m->m_groups.push_back(group);
                group.reset();
            }
            break;

        default:
            //DBG("%08x at %d", chunk, bx::skip(_reader, 0) );
            break;
        }
    }

#undef BGFX_CHUNK_MAGIC_VB 
#undef BGFX_CHUNK_MAGIC_IB 
#undef BGFX_CHUNK_MAGIC_IBC
#undef BGFX_CHUNK_MAGIC_PRI
}

void mesh_bind_materials(Mesh* m, MeshMaterials* mats) {
    for (GroupArray::iterator it = m->m_groups.begin(), itEnd = m->m_groups.end(); it != itEnd; ++it) {
        for (PrimitiveArray::iterator ii = it->m_prims.begin(), iiEnd = it->m_prims.end(); ii != iiEnd; ++ii) {
            ii->m_materialIndex = -1;
            for (uint32_t j = 0, jn = mats->materialsCount; j < jn; ++j) {
                if (stricmp(mats->materials[j].name, ii->m_material) == 0) {
                    ii->m_materialIndex = j;
                    break;
                }
            }
        }
    }

    m->materials = mats;
}

void mesh_unload(Mesh* m)
{
    for (GroupArray::const_iterator it = m->m_groups.begin(), itEnd = m->m_groups.end(); it != itEnd; ++it)
    {
        const Group& group = *it;
        bgfx::destroy(group.m_vbh);

        if (bgfx::isValid(group.m_ibh) )
        {
            bgfx::destroy(group.m_ibh);
        }
    }
    m->m_groups.clear();
}

void mesh_submit(Mesh* m, uint8_t _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state, bgfx::UniformHandle* textures, bgfx::DynamicVertexBufferHandle* buffers)
{
    if (BGFX_STATE_MASK == _state)
    {
        _state = 0
            | BGFX_STATE_RGB_WRITE
            | BGFX_STATE_ALPHA_WRITE
            | BGFX_STATE_DEPTH_WRITE
            | BGFX_STATE_DEPTH_TEST_LESS
            | BGFX_STATE_CULL_CCW
            | BGFX_STATE_MSAA
            ;
    }


    for (GroupArray::const_iterator it = m->m_groups.begin(), itEnd = m->m_groups.end(); it != itEnd; ++it)
    {
        const Group& group = *it;

		bgfx::setTransform(_mtx);
		bgfx::setState(_state);

        bgfx::setBuffer(10, buffers[0], bgfx::Access::Read);
        bgfx::setBuffer(11, buffers[1], bgfx::Access::Read);
        bgfx::setBuffer(12, buffers[2], bgfx::Access::Read);
        bgfx::setBuffer(13, buffers[3], bgfx::Access::Read);
        bgfx::setBuffer(14, buffers[4], bgfx::Access::Read);

        if (group.m_prims[0].m_materialIndex >= 0) {
            MaterialData* mdat = m->materials->materials+group.m_prims[0].m_materialIndex;
            for (uint32_t tt = 0; tt < MATERIAL_MAX; ++tt) {
                if (mdat->flags & (1 << tt)) {
                    bgfx::setTexture(tt, textures[tt], mdat->all[tt]);
                }
            }
			bgfx::setIndexBuffer(group.m_ibh);
			bgfx::setVertexBuffer(0, group.m_vbh);
			bgfx::submit(_id, _program, 0, false);
		}
    }
}

/*
void mesh_submit(Mesh* m, const MeshState*const* _state, uint8_t _numPasses, const float* _mtx, uint16_t _numMatrices) const
{
    uint32_t cached = bgfx::setTransform(_mtx, _numMatrices);

    for (uint32_t pass = 0; pass < _numPasses; ++pass)
    {
        bgfx::setTransform(cached, _numMatrices);

        const MeshState& state = *_state[pass];
        bgfx::setState(state.m_state);

        for (uint8_t tex = 0; tex < state.m_numTextures; ++tex)
        {
            const MeshState::Texture& texture = state.m_textures[tex];
            bgfx::setTexture(texture.m_stage
                    , texture.m_sampler
                    , texture.m_texture
                    , texture.m_flags
                    );
        }

        for (GroupArray::const_iterator it = m_groups.begin(), itEnd = m_groups.end(); it != itEnd; ++it)
        {
            const Group& group = *it;

            bgfx::setIndexBuffer(group.m_ibh);
            bgfx::setVertexBuffer(0, group.m_vbh);
            bgfx::submit(state.m_viewId, state.m_program, 0, it != itEnd-1);
        }
    }
}
*/

}
