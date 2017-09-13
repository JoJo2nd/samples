/*

    Light grid selection;

    Each grid has X number of lights it can store. Once the upper limit is hit, it stops writing.

    The light is is sorted by view depth so dropping far off lights is OK-ish in the real world I guess.
    Each ComputeUnit can load up the local storage with lights (Wavelength at a time at a time?) and then all can test
    and write out the grid list.

*/

#ifndef CS_MULTI
#   define CS_MULTI (0) // define to load/test multiple lights in parallel
#endif

#include "bgfx_compute.sh"
#include "uniforms.sh"

#include "../../common_defines.h"
//#define shared groupshared

struct LightData {
    vec4 pos_r;
    vec4 colour;
};

uniform vec4 u_params[2];

#define u_nearPlane (u_params[0].x)
#define u_farPlane (u_params[0].y)
#define u_fovy (u_params[0].z)
#define u_aspect (u_params[0].w)
#define u_nLights ((uint)(u_params[1].x))
#define u_width ((uint)(u_params[1].y))
#define u_height ((uint)(u_params[1].z))
//uniform vec4 u_viewParams; // x: near plane, y: far Plane, z: fovy
//uniform uint4 u_lightData; // x: number of Lights, zw: width, height
//uniform LightData viewSpaceLights[MAX_LIGHT_COUNT];

BUFFER_RO(viewSpaceLights,       vec4, 0);
BUFFER_WR(lightGrid,             uint, 1);

//shared LightData currentLights[LIGHT_STORAGE_COUNT];
groupshared vec4 tileFrustum[6];
#if CS_MULTI
    groupshared uint writtenLights;
#endif
uint tileLights[LIGHT_STORAGE_COUNT];

vec4 plane_build(vec3 a, vec3 b, vec3 c) {
    vec4 p;
    vec3 t1, t2;
    t1 = b - a;
    t2 = c - a;
    p.xyz = normalize(cross(t1, t2));
    p.w = dot(p.xyz, a);
    return p;
}

#if CS_MULTI
    NUM_THREADS(DISPATCH_WAVE, 1, 1)
#else
    NUM_THREADS(1, 1, 1)
#endif
void main() {
    // Calculate frustums for each tile.
    if (gl_LocalInvocationIndex == 0) {
        // first CU builds the frustum info
        uint width = u_width;
        uint height = u_height;
        float camNearPlane = u_nearPlane;
        float camFarPlane = u_farPlane;
        float camFovY = u_fovy;
        float camAspect = u_aspect;
        float grid_w = 1.f / (width / TILE_SIZE);
        float grid_h = 1.f / ((uint)(height + (TILE_SIZE-1)) / TILE_SIZE);

        // get the half widths for our near and far planes
        float vn_height = tan(deg_to_rad(camFovY*.5f))*camNearPlane;
        float vn_width = vn_height*camAspect;

        float vf_height = tan(deg_to_rad(camFovY*.5f))*camFarPlane;
        float vf_width = vf_height*camAspect;

        vec3 cam_pos = vec3( 0.f, 0.f, 0.f ); // Lights are in view space so this is always the case
        vec3 vs_right = vec3(1.f, 0.f, 0.f), vs_up = vec3(0.f, 1.f, 0.f), vs_fwd = vec3(0.f, 0.f, 1.f);
        vec3 vnear, vnear2, vfar, vnup, vnright, vnup2, vnright2, vfup, vfright, vfup2, vfright2;

        vfar = vs_fwd*camFarPlane;
        vnear = vs_fwd*camNearPlane;

        vnup = vs_up*(vn_height*grid_h*2.f);
        vnright = vs_right*(vn_width*grid_w*2.f);
        vfup = vs_up*(vf_height*grid_h*2.f);
        vfright = vs_right*(vf_width*grid_w*2.f);

        vec3 vn_bl = vnear - (vs_up*vn_height) - (vs_right*vn_width);
        vec3 vf_bl = vfar - (vs_up*vf_height) - (vs_right*vf_width);

        vec3 ntl, ntr, nbl, nbr;
        vec3 ftl, ftr, fbl, fbr;
        
        vn_bl += (gl_WorkGroupID.y*vnup)+(gl_WorkGroupID.x*vnright);
        vf_bl += (gl_WorkGroupID.y*vfup)+(gl_WorkGroupID.x*vfright);

        ntl = vn_bl + vnup;
        ntr = ntl + vnright;
        nbl = vn_bl;
        nbr = vn_bl + vnright;

        ftl = vf_bl + vfup;
        ftr = ftl + vfright;
        fbl = vf_bl;
        fbr = vf_bl + vfright;

        tileFrustum[0] = plane_build(ntr, ntl, nbl); // near
        tileFrustum[1] = plane_build(ftl, ftr, fbl); // far
        tileFrustum[2] = plane_build(ftl, nbl, ntl); // left
        tileFrustum[3] = plane_build(ftr, ntr, nbr); // right
        tileFrustum[4] = plane_build(ntr, ftr, ftl); // top
        tileFrustum[5] = plane_build(nbr, fbl, fbr); // bottom
    }
    if (gl_LocalInvocationIndex == 0) {
#if CS_MULTI        
        // init the written lights
        //atomicExchange(writtenLights, 0);
        uint __x;
        InterlockedExchange(writtenLights, 0, __x);
#endif
    }
    barrier();
    // Load and process lights
    uint lightCount = u_nLights;
#if CS_MULTI
    uint processedLights = gl_LocalInvocationIndex;
#else
    uint processedLights = 0;
#endif
    uint lightOffset = ((gl_WorkGroupID.y*(u_width/TILE_SIZE)) + (gl_WorkGroupID.x)) * MAX_LIGHT_COUNT;
#if !CS_MULTI
    uint writtenLights = 0;
#endif
    while (processedLights < lightCount) {
        
        // Once loaded the next light group/set clip it

        //for (uint l = 0, n = LIGHT_STORAGE_COUNT; l < n; ++l) {
        bool outside = false;
        for (uint p = 0; p < 6; ++p) {
            vec3 pos = viewSpaceLights[(processedLights*2)].xyz;
            float radius = viewSpaceLights[(processedLights*2)].w;
            float dd = dot(tileFrustum[p].xyz, pos) - tileFrustum[p].w + radius;
            if (dd < 0.f) outside = true;
        }
        if (!outside) {
#if CS_MULTI
            //uint idx = atomicAdd(writtenLights, 1);
            uint idx;
            InterlockedAdd(writtenLights, 1, idx);
            //idx = writtenLights++;
            if(idx < (MAX_LIGHT_COUNT-1)) {
                lightGrid[lightOffset + idx] = processedLights;
            }
#else
            if(writtenLights < (MAX_LIGHT_COUNT-1)) {
                lightGrid[lightOffset + writtenLights++] = processedLights;
            }
#endif
        }
        //}

        //processedLights += LIGHT_STORAGE_COUNT;
        if (CS_MULTI)
            processedLights += DISPATCH_WAVE;
        else
            ++processedLights;
    }
    barrier();
#if CS_MULTI
    if (gl_LocalInvocationIndex == 0) {
        lightGrid[lightOffset + writtenLights] = 0xFFFF;
    }
#else
    lightGrid[lightOffset + writtenLights] = 0xFFFF;
#endif
}
