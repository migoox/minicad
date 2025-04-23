#version 430 core

// USAGE: Remember to disable depth test. Draw after drawing the 3D scene.
// Render as GL_POINTS

layout (location = 0) in vec3 a_worldPos;
layout (location = 1) in int a_state;

flat out int sprite_id;
flat out int state;

void main() {
    gl_Position = vec4(a_worldPos, 1.0); 
    state = a_state;
    sprite_id = gl_VertexID;
}
