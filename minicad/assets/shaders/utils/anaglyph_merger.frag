#version 430 core

layout(location = 0) out vec4 fragColor;

uniform sampler2D u_left;
uniform sampler2D u_right;

in vec2 texCoord;

void main() {
    vec4 right = texture(u_right, texCoord);
    right.r = 0;
    vec4 left = texture(u_left, texCoord);
    left.g = 0;
    left.b = 0;
    fragColor = right + left;
}

