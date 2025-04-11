#version 430 core

layout(isolines) in;
uniform mat4 u_pvMat;

void main()
{
    float t = gl_TessCoord.x;
    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;
    vec3 p3 = gl_in[3].gl_Position.xyz;

    float u = (1.0 - t);

    float b3 = t*t*t;
    float b2 = 3.0*t*t*u;
    float b1 = 3.0*t*u*u;
    float b0 = u*u*u;

    vec3 p = p0*b0 + p1*b1 + p2*b2 + p3*b3;

    gl_Position = u_pvMat*vec4(p, 1.0);
}
