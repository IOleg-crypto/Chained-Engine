#version 430 core
#include "../include/color_space.glsl"

in vec3 v_Position;

layout(binding = 0) uniform sampler2D u_Panorama;

uniform int u_IsHDR;
uniform float u_Exposure;
uniform float u_Brightness;
uniform float u_Contrast;
uniform int u_VFlipped;
uniform int u_HFlipped;
uniform float u_Rotation;
uniform vec3 u_SunDir;
uniform int u_SunEnabled;
uniform float u_SunIntensity;

#include "../include/fog_skybox.glsl"

layout(location = 0) out vec4 finalColor;

vec2 SampleSpherical(vec3 dir)
{
    const vec2 invAtan = vec2(0.15915494309, 0.31830988618);
    vec2 uv = vec2(atan(dir.z, dir.x), asin(clamp(dir.y, -1.0, 1.0)));
    uv *= invAtan;
    return uv + 0.5;
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
    vec2 uv = SampleSpherical(direction);
    if (u_VFlipped == 1) uv.y = 1.0 - uv.y;
    if (u_HFlipped == 1) uv.x = 1.0 - uv.x;

    // Filter derivative seam: across the wrap boundary (uv.x jumping 0 -> 1),
    // derivatives jump to ~1.0 causing mipmap popping / vertical line artifact.
    vec2 dX = dFdx(uv);
    vec2 dY = dFdy(uv);
    if (dX.x > 0.5) dX.x -= 1.0;
    else if (dX.x < -0.5) dX.x += 1.0;
    if (dY.x > 0.5) dY.x -= 1.0;
    else if (dY.x < -0.5) dY.x += 1.0;

    // Sample the panorama with filtered derivatives
    vec3 color = textureGrad(u_Panorama, uv, dX, dY).rgb;

    // Convert to Linear if LDR (PNG/JPG)
    if (u_IsHDR == 0) color = pow(color, vec3(2.2));

    // 1. Exposure & Color Correction
    color *= u_Exposure;
    color = color + u_Brightness;
    color = (color - 0.5) * u_Contrast + 0.5;
    
    // Output straight to HDR buffer (tonemapping is in post-process)
    vec4 background = vec4(max(color, vec3(0.0)), 1.0);

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
