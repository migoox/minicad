#version 430 core

layout(location = 0) out vec4 fragColor;
uniform vec4 u_color = vec4(1.0);

void main() {
    fragColor = u_color;
}
