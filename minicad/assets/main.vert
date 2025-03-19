#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;

out vec3 fragNorm;

uniform mat4 mMat;
uniform mat4 vMat;
uniform mat4 pMat;

void main() {
    gl_Position = pMat*vMat*mMat*vec4(aPos, 1.0);
    fragNorm = aNorm;
}
