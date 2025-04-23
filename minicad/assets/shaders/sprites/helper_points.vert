#version 430 core

// USAGE: Remember to disable depth test. Draw after drawing the 3D scene.
// Render as GL_POINTS

layout (location = 0) in vec3 a_worldPos;
uniform int u_parent_id = 0;

flat out int gstate;
flat out int sprite_id;

int combine_ids(int parent_id, int child_id) {
    int result = 0x80000000; // MSB set to 1

    int masked1 = parent_id & 0x1FFF; // 0x1FFF = 13 bits (2^13 - 1)
    result |= (masked1 << 18);

    int masked2 = child_id & 0x3FFFF; // 0x3FFFF = 18 bits (2^18 - 1)
    result |= masked2;

    return result;
}

void main() {
    gl_Position = vec4(a_worldPos, 1.0); 
    sprite_id = combine_ids(u_parent_id, gl_VertexID);
    gstate = 0;
}
