#version 430 core

// USAGE: Remember to disable depth test. Draw after drawing the 3D scene.
// Render as GL_POINTS

layout (location = 0) in vec3 a_worldPos;

flat out int sprite_id;

int combine_ids(int id) {
    const int signature = 0x80000000; // MSB set to 1 for helper points
    return signature | ((~signature) & id);
}

void main() {
    gl_Position = vec4(a_worldPos, 1.0); 
    sprite_id = combine_ids(gl_VertexID);
}
