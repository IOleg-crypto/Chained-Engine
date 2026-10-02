#version 430 core

#include "../include/surface.glsl"
#include "../include/fog.glsl"

uniform vec3 viewPos;
uniform float uTime;

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

void main()
{
    vec2 uv = ProcessUV(fragTexCoord);
    vec4 baseColor = colDiffuse * fragColor;
    if (useTexture == 1)
    {
        baseColor *= texture(texture0, uv);
    }
    
    vec3 emissiveComp = colEmissive.rgb;
    if (useEmissiveTexture == 1)
    {
        emissiveComp *= texture(texture1, uv).rgb;
    }
    emissiveComp *= emissiveIntensity;

    vec4 result = vec4(baseColor.rgb + emissiveComp, baseColor.a);
    finalColor = ApplyFog(result, fragPosition, viewPos, uTime);
}
