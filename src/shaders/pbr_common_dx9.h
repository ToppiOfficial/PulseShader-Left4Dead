#ifndef PBR_COMMON_DX9_H
#define PBR_COMMON_DX9_H

// Shared plumbing for the PulsePBR shader family.
//
// Variants are siblings, not subclasses of each other: each one opens its own
// BEGIN_PBR_SHADER namespace and so owns its params, combos, and constant
// registers outright. Only engine-facing state that no variant has a reason to
// change lives here. Adding a variant must never require editing this file.

#include "BaseVSShader.h"
#include "log.h"
#include "cpp_shader_constant_register_map.h"
#include "pulse_outline_dx9.h"
#include "pulse_shader_convars.h"

// Samplers every variant shares. s8 and s9 are left to the variant, which is
// where feature maps differ (the stocking ramp and face shadow map).
const Sampler_t PBR_SAMPLER_BASETEXTURE   = SHADER_SAMPLER0;
const Sampler_t PBR_SAMPLER_NORMAL        = SHADER_SAMPLER1;
const Sampler_t PBR_SAMPLER_ENVMAP        = SHADER_SAMPLER2;
const Sampler_t PBR_SAMPLER_DETAIL        = SHADER_SAMPLER3;
const Sampler_t PBR_SAMPLER_SHADOWDEPTH   = SHADER_SAMPLER4;
const Sampler_t PBR_SAMPLER_RANDOMROTATION = SHADER_SAMPLER5;
const Sampler_t PBR_SAMPLER_FLASHLIGHT    = SHADER_SAMPLER6;
const Sampler_t PBR_SAMPLER_LIGHTWARP     = SHADER_SAMPLER7;
const Sampler_t PBR_SAMPLER_MRAO          = SHADER_SAMPLER10;
const Sampler_t PBR_SAMPLER_EMISSIVE      = SHADER_SAMPLER11;
const Sampler_t PBR_SAMPLER_SPECULAR      = SHADER_SAMPLER12;

// c10.rgb = detail tint, c10.a = blend factor. c26.x = detail blend mode,
// c26.yzw = flat metalness/roughness/AO. Both are family-wide.
#define PBR_PSREG_DETAIL_TINT 10
#define PBR_PSREG_MISC        26

// c27.xyz = OpenPBR specular IOR, specular weight, base diffuse roughness.
// c33.rgb = the F82 metal tint. Above the shared map, which ends at c31 and
// whose low slots variants already repurpose; c32 is cScreenSize on the Alien
// Swarm branch. Family-wide, like the two above.
#define PBR_PSREG_OPENPBR       27
#define PBR_PSREG_SPECULAR_TINT 33

class CPBRShaderBase : public CBaseVSShader
{
protected:
	void PBRSetFlashlightTexturePath(IMaterialVar **params)
	{
		if (g_pHardwareConfig->SupportsBorderColor())
			params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight_border");
		else
			params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight001");
	}

	void PBRSetModelFlags(IMaterialVar **params)
	{
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_HW_SKINNING);             // Required for skinning
		SET_FLAGS2(MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL);         // Required for dynamic lighting
		SET_FLAGS2(MATERIAL_VAR2_NEEDS_TANGENT_SPACES);             // Required for dynamic lighting
		SET_FLAGS2(MATERIAL_VAR2_LIGHTING_VERTEX_LIT);              // Required for dynamic lighting
		SET_FLAGS2(MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS);   // Required for ambient cube
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);              // Required for flashlight
		SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);                   // Required for flashlight
	}

	int PBRShadowFilterMode(bool flashlight) const
	{
		return flashlight ? g_pHardwareConfig->GetShadowFilterMode() : 0;
	}

	// The always-on samplers. Feature maps stay with the variant that owns them.
	void PBRSnapshotCoreSamplers(IShaderShadow *pShaderShadow)
	{
		pShaderShadow->EnableTexture(PBR_SAMPLER_BASETEXTURE, true);    // Basecolor texture
		pShaderShadow->EnableSRGBRead(PBR_SAMPLER_BASETEXTURE, true);   // Basecolor is sRGB
		pShaderShadow->EnableTexture(PBR_SAMPLER_EMISSIVE, true);       // Emission texture
		pShaderShadow->EnableSRGBRead(PBR_SAMPLER_EMISSIVE, true);      // Emission is sRGB
		pShaderShadow->EnableTexture(PBR_SAMPLER_MRAO, true);           // MRAO texture
		pShaderShadow->EnableSRGBRead(PBR_SAMPLER_MRAO, false);         // MRAO isn't sRGB
		pShaderShadow->EnableTexture(PBR_SAMPLER_NORMAL, true);         // Normal texture
		pShaderShadow->EnableSRGBRead(PBR_SAMPLER_NORMAL, false);       // Normals aren't sRGB
		pShaderShadow->EnableTexture(PBR_SAMPLER_SPECULAR, true);       // Specular F0 texture
		pShaderShadow->EnableSRGBRead(PBR_SAMPLER_SPECULAR, true);      // Specular F0 is sRGB
	}

	void PBRSnapshotFlashlightSamplers(IShaderShadow *pShaderShadow, int shadowFilterMode)
	{
		pShaderShadow->EnableTexture(PBR_SAMPLER_SHADOWDEPTH, true);        // Shadow depth map
		pShaderShadow->SetShadowDepthFiltering(PBR_SAMPLER_SHADOWDEPTH);
		pShaderShadow->EnableSRGBRead(PBR_SAMPLER_SHADOWDEPTH, false);
		if (shadowFilterMode != 0)
			pShaderShadow->EnableTexture(PBR_SAMPLER_RANDOMROTATION, true); // Noise map
		pShaderShadow->EnableTexture(PBR_SAMPLER_FLASHLIGHT, true);         // Flashlight cookie
		pShaderShadow->EnableSRGBRead(PBR_SAMPLER_FLASHLIGHT, true);
	}

	// Per-instance lighting is baked into a command buffer at snapshot time and
	// replayed by the engine per instance - the PI_ helpers write into that
	// buffer, so they are invalid outside this bracket.
	void PBRWriteLightingCommandBuffer(IMaterialVar **params)
	{
		float color2[3];
		params[COLOR2]->GetVecValue(color2, 3);
		params[COLOR2]->SetVecValue(1.0f, 1.0f, 1.0f);
		PI_BeginCommandBuffer();
		PI_SetModulationPixelShaderDynamicState_LinearColorSpace( 1 );
		PI_SetPixelShaderAmbientLightCube( PSREG_AMBIENT_CUBE );
		PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );
		PI_EndCommandBuffer();
		params[COLOR2]->SetVecValue(color2, 3);
	}

	// Binds the cookie, shadow depth, and noise maps and uploads the flashlight
	// constants. Returns whether the depth-shadow combo should be enabled.
	bool PBRBindFlashlightState(IShaderDynamicAPI *pShaderAPI)
	{
		VMatrix worldToTexture;
		ITexture *pFlashlightDepthTexture = NULL;
		const FlashlightState_t &state = pShaderAPI->GetFlashlightStateEx(worldToTexture, &pFlashlightDepthTexture);
		bool bFlashlightShadows = state.m_bEnableShadows && pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture();

		SetFlashLightColorFromState(state, pShaderAPI, PSREG_FLASHLIGHT_COLOR);

		if (state.m_pSpotlightTexture)
			BindTexture(PBR_SAMPLER_FLASHLIGHT, state.m_pSpotlightTexture, state.m_nSpotlightTextureFrame);
		else
			pShaderAPI->BindStandardTexture(PBR_SAMPLER_FLASHLIGHT, TEXTURE_WHITE);

		if (bFlashlightShadows)
		{
			BindTexture(PBR_SAMPLER_SHADOWDEPTH, pFlashlightDepthTexture, 0);
			if (g_pHardwareConfig->GetShadowFilterMode() != 0)
				pShaderAPI->BindStandardTexture(PBR_SAMPLER_RANDOMROTATION, TEXTURE_SHADOW_NOISE_2D);
		}

		float atten[4] = {
			state.m_fConstantAtten,
			state.m_fLinearAtten,
			state.m_fQuadraticAtten,
			state.m_FarZ
		};
		pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_ATTENUATION, atten, 1);

		float pos[4] = {
			state.m_vecLightOrigin[0],
			state.m_vecLightOrigin[1],
			state.m_vecLightOrigin[2],
			0.0f
		};
		pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1);
		pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, worldToTexture.Base(), 4);

		float tweaks[4];
		tweaks[0] = ShadowFilterFromState(state);
		tweaks[1] = ShadowAttenFromState(state);
		HashShadow2DJitter(state.m_flShadowJitterSeed, &tweaks[2], &tweaks[3]);
		pShaderAPI->SetPixelShaderConstant(PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1);

		return bFlashlightShadows;
	}

	// Camera position, with the envmap's usable mip count packed into w. Derived
	// from the cubemap width, clamped for very high and low resolution maps.
	void PBRSetEyePositionAndEnvMapLOD(IShaderDynamicAPI *pShaderAPI, IMaterialVar *pEnvMapVar)
	{
		float vEyePos_SpecExponent[4];
		pShaderAPI->GetWorldSpaceCameraPosition(vEyePos_SpecExponent);

		int iEnvMapLOD = 6;
		ITexture *envTexture = pEnvMapVar ? pEnvMapVar->GetTextureValue() : NULL;
		if (envTexture)
		{
			int width = envTexture->GetMappingWidth();
			int mips = 0;
			while (width >>= 1)
				++mips;
			iEnvMapLOD = mips;
		}

		if (iEnvMapLOD > 12)
			iEnvMapLOD = 12;
		if (iEnvMapLOD < 4)
			iEnvMapLOD = 4;

		vEyePos_SpecExponent[3] = iEnvMapLOD;
		pShaderAPI->SetPixelShaderConstant(PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1);
	}

	// c10.rgb = tint, c10.a = blend factor.
	void PBRSetDetailTint(IShaderDynamicAPI *pShaderAPI, IMaterialVar **params,
		int detailTintVar, int detailBlendFactorVar)
	{
		float detailConst[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		if (detailTintVar != -1)
			params[detailTintVar]->GetVecValue(detailConst, 3);
		detailConst[3] = (detailBlendFactorVar != -1)
					   ? params[detailBlendFactorVar]->GetFloatValue() : 1.0f;
		pShaderAPI->SetPixelShaderConstant(PBR_PSREG_DETAIL_TINT, detailConst, 1);
	}

	// The OpenPBR scalars. Defaults reproduce the pre-OpenPBR look: IOR 1.5 is
	// F0 0.04, diffuse roughness 0 is Lambert, and a white F82 tint collapses
	// the metal Fresnel back to plain Schlick.
	void PBRSetOpenPBRParams(IShaderDynamicAPI *pShaderAPI, IMaterialVar **params,
		int specularIorVar, int specularWeightVar, int baseDiffuseRoughnessVar,
		int specularTintVar, bool renderBackface)
	{
		float openpbrConst[4] = { 1.5f, 1.0f, 0.0f, renderBackface ? 1.0f : 0.0f };
		if (specularIorVar != -1)
			openpbrConst[0] = params[specularIorVar]->GetFloatValue();
		if (specularWeightVar != -1)
			openpbrConst[1] = params[specularWeightVar]->GetFloatValue();
		if (baseDiffuseRoughnessVar != -1)
			openpbrConst[2] = params[baseDiffuseRoughnessVar]->GetFloatValue();
		pShaderAPI->SetPixelShaderConstant(PBR_PSREG_OPENPBR, openpbrConst, 1);

		// The tint multiplies a reflectance, so it stays linear.
		float tintConst[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		if (specularTintVar != -1)
			params[specularTintVar]->GetVecValue(tintConst, 3);
		pShaderAPI->SetPixelShaderConstant(PBR_PSREG_SPECULAR_TINT, tintConst, 1);
	}

	static int PBRFogIndex(IShaderDynamicAPI *pShaderAPI)
	{
		return (pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z) ? 1 : 0;
	}

	// mat_fullbright 2 is diffuse-lighting-only; mat_specular 0 kills envmap
	// reflections. Both replace a bound texture rather than taking a combo.
	void PBRApplyDebugOverrides(IShaderDynamicAPI *pShaderAPI, IMaterialVar **params)
	{
		if (mat_fullbright.GetInt() == 2 && !IS_FLAG_SET(MATERIAL_VAR_NO_DEBUG_OVERRIDE))
			pShaderAPI->BindStandardTexture(PBR_SAMPLER_BASETEXTURE, TEXTURE_GREY);

		if (!mat_specular.GetBool())
			pShaderAPI->BindStandardTexture(PBR_SAMPLER_ENVMAP, TEXTURE_BLACK);
	}
};

// BEGIN_VS_SHADER passes CBaseVSShader as the base class; this passes ours, so
// every variant gets the helpers above while keeping its own param namespace.
#define BEGIN_PBR_SHADER(_name, _help) __BEGIN_SHADER_INTERNAL( CPBRShaderBase, _name, _help, 0 )

#endif // PBR_COMMON_DX9_H
