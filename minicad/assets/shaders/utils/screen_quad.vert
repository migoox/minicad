#version 430 core

const vec3 Pos[4] = vec3[4](
    vec3(-1.0,  -1.0, 0.0),
    vec3(1.0,   -1.0, 0.0),
    vec3(1.0,   1.0,  0.0),
    vec3(-1.0,  1.0,  0.0)
);

const vec2 TexCoords[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

const int Indices[6] = int[6](2, 3, 0, 0, 1, 2);

out vec2 texCoord;

void main() {
    int ind = Indices[gl_VertexID];
    gl_Position = vec4(Pos[ind], 1.0); 

    // out
    texCoord = TexCoords[ind];
}

