#version 430 core
#define CONTROL_POINTS_COUNT 16

layout (vertices=CONTROL_POINTS_COUNT) out;

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    int tessLevelX = 16;
    int tessLevelY = 16;
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
	