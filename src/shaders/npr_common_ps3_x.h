#ifndef NPR_COMMON_PS3_X_H
#define NPR_COMMON_PS3_X_H

// Shared pixel-shader helpers for the PulseNPR family.
//
// Include after the variant has declared its constants and samplers. These
// helpers only touch registers the whole family agrees on: PSREG_FOG_PARAMS,
// PSREG_EYEPOS_SPEC_EXPONENT, and the PSREG_FLASHLIGHT_* block, plus samplers
// s4-s6 under FLASHLIGHT. Everything else is the variant's to assign.
//
// A variant's own lighting model does not belong here - only the scaffolding
// every NPR shader has to repeat.

// Direct light is scaled down before the cel step so a lit surface lands inside
// the ramp instead of clipping white.
static const float NPR_LIGHT_SCALE = 0.7;
const float4 g_RenderBackface : register(c53);

float3 NPRTwoSidedNormal(float3 normal, float faceSign)
{
	return g_RenderBackface.x != 0.0 ? normal * faceSign : normal;
}

// Per-light attenuation is packed into one interpolator; the loop index has to
// be resolved statically because ps_3_0 cannot swizzle by a variable.
float NPRGetLightAtten(float4 attenuation, int lightIndex)
{
#if (NUM_LIGHTS > 1)
	if (lightIndex == 1) return attenuation.y;
#endif
#if (NUM_LIGHTS > 2)
	if (lightIndex == 2) return attenuation.z;
#endif
#if (NUM_LIGHTS > 3)
	if (lightIndex == 3) return attenuation.w;
#endif
	return attenuation.x;
}

// A hard cel edge, antialiased against its own screen-space derivative so the
// terminator holds the same thickness wherever the gradient is steep or shallow.
float NPRCelStep(float edge)
{
	return saturate(edge / max(fwidth(edge), 0.0001));
}

#if FLASHLIGHT
// Cookie, shadow, and distance falloff for the flashlight pass. Returns the
// light's colour contribution and writes the direction to it. Pixels behind the
// projector are clipped.
float3 NPRFlashlightIntensity(float3 worldPos, float3 projPos, out float3 lightDirection)
{
	float4 flashlightPosition = mul(float4(worldPos, 1.0), g_FlashlightWorldToTexture);
	clip(flashlightPosition.w);
	float3 projected = flashlightPosition.xyz / flashlightPosition.w;
	float3 delta = g_FlashlightPos.xyz - worldPos;
	lightDirection = normalize(delta);

	float distanceSquared = dot(delta, delta);
	float distanceToLight = sqrt(distanceSquared);
	float3 flashlightColor = tex2D(FlashlightSampler, projected.xy).rgb * g_FlashlightColor.rgb;

#if FLASHLIGHTSHADOWS
	float flashlightShadow;
#if FLASHLIGHTDEPTHFILTERMODE == NVIDIA_PCF_POISSON
	flashlightShadow = DoShadowNvidiaCheap(ShadowDepthSampler, float4(projected, 1.0));
#else
	flashlightShadow = DoFlashlightShadow(ShadowDepthSampler, RandRotSampler, projected, projPos,
		FLASHLIGHTDEPTHFILTERMODE, g_ShadowTweaks, true);
#endif
	float shadowAttenuated = lerp(flashlightShadow, 1.0, g_ShadowTweaks.y);
	float shadowDistanceAtten = saturate(dot(g_FlashlightAttenuationFactors.xyz,
		float3(1.0, 1.0 / distanceToLight, 1.0 / distanceSquared)));
	flashlightShadow = saturate(lerp(shadowAttenuated, flashlightShadow, shadowDistanceAtten));
	flashlightColor *= flashlightShadow;
#endif

	float endFalloff = RemapValClamped(distanceToLight, g_FlashlightAttenuationFactors.w,
		0.6 * g_FlashlightAttenuationFactors.w, 0.0, 1.0);
	return flashlightColor * endFalloff * NPR_LIGHT_SCALE;
}
#endif

// Fog and destination-alpha handling. The flashlight pass is additive, so it
// must not re-apply fog.
float4 NPRFinalOutput(float3 color, float alpha, float3 worldPos, float3 projPos)
{
	float fogFactor = 0.0;
#if !FLASHLIGHT
	fogFactor = CalcPixelFogFactor(PIXELFOGTYPE, g_FogParams, g_EyePos.z, worldPos.z, projPos.z);
#endif
	bool writeDepth = (WRITE_DEPTH_TO_DESTALPHA != 0) && (WRITEWATERFOGTODESTALPHA == 0);
#if WRITEWATERFOGTODESTALPHA && (PIXELFOGTYPE == PIXEL_FOG_TYPE_HEIGHT)
	alpha = fogFactor;
#endif
	return FinalOutput(float4(color, alpha), fogFactor, PIXELFOGTYPE, TONEMAP_SCALE_LINEAR,
		writeDepth, projPos.z);
}

#endif // NPR_COMMON_PS3_X_H
