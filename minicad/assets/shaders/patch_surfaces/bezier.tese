#version 430 core

layout (quads, equal_spacing, ccw) in;

uniform mat4 u_pvMat;

vec3 find_bezier_degree_3(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float u = (1.0 - t);
    float b3 = t*t*t;
    float b2 = 3.0*t*t*u;
    float b1 = 3.0*t*u*u;
    float b0 = u*u*u;
    return p0*b0 + p1*b1 + p2*b2 + p3*b3;
}

vec3 find_bezier_surface_degree_3(float u, float v) 
{ 
    vec3 pu[4]; 
    for (int i = 0; i < 4; ++i) { 
       vec3 p0 = gl_in[i*4].gl_Position.xyz;
       vec3 p1 = gl_in[i*4 + 1].gl_Position.xyz;
       vec3 p2 = gl_in[i*4 + 2].gl_Position.xyz;
       vec3 p3 = gl_in[i*4 + 3].gl_Position.xyz;

       pu[i] = find_bezier_degree_3(p0, p1, p2, p3, u); 
    } 
    return find_bezier_degree_3(pu[0], pu[1], pu[2], pu[3], v); 
}

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    vec4 p = vec4(find_bezier_surface_degree_3(u, v), 1.0);
    gl_Position = u_pvMat*p;
}

