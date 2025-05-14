// USAGE:
// 1. Remember to set
//      ERAY_GL_CALL( glEnable(GL_BLEND) );
//      ERAY_GL_CALL( glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) );
//
// 2. To draw the grid, bind the shader, fill the u_pvMat, u_vInvMat and u_camWorldPos uniforms and
// invoke glDrawArrays(GL_TRIANGLES, 0, 6)

#version 430 core

const vec3 Pos[4] = vec3[4](
    vec3(-1.0, 0.0, -1.0),  //
    vec3(1.0,  0.0, -1.0),  //
    vec3(1.0,  0.0, 1.0),   //
    vec3(-1.0, 0.0, 1.0)    //
);

const int Indices[6] = int[6](2, 3, 0, 0, 1, 2);

uniform mat4 u_pvMat;
uniform mat4 u_vInvMat;
uniform vec3 u_camWorldPos;
uniform float u_gridSize = 100.0;

out vec3 fragWorldPos;
out float camTilt;

void main() {
    vec3 pos = Pos[Indices[gl_VertexID]] * u_gridSize;
    pos.x -= u_camWorldPos.x;
    pos.z -= u_camWorldPos.z;

    gl_Position = u_pvMat*vec4(pos, 1.0);

    camTilt = abs(vec3(normalize(u_vInvMat*vec4(0.0, 0.0, 1.0, 0.0)))).y;
    fragWorldPos = pos;
}
