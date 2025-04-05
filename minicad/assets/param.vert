#version 430 core
layout (location = 0) in vec2 a_PatchPos;
layout (location = 1) in vec2 a_radii;
layout (location = 2) in ivec2 a_tess_level;
layout (location = 3) in mat4 a_worldMat;
layout (location = 7) in int a_id;
layout (location = 8) in int a_state;

#define FLT_MAX 3.402823466e+38

out VS_OUT {
    mat4 worldMat;
    vec2 radii;
    ivec2 tess_level;
    int id;
    int state;
} vs_out;

void main() {
    if (a_state == 2) {
        // turn off the visibility
        gl_Position = vec4(FLT_MAX);
        vs_out.tess_level = ivec2(0, 0);
    }
    else {
        gl_Position = vec4(a_PatchPos, 0.0, 1.0);
        vs_out.worldMat = a_worldMat;
        vs_out.radii = a_radii;
        vs_out.tess_level = a_tess_level;
        vs_out.id = a_id;
        vs_out.state = a_state;
    }
}
