#version 430 core

layout(location = 0) in vec3 a_Position;

layout(std430, binding = 2) readonly buffer InstanceTransforms {
    mat4 u_InstanceModels[];
};

uniform mat4 u_LightSpaceMatrix;
uniform mat4 matModel;
uniform int u_IsInstanced;

void main()
{
    mat4 modelMat = (u_IsInstanced != 0) ? u_InstanceModels[gl_InstanceID] : matModel;
    gl_Position = u_LightSpaceMatrix * modelMat * vec4(a_Position, 1.0);
}
