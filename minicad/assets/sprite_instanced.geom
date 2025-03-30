#version 430 core
layout(points) in;
layout(triangle_strip, max_vertices=4) out;

uniform mat4 u_pvMat;
uniform float u_aspectRatio;
uniform float u_scale = 0.04;

out vec2 texCoord;

const vec3 Pos[4] = vec3[4](
    vec3(-1.0,  -1.0, 0.0),
    vec3(-1.0,  1.0,  0.0),
    vec3(1.0,   -1.0, 0.0),
    vec3(1.0,   1.0,  0.0)
);

const vec2 TexCoords[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

vec3 billboard_pos(vec3 pos) {
    vec4 ndc = u_pvMat * gl_in[0].gl_Position;
    ndc /= ndc.w;
    pos.x /= u_aspectRatio;
    pos *= u_scale;
    pos += ndc.xyz;
    return pos;
}

void main() {
    gl_Position = vec4(billboard_pos(Pos[0]), 1.0);
    texCoord = TexCoords[0];
    EmitVertex();
    
    gl_Position = vec4(billboard_pos(Pos[1]), 1.0);
    texCoord = TexCoords[1];
    EmitVertex();
    
    gl_Position = vec4(billboard_pos(Pos[2]), 1.0);
    texCoord = TexCoords[2];
    EmitVertex();
    
    gl_Position = vec4(billboard_pos(Pos[3]), 1.0);
    texCoord = TexCoords[3];
    EmitVertex();
    
    EndPrimitive();
}
