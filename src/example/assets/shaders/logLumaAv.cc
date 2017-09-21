
#include "../../../common/common.sh"
#include "../../common_defines.h"

uniform vec4 u_params[2];

#define u_inputSize (u_params[0])
#define u_geometricMeanPower (u_params[0].w) // -> 1/n where 'n' is total pixels in the image.
#define u_numWorkGroups (u_params[1]) // workGroups per dim + Total number of thread groups sent to the dispatch call

groupshared float gridValues[SMALL_DISPATCH_WAVE*SMALL_DISPATCH_WAVE];

BUFFER_WR(workingLuma,             float, 1);
BUFFER_WR(averageLuma,             float, 2);

NUM_THREADS(SMALL_DISPATCH_WAVE, SMALL_DISPATCH_WAVE, 1)
void main() {
  float logLumAvg = 0.f;
  float logLumAvg2 = 0.f;

  //for number of 32x32 blocks in texture. 
  {
    //vec2 uv = gl_GlobalInvocationID.xy / u_inputSize.xy;
    vec3 colour = s_base.m_texture.Load(vec3(gl_GlobalInvocationID.xy, 0)).rgb;
    float pixel_luma = luma(colour).r;
    gridValues[gl_LocalInvocationIndex] = log(pixel_luma + SHADER_FLT_EPSILON);
    barrier();

    // This is dumb and will probably not offer much performance
    if (gl_LocalInvocationIndex == 0) {
      for (uint i =0; i < SMALL_DISPATCH_WAVE*SMALL_DISPATCH_WAVE; ++i) {
        logLumAvg += gridValues[i];
      }
    }
    barrier();
  }

  // this is dumb but should do the job
  uint blockIndex = 
      gl_WorkGroupID.z * u_numWorkGroups.x * u_numWorkGroups.y +
      gl_WorkGroupID.y * u_numWorkGroups.x + 
      gl_WorkGroupID.x;

  if (gl_LocalInvocationIndex == 0) {
    workingLuma[blockIndex] = logLumAvg;
  }

  barrier();

  if (blockIndex == 0 && gl_LocalInvocationIndex == 0) {
    logLumAvg2 = 0.f;
    for (uint i = 0, n = u_numWorkGroups.w; i < n; ++i) {
      logLumAvg2 += workingLuma[i];
    }

    logLumAvg2 *= u_geometricMeanPower;
    float lumAvg = exp2(logLumAvg2);
    averageLuma[0] = lumAvg;
  }
}
