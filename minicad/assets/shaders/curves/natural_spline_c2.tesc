#version 430 core

// first 4 points represent a, b, c and d power basis coefficients
#define CONTROL_POINTS_COUNT 4

// next 2 points represent the segment start and end
#define SEGMENT_POINTS_COUNT 2

#define ALL_POINTS_COUNT 6

layout (vertices=ALL_POINTS_COUNT) out;

uniform mat4 u_pvMat;
uniform float u_width;
uniform float u_height;

const int IsolinesCount = 2;

patch out float isolinesCount;
patch out int patch_ind;

float find_line_length() {
    const int ind0 = CONTROL_POINTS_COUNT;
    const int ind1 = CONTROL_POINTS_COUNT + 1;

    vec4 p0_world = u_pvMat * gl_in[ind0].gl_Position;
    vec2 p0_screen = 0.5*vec2(u_width, u_height)*((p0_world.xy/p0_world.w) + vec2(1.0));

    vec4 p1_world = u_pvMat * gl_in[ind1].gl_Position;
    vec2 p1_screen = 0.5*vec2(u_width, u_height)*((p1_world.xy/p1_world.w) + vec2(1.0));

    return distance(p1_screen, p0_screen);
}

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    if (gl_InvocationID == 0)
    {
        patch_ind = gl_PrimitiveID;
        isolinesCount = float(IsolinesCount);
        float tess_level = clamp(find_line_length() / float(IsolinesCount), 4.0, 64.0);

        gl_TessLevelOuter[0] = float(IsolinesCount);
        gl_TessLevelOuter[1] = tess_level;
    }
}
	