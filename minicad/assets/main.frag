#version 430 core
out vec4 FragColor;
in vec3 fragNorm;

void main() {
    FragColor = vec4(abs(fragNorm), 1.0);
}
