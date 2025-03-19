#version 430 core

layout (quads, equal_spacing, ccw) in;

uniform mat4 mMat;
uniform mat4 vMat;
uniform mat4 pMat;

void main() {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p10 = gl_in[2].gl_Position;
    vec4 p11 = gl_in[3].gl_Position;

    // bilinearly interpolate position coordinate across patch
    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;
    vec4 p = (p1 - p0) * v + p0;

    gl_Position = pMat * vMat * mMat * p;
}
