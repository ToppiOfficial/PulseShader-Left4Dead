#ifndef PULSE_FOG_PS3_X_H
#define PULSE_FOG_PS3_X_H

#include "common_ps_fxc.h"

float PulseCalcPixelFogFactor(int fogType, float4 fogParams, float3 eyePos,
	float3 worldPos, float projZ)
{
	// L4D2 uses Alien Swarm's radial distance and signed fog offset.
	if (fogType == PIXEL_FOG_TYPE_RANGE)
		return min(fogParams.z, saturate(fogParams.x + distance(eyePos, worldPos) * fogParams.w));
	return CalcPixelFogFactor(fogType, fogParams, eyePos.z, worldPos.z, projZ);
}

float4 PulseFinalOutput(float4 color, float fogFactor, int fogType,
	int toneMapScale, bool writeDepth, float projZ)
{
#if FLASHLIGHT
	// Additive light fades to black so the base pass supplies the fog color once.
	color.rgb = BlendPixelFog(color.rgb, fogFactor, float3(0.0, 0.0, 0.0), fogType);
	fogFactor = 0.0;
#endif
	return FinalOutput(color, fogFactor, fogType, toneMapScale, writeDepth, projZ);
}

#endif
