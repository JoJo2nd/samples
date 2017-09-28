//layout(local_size_x = 128) in;
//layout(std430) buffer;
//layout(binding = 0) writeonly buffer Output {
//vec4 elements[];
//} output_data;
//layout(binding = 1) readonly buffer Input0 {
//vec4 elements[];
//} input_data0;
//layout(binding = 2) readonly buffer Input1 {
//vec4 elements[];
//} input_data1;

#include "bgfx_compute.sh"
#include "common/common.sh"

#define GROUP_SIZE 16

uniform vec2 resSize;

BUFFER_WR(output_data, vec4, 0);
BUFFER_RO(input_data0, vec4, 1);
BUFFER_RO(input_data1, vec4, 2);

#define light_type(idx)     (lights[(idx*2)].w)
#define light_position(idx) (lights[(idx*2)].xyz)
#define light_params(idx)   (lights[(idx*2)+1].xyzw)
BUFFER_RO(lights, vec4, 3);

// depth is d24s8 - is r32f correct or should it be d24s8
IMAGE2D_RO(depthImage, r32f, 0);
SHARED float depthVals[GROUP_SIZE*GROUP_SIZE];

NUM_THREADS(GROUP_SIZE, GROUP_SIZE, 1)
void main() {
    float depthMin;
    float depthMax;

    // Grab or calculate the frustum planes for this tile. Should be in view space but will need to be transformed

    // Read depths to shared memory
    if (gl_GlobalInvocationID.x < resSize.x && gl_GlobalInvocationID.y < resSize.y) {
        depthVals[(gl_LocalInvocationID.y*GROUP_SIZE)+gl_LocalInvocationID.x] = imageLoad(depthImage, gl_GlobalInvocationID.xy*vec2(GROUP_SIZE, GROUP_SIZE)).r;
    }
    // sync(1)
    // Calculate near & far depths - atomicMin/interlockedMin & atomicMax/interlockedMax are options here?
    groupMemoryBarrier();
    for (int i=0; i<(GROUP_SIZE); ++i) {
        for (int j=0; j<GROUP_SIZE; ++j) {
            //TODO: bound check
            depthMin = min(depthMin, depthVals[i]);
            depthMax = max(depthMin, depthVals[i]);
        }
    }

    // no need to sync here...

    // Once per work group:
    //  for each light
    //   test against frustum places
    //    if (within frustum) append to tile light list

    // append sun light.

    // done
    uint ident = gl_GlobalInvocationID.x;
    output_data[ident] = input_data0[ident] * input_data1[ident];
}
