#version 430 core

layout(location = 0) out vec4 fragColor;

uniform sampler2D u_textureSampler;

in vec2 texCoord;

void main() {
    fragColor = texture(u_textureSampler, texCoord);
}

