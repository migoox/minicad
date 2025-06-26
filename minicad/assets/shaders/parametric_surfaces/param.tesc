#version 430 core

layout (vertices=4) out;

in VS_OUT {
    mat4 worldMat;
    vec2 radii;
    ivec2 tess_level;
    int state;
    int tex_id;
} tcs_in[];

out TCS_OUT {
    mat4 worldMat;
    vec2 radii;
    int state;
} tcs_out[];

patch out int id;

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcs_out[gl_InvocationID].worldMat = tcs_in[gl_InvocationID].worldMat;
    tcs_out[gl_InvocationID].radii = tcs_in[gl_InvocationID].radii;
    tcs_out[gl_InvocationID].state = tcs_in[gl_InvocationID].state;

    int tessLevelX = tcs_in[gl_InvocationID].tess_level.x;
    int tessLevelY = tcs_in[gl_InvocationID].tess_level.y;
    // invocation zero controls tessellation levels for the entire patch
    if (gl_InvocationID == 0)
    {
        id = tcs_in[gl_InvocationID].tex_id;
        gl_TessLevelOuter[0] = tessLevelY;
        gl_TessLevelOuter[1] = tessLevelX;
        gl_TessLevelOuter[2] = tessLevelY;
        gl_TessLevelOuter[3] = tessLevelX;

        gl_TessLevelInner[0] = tessLevelX;
        gl_TessLevelInner[1] = tessLevelY;
    }
}
	