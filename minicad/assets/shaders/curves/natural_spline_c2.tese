#version 430 core

layout(isolines, equal_spacing) in;

uniform mat4 u_pvMat;
uniform int u_bezier_degree = 3;

patch in float isolinesCount;

vec3 find_power_degree_3(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    return p0 + p1*t + p2*t*t + p3*t*t*t;
}

void main()
{
    float t = gl_TessCoord.x  / isolinesCount + gl_TessCoord.y;
    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;
    vec3 p3 = gl_in[3].gl_Position.xyz;

    vec3 p = find_power_degree_3(p0, p1, p2, p3, t);

    gl_Position = u_pvMat*vec4(p, 1.0);
}
