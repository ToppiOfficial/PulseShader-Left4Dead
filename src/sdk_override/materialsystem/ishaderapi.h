// Shadows Alien Swarm's public/materialsystem/ishaderapi.h.
//
// Swarm declares IShaderInit's loaders with two arguments; L4D2's take a third
// (nAdditionalCreationFlags), as CS:GO's do. These are __thiscall, so the
// CALLEE cleans the stack: calling a 3-argument implementation with 2 pushed
// makes it pop 12 bytes instead of 8, corrupting the return address. That
// showed up as an access violation executing address 0x9 immediately after
// CBaseShader::LoadTexture, with every vtable pointer verified good.
//
// This directory is placed ahead of the SDK on the include path; it holds only
// declarations where L4D2 provably differs from the published headers.
#ifndef ISHADERAPI_MS_H
#define ISHADERAPI_MS_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterialsystem.h"
#include "shaderapi/ishaderdynamic.h"
#include "shaderapi/ishadershadow.h"

class IMaterialVar;

abstract_class IShaderInit
{
public:
	// Loads up a texture
	virtual void LoadTexture( IMaterialVar *pTextureVar, const char *pTextureGroupName, int nAdditionalCreationFlags = 0 ) = 0;
	// Confirmed by test: LoadTexture takes the extra flags argument on L4D2,
	// LoadBumpMap does not. Verified one call at a time - a wrong count here
	// corrupts the stack under __thiscall rather than failing cleanly.
	virtual void LoadBumpMap( IMaterialVar *pTextureVar, const char *pTextureGroupName ) = 0;
	virtual void LoadCubeMap( IMaterialVar **ppParams, IMaterialVar *pTextureVar, int nAdditionalCreationFlags = 0 ) = 0;
};

#endif // ISHADERAPI_MS_H
