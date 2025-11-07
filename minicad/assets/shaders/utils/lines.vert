#version 430 core

layout (location = 0) in vec3 a_worldPos;
layout (location = 1) in vec3 a_color;
uniform mat4 u_pvMat;

out vec3 color;

void main() {
    gl_Position = u_pvMat * vec4(a_worldPos, 1.0); 
    color = a_color;
}
