#version 430 core

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int id;

flat in int gsprite_id;

layout (location = 0) in vec3 a_worldPos;

void main() {
    gl_Position = vec4(a_worldPos, 1.0); 
    id = 1;
}
