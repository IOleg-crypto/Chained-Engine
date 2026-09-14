#version 430 core

#include "../include/color_space.glsl"

in vec3 v_Position;

layout(binding = 0) uniform sampler2D u_CrossMap;

uniform int u_IsHDR;
uniform float u_Exposure;
uniform float u_Brightness;
uniform float u_Contrast;
uniform int u_VFlipped;
uniform int u_HFlipped;
uniform float u_Rotation;

#include "../include/fog_skybox.glsl"

layout(location = 0) out vec4 finalColor;

vec2 DirectionToHorizontalCrossUV(vec3 direction)
{
    vec3 ad = abs(direction);
    vec2 faceUV;
    vec2 atlasCell;

    if (ad.x >= ad.y && ad.x >= ad.z)
    {
        if (direction.x > 0.0)
        {
            faceUV = vec2(-direction.z, -direction.y) / ad.x;
            atlasCell = vec2(2.0, 1.0);
        }
        else
        {
            faceUV = vec2(direction.z, -direction.y) / ad.x;
            atlasCell = vec2(0.0, 1.0);
        }
    }
    else if (ad.y >= ad.x && ad.y >= ad.z)
    {
        if (direction.y > 0.0)
        {
            faceUV = vec2(direction.x, direction.z) / ad.y;
            atlasCell = vec2(1.0, 0.0);
        }
        else
        {
            faceUV = vec2(direction.x, -direction.z) / ad.y;
            atlasCell = vec2(1.0, 2.0);
        }
    }
    else
    {
        if (direction.z > 0.0)
        {
            faceUV = vec2(direction.x, -direction.y) / ad.z;
            atlasCell = vec2(1.0, 1.0);
        }
        else
        {
            faceUV = vec2(-direction.x, -direction.y) / ad.z;
            atlasCell = vec2(3.0, 1.0);
        }
    }

    faceUV = faceUV * 0.5 + 0.5;
    return (atlasCell + faceUV) / vec2(4.0, 3.0);
}

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

    vec2 uv = DirectionToHorizontalCrossUV(direction);
    vec3 color = texture(u_CrossMap, uv).rgb;

    // Convert to linear if source is LDR.
    if (u_IsHDR == 0) color = ToLinear(color);

    color *= u_Exposure;
    color += u_Brightness;
    color = (color - 0.5) * u_Contrast + 0.5;

    vec4 background = vec4(max(color, vec3(0.0)), 1.0);

    if (fogEnabled == 1)
    {
        float horizonEffect = 1.0 - abs(direction.y);
        horizonEffect = pow(horizonEffect, 3.0);
        float fogFactor = clamp(horizonEffect * clamp(fogDensity * 10.0, 0.0, 1.0), 0.0, 1.0);
        finalColor = mix(background, fogColor, fogFactor);
    }
    else
    {
        finalColor = background;
    }
}
