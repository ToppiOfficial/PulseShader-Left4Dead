#ifndef PULSE_OUTLINE_DX9_H
#define PULSE_OUTLINE_DX9_H

// Inverted-hull outline, shared by the NPR and PBR families.
//
// The outline is a second pass drawing back faces with the hull expanded along
// the projected normal. Nothing here touches CBaseVSShader's protected members,
// so both family bases can call it.

#include "BaseVSShader.h"

// [0][0] carries the projection's horizontal field-of-view scale; the outline
// divides it out so its width does not track the FOV.
inline void PulseSetOutlineConstants(IShaderDynamicAPI *pShaderAPI, int vertexConstant,
	float outlineWidth)
{
	VMatrix projMatrix;
	pShaderAPI->GetMatrix(MATERIAL_PROJECTION, projMatrix.Base());
	float projScale = projMatrix[0][0];
	float outlineParams[4] = { outlineWidth,
		fabsf(projScale) > 1e-6f ? 1.0f / projScale : 1.0f, 0.0f, 0.0f };
	pShaderAPI->SetVertexShaderConstant(vertexConstant, outlineParams);
}

#endif // PULSE_OUTLINE_DX9_H
