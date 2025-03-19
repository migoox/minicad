// glEnable(GL_BLEND);
// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//
//   float vertices[] = {
//       -1.F, 0.F, -1.F,  //
//       1.F,  0.F, -1.F,  //
//       1.F,  0.F, 1.F,   //
//       -1.F, 0.F, 1.F    //
//
//   };
//
//   unsigned int indices[] = {
//       0, 1, 2,  //
//       2, 3, 0   //
//   };

#version 430 core

layout(location = 0) in vec3 a_Pos; 

uniform mat4 u_pvMat;
uniform mat4 u_pvInvMat;
uniform vec3 u_camWorldPos;
uniform float u_gridSize = 100.0;

out vec3 fragWorldPos;
out vec3 rayDir;

void main() {
    vec3 pos = a_Pos * u_gridSize;
    pos.x += u_camWorldPos.x;
    pos.z += u_camWorldPos.z;

    gl_Position = u_pvMat*vec4(pos, 1.0);

    rayDir = vec3(normalize(u_pvInvMat*vec4(0.0, 0.0,-1.0, 0.0)));
    fragWorldPos = pos;
}
