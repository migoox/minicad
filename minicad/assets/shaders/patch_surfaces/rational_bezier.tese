#version 430 core

layout (isolines, equal_spacing) in;

uniform mat4 u_pvMat;
uniform bool u_horizontal;

patch in float isolinesCount;
patch in float subdivisions;

vec3 find_bezier_degree_3(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float u = (1.0 - t);
    float b3 = t*t*t;
    float b2 = 3.0*t*t*u;
    float b1 = 3.0*t*u*u;
    float b0 = u*u*u;
    return p0*b0 + p1*b1 + p2*b2 + p3*b3;
}

vec3 find_rational_bezier_surface_degree_3(float u, float v) 
{ 
    vec3 b[4][2];

    b[0][0] = gl_in[1].gl_Position.xyz;
    b[0][1] = gl_in[2].gl_Position.xyz;

    b[1][0] = (u*gl_in[5].gl_Position.xyz + v*gl_in[16].gl_Position.xyz) / ((u + v) + 0.00001);
    b[1][1] = ((1-u)*gl_in[6].gl_Position.xyz + v*gl_in[17].gl_Position.xyz)/((1-u)+v + 0.00001);

    b[2][0] = (u*gl_in[9].gl_Position.xyz + (1-v)*gl_in[18].gl_Position.xyz)/(u+(1-v) + 0.00001);
    b[2][1] = ((1-u)*gl_in[10].gl_Position.xyz + (1-v)*gl_in[19].gl_Position.xyz)/((1-u)+(1-v) + 0.00001);

    b[3][0] = gl_in[13].gl_Position.xyz;
    b[3][1] = gl_in[14].gl_Position.xyz;

    vec3 pu[4]; 
    for (int i = 0; i < 4; ++i) { 
       vec3 p0 = gl_in[i*4].gl_Position.xyz;
       vec3 p1 = b[i][0];
       vec3 p2 = b[i][1];
       vec3 p3 = gl_in[i*4 + 3].gl_Position.xyz;

       pu[i] = find_bezier_degree_3(p0, p1, p2, p3, u); 
    } 
    return find_bezier_degree_3(pu[0], pu[1], pu[2], pu[3], v); 
}

void main() {
    float u = gl_TessCoord.x;
    float v = (subdivisions + 1)/subdivisions*gl_TessCoord.y;
    if (!u_horizontal) {
        vec4 p = vec4(find_rational_bezier_surface_degree_3(u, v), 1.0);
        gl_Position = u_pvMat*p;
    } else {
        vec4 p = vec4(find_rational_bezier_surface_degree_3(v, u), 1.0);
        gl_Position = u_pvMat*p;
    }
}

