#ifndef PULSE_OUTLINE_VS30_H
#define PULSE_OUTLINE_VS30_H

// Inverted-hull outline expansion, shared by the NPR and PBR vertex shaders.
// The caller declares cOutlineParams at whichever SHADER_SPECIFIC_CONST slot it
// has free and applies this to the clip-space position before writing POSITION.

// Expands in clip space along the projected normal. outlineParams.zw optionally
// fade camera-facing expansion while outlineParams.y compensates for FOV.
float4 PulseApplyOutline(float4 projPos, float3 worldPos, float3 worldNormal, float4x4 viewProj,
	float4 outlineParams)
{
	float3 normal = normalize(worldNormal);
	float2 projNormal = mul(float4(normal, 0.0), viewProj).xy;
	float facing = abs(dot(normal, normalize(cEyePos - worldPos)));
	float viewFade = 1.0 - smoothstep(0.0, outlineParams.z, facing);
	projPos.xy += projNormal * (outlineParams.x * outlineParams.y
		* lerp(1.0, viewFade, outlineParams.w));
	return projPos;
}

#endif // PULSE_OUTLINE_VS30_H
