#ifndef CH_SURFACE_GLSL
#define CH_SURFACE_GLSL

#include "color_space.glsl"

// ============================================================
// Material Texture Samplers
// ============================================================
uniform sampler2D texture0; // Albedo / Base Color
uniform sampler2D texture1; // Metallic (B) & Roughness (G)
uniform sampler2D texture2; // Normal Map
uniform sampler2D texture3; // Roughness (legacy standalone map)
uniform sampler2D texture4; // Ambient Occlusion (R)
uniform sampler2D texture5; // Emissive Map

// ============================================================
// Material Properties & Feature Flags
// ============================================================
uniform vec4  colDiffuse;
uniform vec4  colEmissive;
uniform float metalness;
uniform float roughness;
uniform float shininess;
uniform float emissiveIntensity;

uniform int useTexture;
uniform int useNormalMap;
uniform int useMetallicMap;
uniform int useRoughnessMap;
uniform int useOcclusionMap;
uniform int useEmissiveTexture;

// ============================================================
// Surface Representation
// ============================================================
struct Surface
{
    vec3  position;   // World position
    vec3  normal;     // World normal (perturbed by normal map if active)
    vec3  viewDir;    // Unit vector pointing towards camera
    vec3  albedo;     // Base diffuse color in linear space
    vec3  diffuse;    // Diffuse reflectance: albedo * (1.0 - metalness)
    vec3  specular;   // Specular F0 reflectance: mix(0.04, albedo, metalness)
    float roughness;  // Surface roughness clamped to [0.04, 1.0]
    float metalness;  // Metalness clamped to [0.0, 1.0]
    float shininess;  // Blinn-Phong specular power derived from roughness
    float occlusion;  // Ambient occlusion factor [0.0, 1.0]
    vec3  emissive;   // Linear emissive color contribution
    float alpha;      // Surface opacity [0.0, 1.0]
};

// ── Sample normal from map with TBN ────────────────────────
vec3 SampleNormalMap(in vec3 vertexNormal, in mat3 tbn, in vec2 uv, in int hasNormalMap)
{
    vec3 normal = normalize(vertexNormal);
    if (hasNormalMap == 1)
    {
        vec3 mapNormal = texture(texture2, uv).rgb * 2.0 - 1.0;
        normal = normalize(tbn * mapNormal);
    }
    return normal;
}

// ── Construct complete Surface structure ───────────────────
Surface CreateSurface(in vec3 worldPos,
                      in vec3 vertexNormal,
                      in mat3 tbn,
                      in vec2 uv,
                      in vec4 vertexColor,
                      in vec3 cameraPos)
{
    Surface s;
    s.position = worldPos;
    s.viewDir  = normalize(cameraPos - worldPos);
    s.normal   = SampleNormalMap(vertexNormal, tbn, uv, useNormalMap);

    // 1. Albedo & Alpha
    vec4 baseColor = colDiffuse;
    if (length(vertexColor.rgb) > 0.01)
    {
        baseColor *= vertexColor;
    }
    if (useTexture == 1)
    {
        vec4 sampled = texture(texture0, uv);
        baseColor.rgb *= ToLinear(sampled.rgb);
        baseColor.a   *= sampled.a;
    }
    s.albedo = baseColor.rgb;
    s.alpha  = baseColor.a;

    // 2. Material factors
    s.metalness = clamp(metalness, 0.0, 1.0);
    s.roughness = clamp(roughness, 0.04, 1.0);
    s.occlusion = 1.0;

    if (useMetallicMap == 1)
    {
        vec4 mrSample = texture(texture1, uv);
        s.metalness = clamp(s.metalness * mrSample.b, 0.0, 1.0);
        if (useRoughnessMap == 1)
        {
            s.roughness = clamp(s.roughness * mrSample.g, 0.04, 1.0);
        }
    }
    else if (useRoughnessMap == 1)
    {
        s.roughness = clamp(s.roughness * texture(texture3, uv).r, 0.04, 1.0);
    }

    if (useOcclusionMap == 1)
    {
        s.occlusion = texture(texture4, uv).r;
    }

    // 3. Diffuse, Specular and Shininess
    s.diffuse   = s.albedo * (1.0 - s.metalness);
    s.specular  = mix(vec3(0.04), s.albedo, s.metalness);
    s.shininess = max(1.0, (1.0 - s.roughness) * 128.0);

    // 4. Emissive
    s.emissive = colEmissive.rgb;
    if (useEmissiveTexture == 1)
    {
        s.emissive *= ToLinear(texture(texture5, uv).rgb);
    }
    s.emissive *= emissiveIntensity;

    return s;
}

#endif // CH_SURFACE_GLSL
