#version 430 core

layout(location = 0) out vec4 fragColor;

uniform sampler2D u_left;
uniform sampler2D u_right;
uniform vec3 u_output_coeffs;

in vec2 texCoord;

void main() {
    vec4 RR = texture(u_right, texCoord);
    float R = 0.299*RR.r+0.587*RR.g+0.114*RR.b; // to grey scale

    vec4 LL = texture(u_left, texCoord);
    float L = 0.299*LL.r+0.587*LL.g+0.114*LL.b;

    vec3 result = u_output_coeffs*vec3(L, R, R);
    fragColor = vec4(result, 1.0); // colors 
}

