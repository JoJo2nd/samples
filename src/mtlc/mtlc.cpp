
#include "common_util.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

using namespace util;

#pragma warning (disable:4996)

enum ParseState {
	ps_base = MATERIAL_BASE,
	ps_metallic = MATERIAL_METALLIC,
	ps_normal = MATERIAL_NORMAL,
	ps_roughness = MATERIAL_ROUGHNESS,
	ps_mask = MATERIAL_MASK,
	ps_last_texture,

	ps_next,
	ps_newmtl, 

};

#define ps_first_texture (ps_base)

int32_t getNextToken(FILE* file, char* out_token, uint32_t max_token_len) {
	int c;
	uint32_t w = 0;
	do {
		c = fgetc(file);
	} while (isspace(c));

	if (feof(file) || !(max_token_len > 1))
		return -1;

	uint32_t limit = max_token_len - 1;
	do {
		out_token[w++] = c;
		c = fgetc(file);
	} while (!isspace(c) && w < limit);

	out_token[w] = 0;

	return feof(file) ? -1 : w;
}

int32_t skipLine(FILE* file) {
	int c;
	uint32_t w = 0;
	do {
		c = fgetc(file);
	} while (c != '\n' && !feof(file));

	return feof(file) ? -1 : 1;
}

/*
	map_Ka textures_pbr\Dielectric_metallic.tga
	map_Kd textures_pbr\VasePlant_diffuse.tga
	map_bump textures_pbr\VasePlant_normal.tga
	bump textures_pbr\VasePlant_normal.tga
	map_Ns textures_pbr\VasePlant_roughness.tga
	map_d textures_pbr\vase_plant_mask.tga
*/

struct Material {
	uint32_t flags;
	char name[1024];
	char tex_name[ps_last_texture-ps_first_texture][1024];
};

uint32_t mat_count = 0;
Material materials[1024];
MaterialIO ioMaterials[1024];

int main(int argc, char const* argv[]) {
	char token_buffer[1024];
	Material cur_mat;

	if (argc != 2) {
		fprintf(stdout, "mtlc - usage: mtlc input.mtl\n");
	}

	FILE* mtlf = fopen(argv[1], "rb");
	ParseState state = ps_next;

	while (getNextToken(mtlf, token_buffer, sizeof token_buffer) >= 0) {
		if (token_buffer[0] == '#') {
			skipLine(mtlf);
			continue;
		}

		if (state == ps_next) {
			if (stricmp(token_buffer, "newmtl") == 0) {
				if (mat_count > 0) {
					materials[mat_count-1] = cur_mat;
					memset(&cur_mat, 0, sizeof cur_mat);
				}
				state = ps_newmtl;
				++mat_count;
			} else if (stricmp(token_buffer, "map_Ka") == 0) {
				state = ps_metallic;
			} else if (stricmp(token_buffer, "map_Kd") == 0) {
				state = ps_base;
			} else if (stricmp(token_buffer, "map_bump") == 0 || strcmp(token_buffer, "bump") == 0) {
				state = ps_normal;
			} else if (stricmp(token_buffer, "map_Ns") == 0) {
				state = ps_roughness;
			} else if (stricmp(token_buffer, "map_d") == 0) {
				state = ps_mask;
			}
		} else if (state == ps_newmtl) {
			strcpy(cur_mat.name, token_buffer);
			state = ps_next;
		} else if (state >= ps_first_texture && state < ps_last_texture) {
			strcpy(cur_mat.tex_name[state-ps_first_texture], token_buffer);
			cur_mat.flags |= 1 << (state-ps_first_texture);
			state = ps_next;
		}
	}

	materials[mat_count - 1] = cur_mat;
	memset(&cur_mat, 0, sizeof cur_mat);

	fclose(mtlf);

	FILE* matf = fopen("a.mat", "wb");
	uint32_t cur_str_offset = 0;
	MaterialHeader header;
	header.magic = MATERIAL_HEADER_MAGIC;
	header.nMaterials = mat_count - 1;
	header.strTableOffset = sizeof(MaterialIO)*header.nMaterials;

	cur_str_offset = header.strTableOffset;

	for (uint32_t i = 0, n = header.nMaterials; i < n; ++i) {
		memset(ioMaterials + i, 0, sizeof(MaterialIO));
		ioMaterials[i].flags = materials[i].flags;
		ioMaterials[i].nameOffset = cur_str_offset;
		cur_str_offset += (uint32_t)strlen(materials[i].name) + 1;
		if (materials[i].flags & (1 << ps_base)) {
			ioMaterials[i].baseOffset = cur_str_offset;
			cur_str_offset += (uint32_t)strlen(materials[i].tex_name[ps_base]) + 1;
		}
		if (materials[i].flags & (1 << ps_normal)) {
			ioMaterials[i].normalOffset = cur_str_offset;
			cur_str_offset += (uint32_t)strlen(materials[i].tex_name[ps_normal]) + 1;
		}
		if (materials[i].flags & (1 << ps_metallic)) {
			ioMaterials[i].metallicOffset = cur_str_offset;
			cur_str_offset += (uint32_t)strlen(materials[i].tex_name[ps_metallic]) + 1;
		}
		if (materials[i].flags & (1 << ps_roughness)) {
			ioMaterials[i].roughnesOffset = cur_str_offset;
			cur_str_offset += (uint32_t)strlen(materials[i].tex_name[ps_roughness]) + 1;
		}
		if (materials[i].flags & (1 << ps_mask)) {
			ioMaterials[i].maskOffset = cur_str_offset;
			cur_str_offset += (uint32_t)strlen(materials[i].tex_name[ps_mask]) + 1;
		}
	}

	fwrite(&header, sizeof MaterialHeader, 1, matf);
	fwrite(ioMaterials, sizeof MaterialIO, header.nMaterials, matf);

	for (uint32_t i = 0, n = header.nMaterials; i < n; ++i) {
		fwrite(materials[i].name, 1, strlen(materials[i].name) + 1, matf);
		if (materials[i].flags & (1 << ps_base)) {
			fwrite(materials[i].tex_name[ps_base], 1, strlen(materials[i].tex_name[ps_base]) + 1, matf);
		}
		if (materials[i].flags & (1 << ps_normal)) {
			fwrite(materials[i].tex_name[ps_normal], 1, strlen(materials[i].tex_name[ps_normal]) + 1, matf);
		}
		if (materials[i].flags & (1 << ps_metallic)) {
			fwrite(materials[i].tex_name[ps_metallic], 1, strlen(materials[i].tex_name[ps_metallic]) + 1, matf);
		}
		if (materials[i].flags & (1 << ps_roughness)) {
			fwrite(materials[i].tex_name[ps_roughness], 1, strlen(materials[i].tex_name[ps_roughness]) + 1, matf);
		}
		if (materials[i].flags & (1 << ps_mask)) {
			fwrite(materials[i].tex_name[ps_mask], 1, strlen(materials[i].tex_name[ps_mask]) + 1, matf);
		}
	}

	fclose(matf);

	matf = fopen("a.bat", "wb");
	char tmp_buffer[1024];
	size_t slen = 0;
	for (uint32_t i = 0, n = header.nMaterials; i < n; ++i) {
		fprintf(matf, "REM source material - %s\n", materials[i].name);
		if (materials[i].flags & (1 << ps_base)) {
			slen = strlen(materials[i].tex_name[ps_base]);
			strcpy(tmp_buffer, materials[i].tex_name[ps_base]);
			tmp_buffer[slen - 3] = 'k';
			tmp_buffer[slen - 2] = 't';
			tmp_buffer[slen - 1] = 'x';
			fprintf(matf, "-f %s -o %s -t BC3 -m --as ktx\n", materials[i].tex_name[ps_base], tmp_buffer);
		}
		if (materials[i].flags & (1 << ps_normal)) {
			slen = strlen(materials[i].tex_name[ps_normal]);
			strcpy(tmp_buffer, materials[i].tex_name[ps_normal]);
			tmp_buffer[slen - 3] = 'k';
			tmp_buffer[slen - 2] = 't';
			tmp_buffer[slen - 1] = 'x';
			fprintf(matf, "-f %s -o %s -t BC1 -m -n --as ktx\n", materials[i].tex_name[ps_normal], tmp_buffer);
		}
		if (materials[i].flags & (1 << ps_metallic)) {
			slen = strlen(materials[i].tex_name[ps_metallic]);
			strcpy(tmp_buffer, materials[i].tex_name[ps_metallic]);
			tmp_buffer[slen - 3] = 'k';
			tmp_buffer[slen - 2] = 't';
			tmp_buffer[slen - 1] = 'x';
			fprintf(matf, "-f %s -o %s -t BC3 -m --as ktx\n", materials[i].tex_name[ps_metallic], tmp_buffer);
		}
		if (materials[i].flags & (1 << ps_roughness)) {
			slen = strlen(materials[i].tex_name[ps_roughness]);
			strcpy(tmp_buffer, materials[i].tex_name[ps_roughness]);
			tmp_buffer[slen - 3] = 'k';
			tmp_buffer[slen - 2] = 't';
			tmp_buffer[slen - 1] = 'x';
			fprintf(matf, "-f %s -o %s -t BC3 -m --as ktx\n", materials[i].tex_name[ps_roughness], tmp_buffer);
		}
		if (materials[i].flags & (1 << ps_mask)) {
			slen = strlen(materials[i].tex_name[ps_mask]);
			strcpy(tmp_buffer, materials[i].tex_name[ps_mask]);
			tmp_buffer[slen - 3] = 'k';
			tmp_buffer[slen - 2] = 't';
			tmp_buffer[slen - 1] = 'x';
			fprintf(matf, "-f %s -o %s -t BC3 -m --as ktx\n", materials[i].tex_name[ps_mask], tmp_buffer);
		}
	}
	fclose(matf);

    return 0;
}
