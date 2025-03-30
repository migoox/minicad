#version 430 core

// USAGE: Remember to disable depth test. Draw after drawing the 3D scene.

const vec3 Pos[4] = vec3[4](
    vec3(-1.0,  -1.0, 0.0),
    vec3(1.0,   -1.0, 0.0),
    vec3(1.0,   1.0,  0.0),
    vec3(-1.0,  1.0,  0.0)
);

const vec2 TexCoords[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

const int Indices[6] = int[6](2, 3, 0, 0, 1, 2);
uniform mat4 u_pvMat;
uniform vec3 u_worldPos;
uniform float u_aspectRatio;
uniform float u_scale = 0.04;
out vec2 texCoord;

void main() {
    int ind = Indices[gl_VertexID];

    vec4 ndc = u_pvMat * vec4(u_worldPos, 1.0);
    ndc /= ndc.w;

    vec3 pos = Pos[ind];
    pos.x /= u_aspectRatio;
    pos *= u_scale;
    pos += ndc.xyz;
    gl_Position = vec4(pos, 1.0); 

    // out
    texCoord = TexCoords[ind];
}
