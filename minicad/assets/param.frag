#version 430 core
layout(location = 0) out vec4 fragColor;
layout(location = 1) out int lid;

uniform vec4 u_color = vec4(1, 0.54, 0.42, 1.0);

out int id;

uniform bool u_fill = false;

void main() {
    if (!u_fill) {
        fragColor = u_color;
    } else {
        fragColor = vec4(vec3(u_color) * 0.3, 1.0);
    }
    lid = id;
}
