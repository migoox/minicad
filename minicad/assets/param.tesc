#version 430 core

layout (vertices=4) out;

uniform int tessLevelX;
uniform int tessLevelY;

in VS_OUT {
    mat4 worldMat;
} tcs_in[];

out TCS_OUT {
    mat4 worldMat;
} tcs_out[];

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcs_out[gl_InvocationID].worldMat = tcs_in[gl_InvocationID].worldMat;

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
	