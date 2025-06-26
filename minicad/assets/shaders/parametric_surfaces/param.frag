#version 430 core
layout(location = 0) out vec4 fragColor;

uniform vec4 u_color = vec4(1, 0.54, 0.42, 1.0);
uniform vec4 u_fillColor = vec4(vec3(1, 0.54, 0.42) * 0.3, 1.0);
uniform vec4 u_selectionColor = vec4(0.33, 0.7, 0.8, 1.0);
uniform vec4 u_fillSelectionColor = vec4(vec3(0.47, 0.86, 1) * 0.3, 1.0);

flat in int state;

uniform sampler2DArray u_textureSampler;
in vec3 texCoords;

uniform bool u_fill = false;

void main() {
    vec4 c = texture(u_textureSampler, texCoords);
    if (c.r < 0.5 || c.a < 0.5) {
        discard; // trimming
    }

    if (!u_fill) {
        fragColor = state == 0 ? u_color : u_selectionColor;
    } else {
        fragColor = state == 0 ? u_fillColor : u_fillSelectionColor;
    }
}
