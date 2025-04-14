#version 430 core
#define CONTROL_POINTS_COUNT 4

layout (vertices=CONTROL_POINTS_COUNT) out;

uniform int u_bezier_degree = 3;

uniform mat4 u_pvMat;
uniform float u_width;
uniform float u_height;

float find_tess_level() {
    vec2 screen_control_points[CONTROL_POINTS_COUNT];

    for (int i = 0; i < CONTROL_POINTS_COUNT; ++i) {
        vec4 p = u_pvMat * gl_in[i].gl_Position;
        screen_control_points[i] = 0.5*vec2(u_width, u_height)*((p.xy/p.w) + vec2(1.0));
    }

    float polyline_length = 0.0;
    for (int i = 0; i < CONTROL_POINTS_COUNT - 1; ++i) { 
        polyline_length += distance(screen_control_points[i], screen_control_points[i + 1]);
    }

    return clamp(polyline_length, 4.0, 64.0);
}

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    if (gl_InvocationID == 0)
    {
        gl_TessLevelOuter[0] = float(1);
        gl_TessLevelOuter[1] = find_tess_level();
    }
}
	