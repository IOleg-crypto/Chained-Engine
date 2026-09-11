// shadow.glsl — Shadow mapping utilities
//
// How it works:
//   1. The ShadowPass renders the scene from the light's point of view into a depth texture.
//   2. In the lighting shader, we transform each fragment into light-space.
//   3. We compare the fragment's depth against the shadow map depth.
//   4. If the fragment is farther from the light than what's stored in the map, it's in shadow.
//
// Uniforms (set from C++):
//   sampler2D u_ShadowMap    — the depth texture from the ShadowPass
//   mat4      u_LightSpaceMatrix — transforms world coords to light clip space
//   float     u_ShadowBias  — offset to prevent shadow acne (self-shadowing artifacts)
//   int       u_ShadowsEnabled — 0 = no shadows, 1 = shadows on

uniform sampler2D u_ShadowMap;
uniform mat4 u_LightSpaceMatrix;
uniform float u_ShadowBias;
uniform int u_ShadowsEnabled;

// Converts a world-space position to shadow map coordinates.
// Returns vec3(x, y, z) where x,y are UV coords [0,1] and z is the depth.
vec3 ShadowMapCoords(vec4 fragPosLightSpace)
{
    // Perspective divide: convert from clip space to NDC [-1, +1]
    vec3 ndc = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform from NDC [-1,+1] to texture coordinates [0,1]
    // x and y become UV coords, z is the depth we compare against
    return vec3(ndc.x * 0.5 + 0.5, ndc.y * 0.5 + 0.5, ndc.z * 0.5 + 0.5);
}

// Basic shadow test: returns 0.0 (fully in shadow) or 1.0 (fully lit).
float ShadowCalculationBasic(vec4 fragPosLightSpace)
{
    if (u_ShadowsEnabled == 0) return 1.0;

    vec3 coords = ShadowMapCoords(fragPosLightSpace);

    // Fragments behind the light's far plane are not in shadow
    if (coords.z > 1.0) return 1.0;

    // Get the closest depth from the shadow map
    float closestDepth = texture(u_ShadowMap, coords.xy).r;

    // Current fragment's depth (with bias to prevent acne)
    float currentDepth = coords.z - u_ShadowBias;

    // Compare: is this fragment farther than what the shadow map recorded?
    return currentDepth > closestDepth ? 0.0 : 1.0;
}

// PCF (Percentage-Closer Filtering) with a compact 2x2 texel grid. Four samples
// are enough for a soft edge while keeping the per-fragment shadow cost bounded.
float ShadowCalculationPCF(vec4 fragPosLightSpace)
{
    if (u_ShadowsEnabled == 0) return 1.0;

    vec3 coords = ShadowMapCoords(fragPosLightSpace);

    // Fragments behind the light's far plane or outside frustum are not in shadow
    if (coords.z > 1.0 || coords.x < 0.0 || coords.x > 1.0 || coords.y < 0.0 || coords.y > 1.0)
        return 1.0;

    float shadow = 0.0;
    float currentDepth = coords.z - u_ShadowBias;

    // Sample a 2x2 grid of neighbouring texels and average the results.
    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    for (int x = 0; x <= 1; ++x)
    {
        for (int y = 0; y <= 1; ++y)
        {
            float pcfDepth = texture(u_ShadowMap, coords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth > pcfDepth ? 0.0 : 1.0;
        }
    }

    return shadow / 4.0;
}
