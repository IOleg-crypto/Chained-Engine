#ifndef CH_LIGHTING_GLSL
#define CH_LIGHTING_GLSL

#include "surface.glsl"

// ============================================================
// Light Structures & Uniforms
// ============================================================

// Generic Directional Light (can represent Sun, Moon, distant scene light)
struct DirectionalLight
{
    vec3  direction;  // World-space direction pointing along the light rays
    float intensity;  // Light intensity multiplier
    vec4  color;      // Light RGB color + alpha
    float ambient;    // Directional ambient floor
};

// Packed Dynamic Light (Point, Spot, or secondary Directional)
struct Light
{
    vec4  color;       // 16 bytes: rgb = color, a = unused
    vec3  position;    // 12 bytes: world-space position (Point / Spot)
    float intensity;   //  4 bytes: light intensity multiplier
    vec3  direction;   // 12 bytes: cone/travel direction (Spot / Directional)
    float radius;      //  4 bytes: maximum attenuation reach distance
    float innerCutoff; //  4 bytes: spot inner cone angle in degrees
    float outerCutoff; //  4 bytes: spot outer cone angle in degrees
    int   type;        //  4 bytes: 0 = Point, 1 = Spot, 2 = Directional
    int   enabled;     //  4 bytes: 1 = active, 0 = disabled
};

// SSBO of Dynamic Scene Lights
#define MAX_LIGHTS 256
layout(std430, binding = 0) buffer LightData
{
    Light lights[];
};

// Primary Scene Directional Light
uniform DirectionalLight u_MainLight;

// Global Lighting & Scene Uniforms
uniform vec3  lightDir;
uniform vec4  lightColor;
uniform float ambient;
uniform vec4  skyAmbientColor;
uniform vec3  viewPos;
uniform float uTime;
uniform int   uLightCount;
uniform float uExposure;
uniform float uGamma;
uniform float uMode;

// ============================================================
// Lighting Calculation Functions
// ============================================================

const float HALF_LAMBERT_WRAP = 0.15;

// ── Generic Directional Light ──────────────────────────────
vec3 CalcDirectionalLight(in vec3 dir, in vec4 col, in float intensity, in Surface s)
{
    vec3 L = normalize(-dir);

    // Half-Lambert wrap: smooth transition into shadow
    float rawNdotL     = dot(s.normal, L);
    float wrappedNdotL = clamp(rawNdotL * (1.0 - HALF_LAMBERT_WRAP) + HALF_LAMBERT_WRAP, 0.0, 1.0);

    vec3 diffuse = s.diffuse * col.rgb * wrappedNdotL;

    vec3 specular = vec3(0.0);
    if (rawNdotL > 0.0)
    {
        vec3 H = normalize(L + s.viewDir);
        float NdotH = max(dot(s.normal, H), 0.0);
        float HdotV = max(dot(H, s.viewDir), 0.0);

        float spec = pow(NdotH, s.shininess);
        float fresnelBase = 1.0 - HdotV;
        float fresnelSquared = fresnelBase * fresnelBase;
        float fresnel = fresnelSquared * fresnelSquared * fresnelBase;
        vec3 F = s.specular + (vec3(1.0) - s.specular) * fresnel;

        specular = F * col.rgb * spec;
    }

    return (diffuse + specular) * intensity;
}

vec3 CalcDirectionalLight(in DirectionalLight dLight, in Surface s)
{
    vec3 dir = length(dLight.direction) > 0.0001 ? normalize(dLight.direction) : vec3(0.0, -1.0, 0.0);
    return CalcDirectionalLight(dir, dLight.color, dLight.intensity, s);
}

// ── Point Light Attenuation & Evaluation ───────────────────
float PointLightAttenuation(in float distance, in float radius)
{
    if (distance >= radius) return 0.0;

    float attenuation = 1.0 / (1.0 + distance * distance);
    float fadeStart = radius * 0.8;
    if (distance > fadeStart)
    {
        float t = (distance - fadeStart) / (radius - fadeStart);
        attenuation *= 1.0 - t * t;
    }
    return attenuation;
}

vec3 CalcPointLight(in Light light, in Surface s)
{
    if (light.enabled == 0) return vec3(0.0);

    vec3 lightVector = light.position - s.position;
    float distance = length(lightVector);

    float attenuation = PointLightAttenuation(distance, light.radius);
    if (attenuation <= 0.0) return vec3(0.0);

    vec3 L = normalize(lightVector);
    float NdotL = max(dot(s.normal, L), 0.0);

    vec3 diffuse = s.diffuse * light.color.rgb * NdotL;

    vec3 specular = vec3(0.0);
    if (NdotL > 0.0)
    {
        vec3 H = normalize(L + s.viewDir);
        float NdotH = max(dot(s.normal, H), 0.0);
        float HdotV = max(dot(H, s.viewDir), 0.0);

        float spec = pow(NdotH, s.shininess);
        float fresnelBase = 1.0 - HdotV;
        float fresnelSquared = fresnelBase * fresnelBase;
        float fresnel = fresnelSquared * fresnelSquared * fresnelBase;
        vec3 F = s.specular + (vec3(1.0) - s.specular) * fresnel;

        specular = F * light.color.rgb * spec;
    }

    return (diffuse + specular) * attenuation * light.intensity;
}

// ── Spot Light Evaluation ──────────────────────────────────
vec3 CalcSpotLight(in Light light, in Surface s)
{
    if (light.enabled == 0) return vec3(0.0);

    vec3 lightVector = light.position - s.position;
    float distance = length(lightVector);

    float attenuation = PointLightAttenuation(distance, light.radius);
    if (attenuation <= 0.0) return vec3(0.0);

    vec3 L = normalize(lightVector);

    // Spot cone attenuation
    vec3 spotDir = normalize(light.direction);
    float theta = dot(-L, spotDir);
    float innerCos = cos(radians(light.innerCutoff));
    float outerCos = cos(radians(light.outerCutoff));

    if (theta <= outerCos) return vec3(0.0);

    float spotIntensity = clamp((theta - outerCos) / max(0.0001, (innerCos - outerCos)), 0.0, 1.0);

    float NdotL = max(dot(s.normal, L), 0.0);
    vec3 diffuse = s.diffuse * light.color.rgb * NdotL;

    vec3 specular = vec3(0.0);
    if (NdotL > 0.0)
    {
        vec3 H = normalize(L + s.viewDir);
        float NdotH = max(dot(s.normal, H), 0.0);
        float HdotV = max(dot(H, s.viewDir), 0.0);

        float spec = pow(NdotH, s.shininess);
        float fresnelBase = 1.0 - HdotV;
        float fresnelSquared = fresnelBase * fresnelBase;
        float fresnel = fresnelSquared * fresnelSquared * fresnelBase;
        vec3 F = s.specular + (vec3(1.0) - s.specular) * fresnel;

        specular = F * light.color.rgb * spec;
    }

    return (diffuse + specular) * attenuation * spotIntensity * light.intensity;
}

// ── Evaluate any dynamic light by its type ─────────────────
vec3 CalcDynamicLight(in Light light, in Surface s)
{
    if (light.enabled == 0) return vec3(0.0);

    if (light.type == 0)
        return CalcPointLight(light, s);
    else if (light.type == 1)
        return CalcSpotLight(light, s);
    else if (light.type == 2)
    {
        vec3 dir = length(light.direction) > 0.0001 ? normalize(light.direction) : vec3(0.0, -1.0, 0.0);
        return CalcDirectionalLight(dir, light.color, light.intensity, s);
    }
    return vec3(0.0);
}

// ── Ambient & Sky Environment Lighting ─────────────────────
vec3 CalcAmbientLighting(in Surface s, in vec4 skyAmbient, in float ambValue)
{
    const float MIN_AMBIENT = 0.08;
    float effectiveAmbient = max(ambValue, MIN_AMBIENT);
    vec3 sky = skyAmbient.rgb * skyAmbient.a;

    vec3 ambientContrib = s.diffuse * (effectiveAmbient + sky) * s.occlusion;
    ambientContrib += s.specular * sky * s.occlusion;

    return ambientContrib;
}

#endif // CH_LIGHTING_GLSL
