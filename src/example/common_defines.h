
#ifndef COMMON_DEFINES_H__
#define COMMON_DEFINES_H__

#define RENDER_PASS_COMPUTE (0)
#define RENDER_PASS_ZPREPASS (1)
#define RENDER_PASS_SOLID (2)

#define RENDER_PASS_LUMINANCE_START (10)

#define RENDER_PASS_TONEMAP (32)

#define RENDER_PASS_DEBUG (50)
#define RENDER_POST_DEBUG_BLIT (51)
#define RENDER_PASS_2DDEBUG (52)


#define INSTANCE_GIRD_SIZE (10)
#define TOTAL_GRID_COUNT (INSTANCE_GIRD_SIZE * INSTANCE_GIRD_SIZE)
#define LIGHT_COUNT (2000)
#define GRID_SIZE (32)
#define GRID_SIZE_M_ONE (GRID_SIZE - 1)
#define LIGHT_GRID_SPACE (4096) // (1280/16)x(720/16) = 80x45 = 3600
#define Z_BUCKETS (4096)        // 4096 for larger ranges?
#define Z_INVALID_BUCKET (0xFFFF)
#define MAX_LIGHT_COUNT (512)
#define TILE_SIZE (32)
#define DISPATCH_WAVE (64)
#define SMALL_DISPATCH_WAVE (32)
#define LIGHT_STORAGE_COUNT (TILE_SIZE * TILE_SIZE)
#define LIGHT_FALLOFF (2)
#define SOLID_PROGS (7)
#define MAX_LIGHT_GRID_COUNT (MAX_LIGHT_COUNT)

#define TOTAL_ORBS (60)

#define COLOUR_LUT_DIM (16)

#define MESH_ASSET_PATH ("data/lostempire.mesh")
#define SPONZA_MESH_ASSET_PATH ("data/sponza_pbr.mesh")
#define SPONZA_MATERIAL_ASSET_PATH ("data/sponza.mat")
#define ORB_MESH_ASSET_PATH ("data/orb.mesh")
#define VS_ASSET_PATH ("data/simple.vs")
#define VS_DEBUG_ASSET_PATH ("data/postex.vs")
#define FS_ASSET_PATH ("data/simple.ps")
#define FS_NO_TEX_ASSET_PATH ("data/simple_nt.ps")
#define FS_MASK_ASSET_PATH ("data/simple_am.ps")
#define CS_LIGHT_CULL_PATH_NO_OPT ("data/light_grid.cs")
#define CS_LIGHT_CULL_PATH ("data/light_grid_o.cs")
#define FS_DEBUG01_ASSET_PATH ("data/debug01.ps")
#define FS_DEBUG02_ASSET_PATH ("data/debug02.ps")
#define FS_FSDEBUG_WCS_ASSET_PATH ("data/fs_debug_wcs.ps")
#define FS_COPY_ASSET_PATH ("data/copy.ps")

#define TONEMAP_ASSET_PATH ("data/tonemap.ps")
#define FS_LUMINANCE_AVG_ASSET_PATH ("data/log_luminance_avg.ps")
#define LUMINANCE_AVG_ASSET_PATH ("data/log_luminance_avg.cs")

#define VS_Z_FILL ("data/zfill.vs")
#define PS_Z_FILL ("data/zfill.ps")

#define PI (3.1415926535897932384626433832795f)
#define ONE_OVER_PI (1.f / PI)
#define deg_to_rad(x) (x * (PI / 180.f))

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)
#define CLAMP(v, a, b) (v < a ? a : v > b ? b : v)

#define SHADER_NONMETAL_SPEC_COLOUR (vec3(0.4, 0.4, 0.4))

#define GLOBAL_SCALE (0.1f)

#define SHADER_FLT_EPSILON (0.00001)

#ifndef DEBUG_LIGHT_EVAL
#define DEBUG_LIGHT_EVAL (0)
#endif
#ifndef DEBUG_NO_BINS
#define DEBUG_NO_BINS (0)
#endif
#ifndef CS_LIGHT_CULL
#define CS_LIGHT_CULL (0)
#endif
#ifndef ALPHA_MASK
#define ALPHA_MASK (0)
#endif
#ifndef NO_TEXTURES
#define NO_TEXTURES (0)
#endif

#endif // COMMON_DEFINES_H__
