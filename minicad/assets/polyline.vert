#version 430 core

// USAGE:
// Render as GL_LINE_STRIP 

layout (location = 0) in vec3 a_worldPos;
uniform mat4 u_pvMat;

void main() {
    gl_Position = u_pvMat * vec4(a_worldPos, 1.0); 
}
