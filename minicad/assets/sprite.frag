#version 430 core
in vec2 texCoord;
out vec4 fragColor;

uniform sampler2D u_textureSampler;

void main() {
    
    fragColor = texture(u_textureSampler, texCoord);
}
