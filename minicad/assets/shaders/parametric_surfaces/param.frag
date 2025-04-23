#version 430 core
layout(location = 0) out vec4 fragColor;
layout(location = 1) out int lid;

uniform vec4 u_color = vec4(1, 0.54, 0.42, 1.0);
uniform vec4 u_fillColor = vec4(vec3(1, 0.54, 0.42) * 0.3, 1.0);
uniform vec4 u_selectionColor = vec4(0.33, 0.7, 0.8, 1.0);
uniform vec4 u_fillSelectionColor = vec4(vec3(0.47, 0.86, 1) * 0.3, 1.0);

flat in int id;
flat in int state;

uniform bool u_fill = false;

void main() {
    if (!u_fill) {
        fragColor = state == 0 ? u_color : u_selectionColor;
    } else {
        fragColor = state == 0 ? u_fillColor : u_fillSelectionColor;
    }
    lid = id;
}
