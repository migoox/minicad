#version 430 core

layout (vertices=4) out;

in VS_OUT {
    mat4 worldMat;
    vec2 radii;
    ivec2 tess_level;
} tcs_in[];

out TCS_OUT {
    mat4 worldMat;
    vec2 radii;
} tcs_out[];

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcs_out[gl_InvocationID].worldMat = tcs_in[gl_InvocationID].worldMat;
    tcs_out[gl_InvocationID].radii = tcs_in[gl_InvocationID].radii;

    int tessLevelX = tcs_in[gl_InvocationID].tess_level.x;
    int tessLevelY = tcs_in[gl_InvocationID].tess_level.y;
    // invocation zero controls tessellation levels for the entire patch
    if (gl_InvocationID == 0)
    {
        gl_TessLevelOuter[0] = tessLevelY;
        gl_TessLevelOuter[1] = tessLevelX;
        gl_TessLevelOuter[2] = tessLevelY;
        gl_TessLevelOuter[3] = tessLevelX;

        gl_TessLevelInner[0] = tessLevelX;
        gl_TessLevelInner[1] = tessLevelY;
    }
}
	