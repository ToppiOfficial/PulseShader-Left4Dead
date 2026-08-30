//==================================================================================================
//
// OpenPBR Surface - base and specular lobes, for models
//
// Written against the OpenPBR Surface specification
// (AcademySoftwareFoundation/OpenPBR). Coat, fuzz, thin-film, transmission and
// subsurface are not implemented: Source 1 has no way to express the last three,
// and the first two are not needed by any shader here yet.
//
//==================================================================================================

// Optional BRDF ramp hook. A variant with stylised lighting curves defines
// PBR_BRDF_RAMP to its own combo before including this and passes a ramp
// sampler; nothing here names the feature that uses it.
#ifndef PBR_BRDF_RAMP
#define PBR_BRDF_RAMP 0
#endif

#if PBR_BRDF_RAMP
#define PBR_BRDF_RAMP_PARAMS , sampler brdfRampSampler, float brdfRampShadowScale
#define PBR_BRDF_RAMP_ARGS(s_, shadow_) , s_, shadow_
#else
#define PBR_BRDF_RAMP_PARAMS
#define PBR_BRDF_RAMP_ARGS(s_, shadow_)
#endif

static const float PI = 3.141592;
static const float ONE_OVER_PI = 0.318309;
static const float EPSILON = 0.00001;

//--------------------------------------------------------------------------------------
// Fresnel
//--------------------------------------------------------------------------------------

// Dielectric F0 from index of refraction, scaled by specular_weight.
// IOR 1.5 gives 0.04, the value most PBR pipelines hardcode.
float openpbrDielectricF0(float ior, float specularWeight)
{
    float f = (ior - 1.0) / (ior + 1.0);
    return f * f * specularWeight;
}

float3 openpbrFresnelSchlick(float3 F0, float cosTheta)
{
    float m = 1.0 - cosTheta;
    float m2 = m * m;
    return F0 + (1.0 - F0) * (m2 * m2 * m);
}

// Metal Fresnel with an F82 tint (OpenPBR "Metal", after Kutz et al.).
// Schlick fits reflectance at normal incidence and at grazing, but real metals
// dip near 82 degrees; specularColor tints that dip. specularColor = 1 collapses
// to plain Schlick.
float3 openpbrFresnelF82(float3 F0, float3 specularColor, float cosTheta)
{
    const float kCosF82 = 1.0 / 7.0;   // cos(82 degrees), the spec's fixed pivot
    float3 schlick = openpbrFresnelSchlick(F0, cosTheta);

    // Reflectance the plain Schlick curve gives at the pivot, and the value the
    // tint asks for there.
    float m82 = 1.0 - kCosF82;
    float3 schlick82 = F0 + (1.0 - F0) * (m82 * m82 * m82 * m82 * m82);
    float3 target82 = specularColor * schlick82;

    // Single correction lobe peaking at the pivot, vanishing at both ends.
    float m = 1.0 - cosTheta;
    float lobe = cosTheta * m * m * m * m * m * m;
    const float kLobeAtPivot = kCosF82 * pow(m82, 6.0);
    return saturate(schlick - lobe * ((schlick82 - target82) / kLobeAtPivot));
}

//--------------------------------------------------------------------------------------
// Specular microfacet lobe
//--------------------------------------------------------------------------------------

// GGX / Trowbridge-Reitz, Disney's alpha = roughness^2 reparametrisation.
float openpbrNdfGGX(float cosLh, float roughness)
{
    float alpha   = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / max(EPSILON, PI * denom * denom);
}

// Height-correlated Smith visibility (Heitz), with the 1/(4 NoL NoV) of the
// Cook-Torrance denominator folded in - so the caller multiplies D * V * F and
// nothing else.
float openpbrVisibilitySmith(float cosLi, float cosLo, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float lambdaV = cosLi * sqrt(cosLo * cosLo * (1.0 - alphaSq) + alphaSq);
    float lambdaL = cosLo * sqrt(cosLi * cosLi * (1.0 - alphaSq) + alphaSq);
    return 0.5 / max(EPSILON, lambdaV + lambdaL);
}

// Split-sum DFG terms, Lazarov's analytic fit. Returns the scale and bias that
// multiply F0: E = F0 * x + y. One evaluation serves energy compensation,
// diffuse albedo scaling, and the environment lookup.
float2 openpbrDFG(float roughness, float NoV)
{
    const float4 c0 = { -1, -0.0275, -0.572, 0.022 };
    const float4 c1 = { 1, 0.0425, 1.04, -0.04 };
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    return float2(-1.04, 1.04) * a004 + r.zw;
}

// Directional albedo of the specular lobe - how much energy it takes before the
// diffuse lobe underneath sees any. This is OpenPBR's slab layering by albedo
// scaling, replacing the older kd = 1 - F.
float openpbrSpecularAlbedo(float3 F0, float2 dfg)
{
    float3 E = F0 * dfg.x + dfg.y;
    return saturate(dot(E, float3(0.2126, 0.7152, 0.0722)));
}

// Turquin's multiple-scattering compensation. A single-scatter GGX lobe loses
// energy as roughness rises, which is what makes rough metals go dark and grey.
float3 openpbrEnergyCompensation(float3 F0, float2 dfg)
{
    float Ess = dfg.x + dfg.y;
    return 1.0 + F0 * (1.0 / max(EPSILON, Ess) - 1.0);
}

//--------------------------------------------------------------------------------------
// Diffuse lobe
//--------------------------------------------------------------------------------------

// EON, the energy-preserving Oren-Nayar of the OpenPBR spec, in its qualitative
// single-scatter form plus the multiple-scatter term that keeps rough diffuse
// from darkening. diffuseRoughness 0 reduces to Lambert.
//
// ponytail: no 1/PI here. Source's light intensities are authored against
// VertexLitGeneric's unnormalised Lambert, so adding the correct normalisation
// darkens every existing material by ~3x. Restore it only alongside a global
// light-intensity rescale.
float openpbrDiffuseEON(float NoL, float NoV, float LoV, float diffuseRoughness)
{
    if (diffuseRoughness <= 0.0)
        return NoL;

    float s = LoV - NoL * NoV;
    float stinv = s > 0.0 ? s / max(max(NoL, NoV), EPSILON) : s;

    float sigma = saturate(diffuseRoughness);
    float A = 1.0 / (1.0 + 0.5 * sigma);
    float B = sigma * A;

    // Single scatter, then a smooth multiple-scatter refill weighted by sigma.
    float single = NoL * (A + B * stinv);
    float multi = 0.17 * sigma * NoL * (1.0 - NoV * NoL);
    return max(0.0, single + multi);
}

float openpbrDiffuseFalloff(float3 normal, float3 lightIn, float3 lightOut,
                            float NoV, float diffuseRoughness, bool halfLambert)
{
    float NoL = dot(normal, lightIn);
    if (halfLambert)
        return pow(saturate(NoL * 0.5 + 0.5), 2.0);
    return openpbrDiffuseEON(max(0.0, NoL), NoV, dot(lightIn, lightOut), diffuseRoughness);
}

//--------------------------------------------------------------------------------------
// Surface description
//--------------------------------------------------------------------------------------

// Everything the lobes need that does not change per light. Built once per pixel
// by openpbrSetupSurface, then handed to every light and to the ambient path so
// direct and indirect agree.
struct OpenPBRSurface
{
    float3 albedo;          // diffuse reflectance after metal extinction
    float3 F0;              // specular reflectance at normal incidence
    float3 specularColor;   // F82 tint; 1 for dielectrics
    float3 energyComp;      // multiple-scattering compensation
    float  diffuseScale;    // 1 - directional albedo of the specular lobe
    float  roughness;
    float  diffuseRoughness;
    float  metalness;
    float  NoV;
    float2 dfg;
};

// f0Override is the $speculartexture value, used verbatim when SPECULAR is on.
// Otherwise F0 comes from the IOR for dielectrics and from albedo for metals.
OpenPBRSurface openpbrSetupSurface(float3 albedo, float metalness, float roughness,
                                   float diffuseRoughness, float NoV,
                                   float specularIor, float specularWeight,
                                   float3 specularTint, float3 f0Override, bool useF0Override)
{
    OpenPBRSurface s;
    s.roughness = clamp(roughness, 0.02, 1.0);
    s.diffuseRoughness = saturate(diffuseRoughness);
    s.metalness = saturate(metalness);
    s.NoV = max(EPSILON, NoV);

    float dielectricF0 = openpbrDielectricF0(specularIor, specularWeight);
    s.F0 = useF0Override ? f0Override
                         : lerp(float3(dielectricF0, dielectricF0, dielectricF0), albedo, s.metalness);
    s.specularColor = lerp(float3(1, 1, 1), specularTint, s.metalness);

    // Metals have no diffuse lobe. An explicit F0 map drives specular alone and
    // leaves albedo intact.
    s.albedo = useF0Override ? albedo : albedo * (1.0 - s.metalness);

    s.dfg = openpbrDFG(s.roughness, s.NoV);
    s.energyComp = openpbrEnergyCompensation(s.F0, s.dfg);
    s.diffuseScale = 1.0 - openpbrSpecularAlbedo(s.F0, s.dfg);
    return s;
}

//--------------------------------------------------------------------------------------
// Direct lighting
//--------------------------------------------------------------------------------------

// One punctual light. Source lights have no size, so there is no area-light term
// to fit - OpenPBR specifies a surface, not a light model.
float3 openpbrDirect(OpenPBRSurface s, float3 lightIn, float3 lightIntensity, float3 lightOut,
                     float3 normal, bool halfLambert, float diffuseFalloffOverride,
                     sampler lightWarpSampler PBR_BRDF_RAMP_PARAMS)
{
    float3 halfAngle = normalize(lightIn + lightOut);
    float cosLightIn = max(0.0, dot(normal, lightIn));
    float cosHalfAngle = max(0.0, dot(normal, halfAngle));
    float cosHalfOut = max(0.0, dot(halfAngle, lightOut));

    float diffuseFalloff = diffuseFalloffOverride >= 0.0
        ? diffuseFalloffOverride
        : openpbrDiffuseFalloff(normal, lightIn, lightOut, s.NoV, s.diffuseRoughness, halfLambert);

    float3 F = openpbrFresnelF82(s.F0, s.specularColor, cosHalfOut);
    float D = openpbrNdfGGX(cosHalfAngle, s.roughness);
    float V = openpbrVisibilitySmith(cosLightIn, s.NoV, s.roughness);

    float3 specularBRDF = F * D * V * s.energyComp;
    float3 diffuseBRDF = s.albedo * s.diffuseScale;

#if PBR_BRDF_RAMP
    // Row coordinates and the 2048 clamp are the source shader's own literals;
    // ours carries the 1/PI that its NDF omits. Explicit LOD 0 matches its
    // SampleLevel. Every Source light is punctual, so all take the punctual row.
    const float kRampDiffuseV = 0.875;
    const float kRampSpecularV = 0.375;
    const float kRampNdfMax = 2048.0 * ONE_OVER_PI;
    float cosLightHalf = max(EPSILON, dot(lightIn, halfAngle));
    float peakLobe = min(kRampNdfMax, openpbrNdfGGX(1.0, s.roughness))
                   * openpbrVisibilitySmith(cosLightHalf, cosLightHalf, s.roughness);
    float3 rampedSpecular = tex2Dlod(brdfRampSampler, float4(
        saturate(min(kRampNdfMax, D) * V / max(EPSILON, peakLobe)), kRampSpecularV, 0, 0)).rgb;
    specularBRDF = F * min(10.0, rampedSpecular * peakLobe) * s.energyComp;

    // The sampled value stands in for the N.L cosine on both lobes, and shadow
    // rides inside the index rather than scaling the result. Replacing the
    // falloff outright is why this takes priority over $lightwarptexture.
    float3 rampedDiffuseFalloff = tex2Dlod(brdfRampSampler, float4(
        max(EPSILON, diffuseFalloff * brdfRampShadowScale), kRampDiffuseV, 0, 0)).rgb;
    return rampedDiffuseFalloff * (diffuseBRDF + specularBRDF) * lightIntensity;
#endif
#if LIGHTWARPTEXTURE == 1
    // The ramp remaps the diffuse falloff only; specular keeps the true
    // geometric term or highlights would break. x2 because a neutral ramp sits
    // at 0.5, matching VertexLitGeneric.
    float3 warpedDiffuseFalloff = 2.0f * tex1D(lightWarpSampler, diffuseFalloff).rgb;
    return (diffuseBRDF * warpedDiffuseFalloff + specularBRDF * cosLightIn) * lightIntensity;
#else
    return (diffuseBRDF * diffuseFalloff + specularBRDF * cosLightIn) * lightIntensity;
#endif
}

// Split-sum specular for an environment radiance sample. Applies to metals and
// dielectrics alike - the DFG terms already carry the difference.
float3 openpbrEnvSpecular(OpenPBRSurface s, float3 radiance)
{
    float3 E = s.F0 * s.dfg.x + s.dfg.y;
    return radiance * E * s.energyComp * s.specularColor;
}

// Diffuse irradiance under the specular slab, matching the direct path's layering.
float3 openpbrEnvDiffuse(OpenPBRSurface s, float3 irradiance)
{
    return s.albedo * s.diffuseScale * irradiance;
}

// Specular occlusion from a hemispherical AO term (Lagarde). A specular lobe
// samples a narrow cone, not the hemisphere AO averages over, so applying AO
// flat leaves smooth surfaces looking dirty. Collapses to plain AO at roughness
// 1, so only smooth surfaces change.
float openpbrSpecularOcclusion(OpenPBRSurface s, float ao)
{
    return saturate(pow(s.NoV + ao, exp2(-16.0 * s.roughness - 1.0)) - 1.0 + ao);
}

//--------------------------------------------------------------------------------------
// Source engine glue - not part of the BRDF
//--------------------------------------------------------------------------------------

// Tangent frame from screen-space derivatives, for DirectX normal maps in Mikk
// Tangent Space http://www.mikktspace.com
float3x3 compute_tangent_frame(float3 N, float3 P, float2 uv, out float3 T, out float3 B, out float sign_det)
{
    float3 dp1 = ddx(P);
    float3 dp2 = ddy(P);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    sign_det = dot(dp2, cross(N, dp1)) > 0.0 ? -1 : 1;

    float3x3 M = float3x3(dp1, dp2, cross(dp1, dp2));
    float2x3 inverseM = float2x3(cross(M[1], M[2]), cross(M[2], M[0]));
    T = normalize(mul(float2(duv1.x, duv2.x), inverseM));
    B = normalize(mul(float2(duv1.y, duv2.y), inverseM));
    return float3x3(T, B, N);
}

float GetAttenForLight(float4 lightAtten, int lightNum)
{
#if (NUM_LIGHTS > 1)
    if (lightNum == 1) return lightAtten.y;
#endif

#if (NUM_LIGHTS > 2)
    if (lightNum == 2) return lightAtten.z;
#endif

#if (NUM_LIGHTS > 3)
    if (lightNum == 3) return lightAtten.w;
#endif

    return lightAtten.x;
}

// Models take ambient from the cube; the lightmapped path is gone with the
// LIGHTMAPPED combo.
float3 ambientLookup(float3 normal, float3 EnvAmbientCube[6])
{
    return PixelShaderAmbientLight(normal, EnvAmbientCube);
}

// Create an ambient cube from the envmap
void setupEnvMapAmbientCube(out float3 EnvAmbientCube[6], sampler EnvmapSampler)
{
    float4 directionPosX = { 1, 0, 0, 12 }; float4 directionNegX = {-1, 0, 0, 12 };
    float4 directionPosY = { 0, 1, 0, 12 }; float4 directionNegY = { 0,-1, 0, 12 };
    float4 directionPosZ = { 0, 0, 1, 12 }; float4 directionNegZ = { 0, 0,-1, 12 };
    EnvAmbientCube[0] = ENV_MAP_SCALE * texCUBElod(EnvmapSampler, directionPosX).rgb;
    EnvAmbientCube[1] = ENV_MAP_SCALE * texCUBElod(EnvmapSampler, directionNegX).rgb;
    EnvAmbientCube[2] = ENV_MAP_SCALE * texCUBElod(EnvmapSampler, directionPosY).rgb;
    EnvAmbientCube[3] = ENV_MAP_SCALE * texCUBElod(EnvmapSampler, directionNegY).rgb;
    EnvAmbientCube[4] = ENV_MAP_SCALE * texCUBElod(EnvmapSampler, directionPosZ).rgb;
    EnvAmbientCube[5] = ENV_MAP_SCALE * texCUBElod(EnvmapSampler, directionNegZ).rgb;
}
