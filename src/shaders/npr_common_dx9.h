#ifndef NPR_COMMON_DX9_H
#define NPR_COMMON_DX9_H

// Shared plumbing for the PulseNPR shader family.
//
// Variants are siblings, not subclasses of each other: each one opens its own
// BEGIN_NPR_SHADER namespace and so owns its params, combos, and constant
// registers outright. Only engine-facing state that no variant has a reason to
// change lives here. Adding a variant must never require editing this file.

#include "BaseVSShader.h"
#include "cpp_shader_constant_register_map.h"
#include "pulse_outline_dx9.h"
#include "pulse_shader_convars.h"

// Samplers every variant shares. Slots s1-s3 are left to the variant, which is
// where map layouts differ (Umamusume's AOHA and RSRFL, MMD's toon ramp).
const Sampler_t NPR_SAMPLER_BASE        = SHADER_SAMPLER0;
const Sampler_t NPR_SAMPLER_FLASHLIGHT  = SHADER_SAMPLER4;
const Sampler_t NPR_SAMPLER_SHADOWDEPTH = SHADER_SAMPLER5;
const Sampler_t NPR_SAMPLER_RANDROT     = SHADER_SAMPLER6;
const Sampler_t NPR_SAMPLER_DETAIL      = SHADER_SAMPLER7;
const Sampler_t NPR_SAMPLER_REFLECTION  = SHADER_SAMPLER8;

// c47 is the one detail constant the whole family agrees on. The rest of the
// constant map is per-variant.
#define NPR_PSREG_DETAIL_TINT 47

class CNPRShaderBase : public CBaseVSShader
{
protected:
	// Blend, depth, and cull state for one pass of the outline/main loop. The
	// outline pass draws back faces, so it inverts culling.
	void NPRSnapshotPassState(IShaderShadow *pShaderShadow, IMaterialVar **params,
		bool outline, bool flashlight, bool translucent, bool alphaTest, float alphaTestRef)
	{
		pShaderShadow->EnableCulling(!outline && !IS_FLAG_SET(MATERIAL_VAR_NOCULL));
		pShaderShadow->EnableAlphaTest(alphaTest);
		if (alphaTest)
			pShaderShadow->AlphaFunc(SHADER_ALPHAFUNC_GEQUAL, alphaTestRef);
		pShaderShadow->EnableDepthWrites(!flashlight && !translucent);
		pShaderShadow->EnableBlending(flashlight || translucent);
		if (flashlight)
			pShaderShadow->BlendFunc(SHADER_BLEND_ONE, SHADER_BLEND_ONE);
		else if (translucent)
			pShaderShadow->BlendFunc(SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA);
		pShaderShadow->EnableAlphaWrites(!flashlight && !translucent);
	}

	int NPRShadowFilterMode(bool flashlight) const
	{
		return flashlight ? g_pHardwareConfig->GetShadowFilterMode() : 0;
	}

	void NPRSnapshotFlashlightSamplers(IShaderShadow *pShaderShadow, int shadowFilterMode)
	{
		pShaderShadow->EnableTexture(NPR_SAMPLER_FLASHLIGHT, true);
		pShaderShadow->EnableSRGBRead(NPR_SAMPLER_FLASHLIGHT, true);
		pShaderShadow->EnableTexture(NPR_SAMPLER_SHADOWDEPTH, true);
		pShaderShadow->SetShadowDepthFiltering(NPR_SAMPLER_SHADOWDEPTH);
		if (shadowFilterMode != 0)
			pShaderShadow->EnableTexture(NPR_SAMPLER_RANDROT, true);
	}

	// Per-instance lighting is baked into a command buffer at snapshot time and
	// replayed per instance, so the PI_ helpers are only valid inside this
	// bracket.
	void NPRWriteLightingCommandBuffer()
	{
		PI_BeginCommandBuffer();
		PI_SetModulationPixelShaderDynamicState_LinearColorSpace(1);
		PI_SetPixelShaderAmbientLightCube(PSREG_AMBIENT_CUBE);
		PI_SetPixelShaderLocalLighting(PSREG_LIGHT_INFO_ARRAY);
		PI_EndCommandBuffer();
	}

	// Binds the cookie, shadow depth, and noise maps and uploads the flashlight
	// constants. Returns whether the depth-shadow combo should be enabled.
	bool NPRBindFlashlightState(IShaderDynamicAPI *pShaderAPI)
	{
		VMatrix worldToTexture;
		ITexture *depthTexture = NULL;
		const FlashlightState_t &state = pShaderAPI->GetFlashlightStateEx(worldToTexture, &depthTexture);
		bool flashlightShadows = state.m_bEnableShadows && depthTexture && g_pConfig->ShadowDepthTexture();

		SetFlashLightColorFromState(state, pShaderAPI, PSREG_FLASHLIGHT_COLOR);

		if (state.m_pSpotlightTexture)
			BindTexture(NPR_SAMPLER_FLASHLIGHT, state.m_pSpotlightTexture, state.m_nSpotlightTextureFrame);
		else
			pShaderAPI->BindStandardTexture(NPR_SAMPLER_FLASHLIGHT, TEXTURE_WHITE);

		if (flashlightShadows)
		{
			BindTexture(NPR_SAMPLER_SHADOWDEPTH, depthTexture, 0);
			if (g_pHardwareConfig->GetShadowFilterMode() != 0)
				pShaderAPI->BindStandardTexture(NPR_SAMPLER_RANDROT, TEXTURE_SHADOW_NOISE_2D);
		}

		float attenuation[4] = { state.m_fConstantAtten, state.m_fLinearAtten,
			state.m_fQuadraticAtten, state.m_FarZ };
		pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_ATTENUATION, attenuation);
		float position[4] = { state.m_vecLightOrigin[0], state.m_vecLightOrigin[1],
			state.m_vecLightOrigin[2], 0.0f };
		pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_POSITION_RIM_BOOST, position);
		pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, worldToTexture.Base(), 4);

		float tweaks[4];
		tweaks[0] = ShadowFilterFromState(state);
		tweaks[1] = ShadowAttenFromState(state);
		HashShadow2DJitter(state.m_flShadowJitterSeed, &tweaks[2], &tweaks[3]);
		pShaderAPI->SetPixelShaderConstant(PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks);

		return flashlightShadows;
	}

	void NPRSetOutlineConstants(IShaderDynamicAPI *pShaderAPI, float outlineWidth,
		float outlineAngle)
	{
		PulseSetOutlineConstants(pShaderAPI, VERTEX_SHADER_SHADER_SPECIFIC_CONST_2,
			outlineWidth, outlineAngle);
	}

	// Tint is gamma-corrected to match the sRGB base map; alpha carries the
	// blend strength.
	void NPRSetDetailTint(IShaderDynamicAPI *pShaderAPI, IMaterialVar **params,
		int detailTintVar, int detailBlendFactorVar)
	{
		float detailColor[4] = { 1.0f, 1.0f, 1.0f, params[detailBlendFactorVar]->GetFloatValue() };
		params[detailTintVar]->GetVecValue(detailColor, 3);
		detailColor[0] = GammaToLinear(detailColor[0]);
		detailColor[1] = GammaToLinear(detailColor[1]);
		detailColor[2] = GammaToLinear(detailColor[2]);
		pShaderAPI->SetPixelShaderConstant(NPR_PSREG_DETAIL_TINT, detailColor);
	}

	static int NPRFogIndex(IShaderDynamicAPI *pShaderAPI)
	{
		return pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ? 1 : 0;
	}

	// mat_fullbright 2 is diffuse-lighting-only: swap the albedo for flat grey.
	void NPRApplyLightingOnly(IShaderDynamicAPI *pShaderAPI, IMaterialVar **params)
	{
		if (mat_fullbright.GetInt() == 2 && !IS_FLAG_SET(MATERIAL_VAR_NO_DEBUG_OVERRIDE))
			pShaderAPI->BindStandardTexture(NPR_SAMPLER_BASE, TEXTURE_GREY);
	}

	void NPRSetModelFlags(IMaterialVar **params)
	{
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_HW_SKINNING);
		SET_FLAGS2(MATERIAL_VAR2_LIGHTING_VERTEX_LIT);
		SET_FLAGS2(MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS);
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);
		SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);
	}

	void NPRSetFlashlightTexturePath(IMaterialVar **params)
	{
		if (g_pHardwareConfig->SupportsBorderColor())
			params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight_border");
		else
			params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight001");
	}
};

// BEGIN_VS_SHADER passes CBaseVSShader as the base class; this passes ours, so
// every variant gets the helpers above while keeping its own param namespace.
#define BEGIN_NPR_SHADER(_name, _help) __BEGIN_SHADER_INTERNAL( CNPRShaderBase, _name, _help, 0 )

#endif // NPR_COMMON_DX9_H
