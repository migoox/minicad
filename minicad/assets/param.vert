#version 430 core
layout (location = 0) in vec2 a_PatchPos;
layout (location = 1) in mat4 a_worldMat;

out VS_OUT {
    mat4 worldMat;
} vs_out;

void main() {
    gl_Position = vec4(a_PatchPos, 0.0, 1.0);
    vs_out.worldMat = a_worldMat;
}
