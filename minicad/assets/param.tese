#version 430 core

layout (quads, equal_spacing, ccw) in;

uniform mat4 mMat;
uniform mat4 vMat;
uniform mat4 pMat;

uniform float minorRad;
uniform float majorRad;

#define PI 3.14159265359

void main() {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    float x = u * 2 * PI;
    float y = v * 2 * PI;

    // torus parametrization
    vec4 p = vec4(cos(x)*(minorRad*cos(y) + majorRad),
                  minorRad*sin(y),
                  -sin(x)*(minorRad*cos(y) + majorRad), 
                  1.0);

    gl_Position = pMat*vMat*mMat*p;
}
