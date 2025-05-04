#version 430 core

layout(isolines, equal_spacing) in;

uniform mat4 u_pvMat;
uniform int u_bezier_degree = 3;

patch in float isolinesCount;

void main()
{
    float t = gl_TessCoord.x  / isolinesCount + gl_TessCoord.y;
    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;
    vec3 p3 = gl_in[3].gl_Position.xyz;

    vec3 p4 = gl_in[4].gl_Position.xyz;
    vec3 p5 = gl_in[5].gl_Position.xyz;

    vec3 p = p4;
    p = mix(p4, p5, t);

    gl_Position = u_pvMat*vec4(p, 1.0);
}
