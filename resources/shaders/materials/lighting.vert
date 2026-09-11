#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;
layout(location = 5) in vec3 a_Tangent;

#include "../include/camera.glsl"

layout(std430, binding = 2) readonly buffer InstanceTransforms {
    mat4 u_InstanceModels[];
};

uniform int u_IsInstanced;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 u_LightSpaceMatrix;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out mat3 fragTBN;
out vec4 fragPosLightSpace;

void main()
{
    vec3 vPos = a_Position;
    vec3 vNormal = a_Normal;
    vec3 vTangent = a_Tangent;

    mat4 modelMat = (u_IsInstanced != 0) ? u_InstanceModels[gl_InstanceID] : matModel;
    mat4 normMat = (u_IsInstanced != 0) ? modelMat : matNormal;

    fragPosition = vec3(modelMat * vec4(vPos, 1.0));
    fragTexCoord = a_TexCoord;
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);

    vec3 N = normalize(vec3(normMat * vec4(vNormal, 0.0)));
    vec3 T;

    if (length(vTangent) < 0.01)
    {
        vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        T = normalize(cross(up, N));
    }
    else
    {
        T = normalize(vec3(normMat * vec4(vTangent, 0.0)));
    }

    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    fragNormal = N;
    fragTBN = mat3(T, B, N);

    // Transform vertex position to light space for shadow mapping
    fragPosLightSpace = u_LightSpaceMatrix * vec4(fragPosition, 1.0);

    gl_Position = u_ViewProjection * modelMat * vec4(vPos, 1.0);
}
