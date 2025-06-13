#version 430 core
in vec2 texCoord;

flat in int gsprite_id;
flat in int gstate;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out int id;

uniform sampler2D u_textureSampler;

void main() {
    if (gstate == 2) {
        discard;
    }
    fragColor = texture(u_textureSampler, texCoord);
    if (gstate != 0) {
        fragColor = vec4(vec3(fragColor) * vec3(0.5, 0.5, 1.0), fragColor.w);
    }
    if (fragColor.a < 0.05) {
        discard;
    }
    id = gsprite_id;
}
