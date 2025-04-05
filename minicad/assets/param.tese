#version 430 core

layout (quads, equal_spacing, ccw) in;

// uniform mat4 mMat;
uniform mat4 u_vMat;
uniform mat4 u_pMat;

in TCS_OUT {
    mat4 worldMat;
    vec2 radii;
    int id;
    int state;
} tes_in[];

flat out int id;
flat out int state;

#define PI 3.14159265359

void main() {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    float x = u * 2 * PI;
    float y = v * 2 * PI;

    id = tes_in[0].id;
    state = tes_in[0].state;

    float minorRad = tes_in[0].radii.x;
    float majorRad = tes_in[0].radii.y;
    // torus parametrization
    vec4 p = vec4(cos(x)*(minorRad*cos(y) + majorRad),
                  minorRad*sin(y),
                  -sin(x)*(minorRad*cos(y) + majorRad), 
                  1.0);

    gl_Position = u_pMat*u_vMat*tes_in[0].worldMat*p;
}
