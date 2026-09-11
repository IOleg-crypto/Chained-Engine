// ============================================================
// Fog — distance + height-based volumetric approximation
// ============================================================
//
// Distance modes:
//   0 = Linear:        fogFactor = clamp((d - start) / (end - start))
//   1 = Exponential:   fogFactor = 1 - exp(-d * density)
//   2 = Exp Squared:   fogFactor = 1 - exp(-(d * density)^2)
//
// Height fog:
//   Integrates fog density along the view ray from camera to fragment.
//   Low camera = more fog (looking through ground-level fog layer).
//   High camera = less fog (above the fog layer).
//   Fragment height only matters relative to the fog layer base.
//
// Sun scattering:
//   Rayleigh-like phase + forward Mie lobe gives a warm
//   glow when looking toward the sun through the fog.
// ============================================================

uniform int   fogEnabled;
uniform int   fogMode;
uniform vec4  fogColor;
uniform float fogDensity;
uniform float fogStart;
uniform float fogEnd;
uniform float fogHeightFalloff;

// ── Distance fog modes ────────────────────────────────────
float LinearFog(float distance)
{
    float range = fogEnd - fogStart;
    return clamp((distance - fogStart) / max(0.0001, range), 0.0, 1.0);
}

float ExponentialFog(float distance)
{
    float d = max(0.0, distance - fogStart);
    return 1.0 - exp(-d * fogDensity);
}

float ExponentialSquaredFog(float distance)
{
    float d = max(0.0, distance - fogStart) * fogDensity;
    return 1.0 - exp(-(d * d));
}

float GetDistanceFog(float distance)
{
    if (fogMode == 0)
        return LinearFog(distance);
    else if (fogMode == 1)
        return ExponentialFog(distance);
    else if (fogMode == 2)
        return ExponentialSquaredFog(distance);
    return 0.0;
}

// ── Height fog (Beer-Lambert integration) ─────────────────
// Approximates integrated fog density along the ray from
// camera to fragment through a height-based fog layer.
//
// The fog layer sits at y=0 and thins exponentially upward.
// When both camera and fragment are above the fog layer,
// the integral is near zero (no fog).
// When both are inside the fog layer, it behaves like
// distance fog scaled by the fog density at their heights.
float HeightFogFactor(vec3 camPos, vec3 fragPos)
{
    float falloff = fogHeightFalloff;
    if (falloff < 0.0001) return 0.0;

    float camH = camPos.y;
    float fragH = fragPos.y;

    // If both are well above ground, minimal height fog
    if (camH > 50.0 && fragH > 50.0) return 0.0;

    // Approximate: height fog density at the midpoint height
    float midHeight = (camH + fragH) * 0.5;
    float densityAtMid = exp(-max(0.0, midHeight) * falloff);

    // Scale by how much of the ray is "inside" the fog layer
    // (both endpoints below or near ground = full contribution)
    float camFactor = exp(-max(0.0, camH) * falloff);
    float fragFactor = exp(-max(0.0, fragH) * falloff);
    float coverage = (camFactor + fragFactor) * 0.5;

    return densityAtMid * coverage;
}

// ── Sun scattering through fog ────────────────────────────
vec3 ComputeFogScattering(vec3 viewDir, vec3 lightDir, vec3 lightColor, float sunIntensity)
{
    float cosTheta = dot(viewDir, normalize(lightDir));

    // Rayleigh-like phase (smooth, view-angle dependent)
    float phaseRayleigh = 0.75 * (1.0 + cosTheta * cosTheta);

    // Mie forward lobe (bright glow when looking toward sun)
    float forwardScatter = pow(max(0.0, cosTheta), 8.0) * sunIntensity;

    return lightColor * (phaseRayleigh * 0.3 + forwardScatter * 0.7);
}

// ── Horizon boost ─────────────────────────────────────────
float HorizonFogBoost(vec3 viewDir)
{
    float upness = abs(viewDir.y);
    return 1.0 + (1.0 - upness) * 0.3;
}

// ══════════════════════════════════════════════════════════
// Main apply — used by lighting.fs
// ══════════════════════════════════════════════════════════
vec3 ApplyLinearFog(vec3 surfaceColor, vec3 fragPos, vec3 viewPos, vec3 lightDir, vec3 lightColor)
{
    if (fogEnabled == 0)
        return surfaceColor;

    vec3 viewDir = normalize(viewPos - fragPos);
    float distance = length(viewPos - fragPos);

    // Distance fog
    float distFog = GetDistanceFog(distance);

    // Height fog (only adds where there's actual fog layer)
    float heightFog = HeightFogFactor(viewPos, fragPos);

    // Combine: height fog multiplies the distance fog presence,
    // not adds to it. This prevents "white paint" on close objects.
    float fogFactor = clamp(distFog + (1.0 - distFog) * heightFog * 0.5, 0.0, 1.0);

    // Horizon boost
    fogFactor *= HorizonFogBoost(viewDir);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // Convert fog color from sRGB to linear
    vec3 baseFogLinear = pow(fogColor.rgb, vec3(2.2));

    // Sun scattering
    float sunIntensity = 2.0;
    vec3 scatteredLight = ComputeFogScattering(viewDir, lightDir, lightColor, sunIntensity);

    vec3 finalFogColor = baseFogLinear + scatteredLight * 0.3;

    return mix(surfaceColor, finalFogColor, fogFactor);
}

// ══════════════════════════════════════════════════════════
// Convenience wrapper — unlit / sprite / grid shaders
// ══════════════════════════════════════════════════════════
vec4 ApplyFog(vec4 surfaceColor, vec3 fragPos, vec3 viewPos, float uTime)
{
    vec3 defaultSunDir = vec3(0.0, 1.0, 0.0);
    vec3 defaultLightColor = vec3(1.0);
    vec3 fogRGB = ApplyLinearFog(surfaceColor.rgb, fragPos, viewPos, defaultSunDir, defaultLightColor);
    return vec4(fogRGB, surfaceColor.a);
}
