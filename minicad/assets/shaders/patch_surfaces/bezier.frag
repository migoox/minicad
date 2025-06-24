#version 430 core

layout(location = 0) out vec4 fragColor;
uniform vec4 u_color = vec4(1.0);

uniform sampler2DArray u_textureSampler;
in vec3 texCoords;

void main() {
    vec4 c = texture(u_textureSampler, texCoords);
    if (c.r < 0.5 || c.a < 0.5) {
        discard; // trimming
    }

    fragColor = u_color;
}
