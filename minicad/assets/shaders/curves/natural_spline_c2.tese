#version 430 core

layout(isolines, equal_spacing) in;

uniform mat4 u_pvMat;
uniform int u_bezier_degree = 3;

patch in float isolinesCount;

vec3 find_power_degree_3(vec3 a, vec3 b, vec3 c, vec3 d, float t) {
    return a + b*t + c*t*t + d*t*t*t;
}

void main()
{
    float t = gl_TessCoord.x  / isolinesCount + gl_TessCoord.y;
    vec3 a = gl_in[0].gl_Position.xyz;
    vec3 b = gl_in[1].gl_Position.xyz;
    vec3 c = gl_in[2].gl_Position.xyz;
    vec3 d = gl_in[3].gl_Position.xyz;
    vec3 start = gl_in[4].gl_Position.xyz;
    vec3 end = gl_in[5].gl_Position.xyz;

    vec3 p = find_power_degree_3(a, b, c, d, t * distance(start, end));

    gl_Position = u_pvMat*vec4(p, 1.0);
}
