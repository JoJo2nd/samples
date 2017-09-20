
#include "common_util.h"
#include "bx/readerwriter.h"
#include "bx/error.h"
#include "bgfx_utils.h"

namespace util {

void material_load(MeshMaterials* m, bx::FileReaderI* reader) {
	int64_t fsize = getSize(reader);
    MaterialHeader header;
    read(reader, header);

	m->materialsCount = header.nMaterials;
	m->materials = new MaterialData[header.nMaterials];
	m->stringTable = new char[fsize - header.strTableOffset];

	bx::Error err;
	reader->seek(header.strTableOffset);
	reader->read(m->stringTable, (int32_t)(fsize - header.strTableOffset), &err);
	reader->seek(sizeof(MaterialHeader), bx::Whence::Begin);

	MaterialIO* io_mat = (MaterialIO*)malloc(sizeof(MaterialIO)*header.nMaterials);
	for (uint32_t i = 0, n = header.nMaterials; i < n; ++i) {
		read(reader, io_mat[i]);
	}
	bx::close(reader);

#define FIXUP_TEXTURE_PATH(v) \
			/* +13 for common 'textures_pbr' prefix */ \
			char* base = m->stringTable + (v - header.strTableOffset) + 13; \
			strcpy(tmp, base); \
			/* change ext to ktx */ \
			size_t l = strlen(buf); \
			buf[l-3] = 'k'; \
			buf[l-2] = 't'; \
			buf[l-1] = 'x';

	char buf[1024] = "data/";
	char* tmp = buf + 5;
	for (uint32_t i = 0, n = header.nMaterials; i < n; ++i) {
		m->materials[i].name = m->stringTable + (io_mat[i].nameOffset - header.strTableOffset);
		m->materials[i].flags = io_mat[i].flags;
		if (io_mat[i].flags & MATERIAL_BASE_FLAG) {
			FIXUP_TEXTURE_PATH(io_mat[i].baseOffset);
			m->materials[i].base = loadTexture(buf);
		}
		if (io_mat[i].flags & MATERIAL_METALLIC_FLAG) {
			FIXUP_TEXTURE_PATH(io_mat[i].metallicOffset);
			m->materials[i].metallic = loadTexture(buf);
		}
		if (io_mat[i].flags & MATERIAL_NORMAL_FLAG) {
			FIXUP_TEXTURE_PATH(io_mat[i].normalOffset);
			m->materials[i].normal = loadTexture(buf);
		}
		if (io_mat[i].flags & MATERIAL_ROUGHNESS_FLAG) {
			FIXUP_TEXTURE_PATH(io_mat[i].roughnesOffset);
			m->materials[i].roughnes = loadTexture(buf);
		}
		if (io_mat[i].flags & MATERIAL_MASK_FLAG) {
			FIXUP_TEXTURE_PATH(io_mat[i].maskOffset);
			m->materials[i].mask = loadTexture(buf);
		}
	}

#undef FIXUP_TEXTURE_PATH
}

void material_unload(MeshMaterials* m) {
    for (uint32_t i = 0, n = m->materialsCount; i < n; ++i) {
		if (m->materials[i].flags & MATERIAL_BASE_FLAG) {
			bgfx::destroy(m->materials[i].base);
		}
		if (m->materials[i].flags & MATERIAL_METALLIC_FLAG) {
			bgfx::destroy(m->materials[i].metallic);
		}
		if (m->materials[i].flags & MATERIAL_NORMAL_FLAG) {
			bgfx::destroy(m->materials[i].normal);
		}
		if (m->materials[i].flags & MATERIAL_ROUGHNESS_FLAG) {
			bgfx::destroy(m->materials[i].roughnes);
		}
		if (m->materials[i].flags & MATERIAL_MASK_FLAG) {
			bgfx::destroy(m->materials[i].mask);
		}
    }

    delete m->stringTable;
    delete m->materials;
}

}
