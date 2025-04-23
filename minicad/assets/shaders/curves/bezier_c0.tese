#version 430 core

layout(isolines, equal_spacing) in;

uniform mat4 u_pvMat;
uniform int u_bezier_degree = 3;

patch in float isolinesCount;

vec3 find_bezier_degree_3(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float u = (1.0 - t);
    float b3 = t*t*t;
    float b2 = 3.0*t*t*u;
    float b1 = 3.0*t*u*u;
    float b0 = u*u*u;
    return p0*b0 + p1*b1 + p2*b2 + p3*b3;
}

vec3 find_bezier_degree_2(vec3 p0, vec3 p1, vec3 p2, float t) {
    float u = (1.0 - t);
    float b2 = t*t;
    float b1 = 2.0*t*u;
    float b0 = u*u;
    return p0*b0 + p1*b1 + p2*b2;
}

void main()
{
    float t = gl_TessCoord.x  / isolinesCount + gl_TessCoord.y;
    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;
    vec3 p3 = gl_in[3].gl_Position.xyz;

    vec3 p = p0;
    if (u_bezier_degree >= 3) {
        p = find_bezier_degree_3(p0, p1, p2, p3, t);
    } else if (u_bezier_degree == 2) {
        p = find_bezier_degree_2(p0, p1, p2, t);
    } else if (u_bezier_degree == 1) {
        p = mix(p0, p1, t);
    }

    gl_Position = u_pvMat*vec4(p, 1.0);
}
