#version 430 core
in vec2 texCoord;

flat in int gsprite_id;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int id;

uniform sampler2D u_textureSampler;

void main() {
    fragColor = texture(u_textureSampler, texCoord);
    id = gsprite_id;
}
