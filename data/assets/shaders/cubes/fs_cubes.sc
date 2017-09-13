$input v_color0

/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "../common/common.sh"

SAMPLER3D(s_sdf, 0);

#define MAX_STEPS (32)
#define DIMS (vec3(1.37769, 2.0551, 0.998526)-vec3(-1.1793, -0.488239, -1.20821))
#define DIMS_LEN (length(DIMS))
#define RCP_DIMS_LEN (1.f/DIMS_LEN)

void main()
{

    // v_color is our starting position in the cube (where the camera ray hit the cube)
    // Ray trace until we hit a solid voxel (sdf <= 0), we leave the cube (x, y & z > 1.f) or max steps
    vec3 obj_pos = v_color0.xyz;
    float next_step = 0.f;
    float factor = 0.05f;//DIMS_LEN;
    float steps = 0;
    //vec3 viewDir = normalize(obj_pos);
    vec3 viewDir = normalize(u_invView[2].xyz);
    viewDir = mul(transpose(u_model[0]), viewDir);
    for (uint i = 0; i < MAX_STEPS; ++i) {
        obj_pos += (viewDir*next_step*factor);
        float sdf = texture3D(s_sdf, obj_pos).r;
        next_step = max(sdf, 0.1f);
        steps += 1.f/(MAX_STEPS*4);
        if (sdf <= 0.0f) { 
            gl_FragColor=vec4(steps.x, 0, 0, 1); 
            return;
            //break;
        }
        if (obj_pos.x > 1.f || obj_pos.y > 1.f || obj_pos.z > 1.f ||
            obj_pos.x < 0.f || obj_pos.y < 0.f || obj_pos.z < 0.f) {
            clip(-1);
            //break;
        }
    }
	//gl_FragColor = vec4(v_color0.xyz, 1);
    //gl_FragColor = vec4(steps.xxx, 1);
    clip(-1);
}
