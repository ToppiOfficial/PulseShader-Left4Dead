#ifndef PULSE_OUTLINE_VS30_H
#define PULSE_OUTLINE_VS30_H

// Inverted-hull outline expansion, shared by the NPR and PBR vertex shaders.
// The caller declares cOutlineParams at whichever SHADER_SPECIFIC_CONST slot it
// has free and applies this to the clip-space position before writing POSITION.

// Expands in clip space along the projected normal. It never moves in depth, holds
// its width where the normal faces the camera, and outlineParams.y divides the
// projection's field-of-view scale back out so the width survives an FOV change.
float4 PulseApplyOutline(float4 projPos, float3 worldNormal, float4x4 viewProj,
	float4 outlineParams)
{
	float2 projNormal = mul(float4(normalize(worldNormal), 0.0), viewProj).xy;
	projPos.xy += projNormal * (outlineParams.x * outlineParams.y);
	return projPos;
}

#endif // PULSE_OUTLINE_VS30_H
