#version 430 core

layout(location = 0) out vec4 fragColor;

uniform sampler2D u_left;
uniform sampler2D u_right;

in vec2 texCoord;

void main() {
    vec4 RR = texture(u_right, texCoord);
    float R = 0.299*RR.r+0.587*RR.g+0.114*RR.b; // to grey scale

    vec4 LL = texture(u_left, texCoord);
    float L = 0.299*LL.r+0.587*LL.g+0.114*LL.b;
    
    // vec3 result;
    // result.r = L.r;
    // result.g = R.g;
    // result.b = R.b;
    // result.r = 0.4561 * L.r + 0.500484 * L.g + 0.176381 * L.b - 0.0434706 * R.r - 0.0879388 * R.g - 0.00155529 * R.b;
    // result.g =  -0.0400822 * L.r - 0.0378246 * L.g - 0.0157589 * L.b + 0.378476 * R.r + 0.73364 * R.g - 0.0184503 * R.b;
    // result.b = -0.0152161 * L.r - 0.0205971 * L.g - 0.00546856 * L.b - 0.0721527 * R.r - 0.112961 * R.g + 1.2264 * R.b;

    fragColor = vec4(L, R, R, 1.0);
}

