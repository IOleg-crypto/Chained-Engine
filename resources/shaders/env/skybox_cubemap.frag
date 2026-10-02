#version 430 core
#include "../include/color_space.glsl"

in vec3 v_Position;

uniform samplerCube u_Cubemap;

uniform int u_IsHDR;
uniform float u_Exposure;
uniform float u_Brightness;
uniform float u_Contrast;
uniform int u_VFlipped;
uniform int u_HFlipped;
uniform float u_Rotation;

#include "../include/fog_skybox.glsl"

layout(location = 0) out vec4 finalColor;

void main()
{
    vec3 direction = normalize(v_Position);
    if (u_Rotation != 0.0)
    {
        float s = sin(u_Rotation);
        float c = cos(u_Rotation);
        direction = vec3(c * direction.x + s * direction.z, direction.y, -s * direction.x + c * direction.z);
    }
    if (u_VFlipped == 1) direction.y = -direction.y;
    if (u_HFlipped == 1) direction.x = -direction.x;
    
    // Sample the environment cubemap
    vec3 color = texture(u_Cubemap, direction).rgb;

    // 1. Convert to Linear if LDR
    if (u_IsHDR == 0) color = pow(color, vec3(2.2));

    // 2. Exposure & Color Correction
    color *= u_Exposure;
    color = color + u_Brightness;
    color = (color - 0.5) * u_Contrast + 0.5;

    // Output straight to HDR buffer (tonemapping is in post-process)
    vec4 background = vec4(max(color, vec3(0.0)), 1.0);

    // 4. Unified Horizon & Ground Fog
    if (fogEnabled == 1) {
        float verticalFactor = clamp(1.0 - (direction.y + 0.05) * 10.0, 0.0, 1.0);
        float fogFactor = pow(verticalFactor, 2.0); 
        float horizonHaze = pow(1.0 - abs(direction.y), 5.0) * 0.5;
        fogFactor = max(fogFactor, horizonHaze);
        fogFactor = clamp(fogFactor * clamp(fogDensity * 5.0, 0.0, 1.0), 0.0, 1.0);
        finalColor = mix(background, fogColor, fogFactor);
    } else {
        finalColor = background;
    }
}
