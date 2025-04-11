#version 430 core

layout (vertices=4) out;

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    // invocation zero controls tessellation levels for the entire patch
    // if (gl_InvocationID == 0)
    // {
        gl_TessLevelOuter[0] = 1.0;

        // gl_TessLevelInner[0] = tessLevelX;
        gl_TessLevelOuter[1] = 64.0;
    // }
}
	