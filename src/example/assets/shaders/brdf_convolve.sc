$input v_texcoord0


#include "../../../common/common.sh"
#include "../../common_defines.h"
#include "convolve_common.sh"

// ----------------------------------------------------------------------------
void main() 
{
    vec2 integratedBRDF = IntegrateBRDF(v_texcoord0.x, v_texcoord0.y);
    gl_FragColor = vec4(integratedBRDF.rg, 0, 0);
}
