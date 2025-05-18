#version 430 core

layout (location = 0) in vec3 a_worldPos;

void main() {
    gl_Position = vec4(a_worldPos, 1.0); 
}

