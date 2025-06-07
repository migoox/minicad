#version 430 core
#define CONTROL_POINTS_COUNT 20

layout (vertices=CONTROL_POINTS_COUNT) out;

uniform mat4 u_pvMat;
uniform float u_width;
uniform float u_height;

const int IsolinesCount = 2;
patch out float isolinesCount;
patch out float subdivisions;

float find_polyline_length() {
    vec2 screen_control_points[CONTROL_POINTS_COUNT];

    for (int i = 0; i < CONTROL_POINTS_COUNT; ++i) {
        vec4 p = u_pvMat * gl_in[i].gl_Position;
        screen_control_points[i] = 0.5*vec2(u_width, u_height)*((p.xy/p.w) + vec2(1.0));
    }

    float polyline_length = 0.0;
    for (int i = 0; i < CONTROL_POINTS_COUNT - 1; ++i) { 
        polyline_length += distance(screen_control_points[i], screen_control_points[i + 1]);
    }

    return polyline_length;
}

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    if (gl_InvocationID == 0)
    {
        isolinesCount = float(IsolinesCount);
        subdivisions = float(int(sqrt(100)));

        // float tess_level = clamp(find_polyline_length() / float(IsolinesCount), 4.0, 64.0);
        float tess_level = 64.0;
        // gl_TessLevelOuter[0] = (subdivisions + 1)*float(IsolinesCount);
        gl_TessLevelOuter[0] = subdivisions + 1;
        gl_TessLevelOuter[1] = tess_level;
    }
}
	