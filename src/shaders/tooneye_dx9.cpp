//==================================================================================================
//
// PulseToonEye: a toon-styled fork of Valve's eyerefract. It keeps the eyeball
// system intact - the engine writes $eyeorigin/$irisu/$irisv per eye each frame and
// the shader projects the base map onto those planes so the iris tracks gaze - but
// drops the cornea/parallax/AO/raytrace machinery for a flat anime eye with base<->
// shade blending, iris/eyewhite tinting, a specular shine map, cube reflection on the
// iris, selfillum, and the shared eyelid-over-hair overlay.
//
// The base map's alpha is the iris/eyewhite mask: ~0 = iris (non-opaque), ~1 = eyewhite.
//
//==================================================================================================

#include "npr_common_dx9.h"

#include "pulse_tooneye_vs30.inc"
#include "pulse_tooneye_ps30.inc"

// Iris reflection cube map.
const Sampler_t SAMPLER_ENVMAP = SHADER_SAMPLER9;

struct ToonEye_Vars_t
{
	ToonEye_Vars_t()
	{
		memset(this, 0xFF, sizeof(*this));
	}

	int baseTexture;
	int baseTextureFrame;
	int baseColor;
	int baseColor2;
	int alpha;
	int shadeTexture;
	int specularTexture;
	int specularTextureFrame;
	int lightWarpTexture;
	int envMap;
	int envMapTint;
	int envMapScale;
	int irisColor;
	int eyeWhiteColor;
	int irisScale;
	int irisMaskReference;
	int irisMaskSoftness;
	int selfIllum;
	int selfIllumMask;
	int selfIllumMaskFrame;
	int selfIllumTint;
	int eyelid;
	int eyelidBlend;
	int specBrightness;
	int specSaturation;
	int fallbackBrightness;
	int eyeOrigin;
	int irisU;
	int irisV;
	int flashlightTexture;
};

BEGIN_NPR_SHADER(PulseToonEye, "Toon eyeball shader: eyerefract projection with flat anime shading")
	BEGIN_SHADER_PARAMS;
		SHADER_PARAM(BASESHADETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Shaded base texture, blended toward as the eye turns into shadow; defaults to $basetexture");
		SHADER_PARAM(SPECULARTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Eyeball-UV shine mask (red), lit like a highlight when light hits it");
		SHADER_PARAM(SPECULARTEXTUREFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $speculartexture");
		SHADER_PARAM(LIGHTWARPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "1D ramp remapping the diffuse falloff");
		SHADER_PARAM(ENVMAP, SHADER_PARAM_TYPE_ENVMAP, "", "Cubemap (env_cubemap or a baked cube) reflected on the iris only");
		SHADER_PARAM(ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Iris reflection tint");
		SHADER_PARAM(ENVMAPSCALE, SHADER_PARAM_TYPE_FLOAT, "1", "Linear brightness scale for the reflection, unbounded");
		SHADER_PARAM(IRISCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Tints the iris (low base alpha); the eyewhite is left untinted");
		SHADER_PARAM(EYEWHITECOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Tints the eyewhite (opaque base alpha); the iris is left untinted");
		SHADER_PARAM(IRISSCALE, SHADER_PARAM_TYPE_FLOAT, "1", "Scales the iris and its specular in UV space; 1 is authored size. Proxy-drivable post control on top of the mdl eyeball size");
		SHADER_PARAM(IRISMASKREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0.5", "Base-alpha value where the iris/eyewhite tint boundary sits, like $alphatestreference");
		SHADER_PARAM(IRISMASKSOFTNESS, SHADER_PARAM_TYPE_FLOAT, "0", "Extra width added to the screen-space anti-aliased boundary; 0 is a crisp ~2px edge");
		SHADER_PARAM(SELFILLUM, SHADER_PARAM_TYPE_BOOL, "0", "Enable selfillum from $selfillummask");
		SHADER_PARAM(SELFILLUMMASK, SHADER_PARAM_TYPE_TEXTURE, "", "Selfillum mask (red); masked pixels read as unlit eye color");
		SHADER_PARAM(SELFILLUMMASKFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $selfillummask");
		SHADER_PARAM(SELFILLUMTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Selfillum tint");
		SHADER_PARAM(EYELID, SHADER_PARAM_TYPE_BOOL, "0", "Draw over the front hair: the iris reads through the bangs");
		SHADER_PARAM(EYELIDBLEND, SHADER_PARAM_TYPE_FLOAT, "1", "Opacity of the iris-over-hair overlay; not usable when the hair is a PulseUmamusume material");
		SHADER_PARAM(SPECULARBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "1", "Additive shine brightness for $speculartexture");
		SHADER_PARAM(SPECULARSATURATION, SHADER_PARAM_TYPE_FLOAT, "1", "Saturation of the shine colour; 1 unchanged, 0 greyscale, above 1 more saturated");
		SHADER_PARAM(FALLBACKBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "0", "Ambient floor when there are no local lights");

		// Written by the studiorender for eyeball meshes; declared so the engine can set
		// them. $eyeorigin/$irisu/$irisv drive the projection; the rest are kept as no-ops
		// so existing eye VMTs do not warn.
		SHADER_PARAM(EYEORIGIN, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "Eyeball origin (engine-written)");
		SHADER_PARAM(IRISU, SHADER_PARAM_TYPE_VEC4, "[0 1 0 0]", "Iris U projection plane (engine-written)");
		SHADER_PARAM(IRISV, SHADER_PARAM_TYPE_VEC4, "[0 0 1 0]", "Iris V projection plane (engine-written)");
		SHADER_PARAM(DILATION, SHADER_PARAM_TYPE_FLOAT, "0", "Ignored; kept for eye VMT compatibility");
		SHADER_PARAM(EYEBALLRADIUS, SHADER_PARAM_TYPE_FLOAT, "0", "Ignored; kept for eye VMT compatibility");
	END_SHADER_PARAMS;

	void SetupVars(ToonEye_Vars_t &info)
	{
		info.baseTexture = BASETEXTURE;
		info.baseTextureFrame = FRAME;
		info.baseColor = COLOR;
		info.baseColor2 = COLOR2;
		info.alpha = ALPHA;
		info.shadeTexture = BASESHADETEXTURE;
		info.specularTexture = SPECULARTEXTURE;
		info.specularTextureFrame = SPECULARTEXTUREFRAME;
		info.lightWarpTexture = LIGHTWARPTEXTURE;
		info.envMap = ENVMAP;
		info.envMapTint = ENVMAPTINT;
		info.envMapScale = ENVMAPSCALE;
		info.irisColor = IRISCOLOR;
		info.eyeWhiteColor = EYEWHITECOLOR;
		info.irisScale = IRISSCALE;
		info.irisMaskReference = IRISMASKREFERENCE;
		info.irisMaskSoftness = IRISMASKSOFTNESS;
		info.selfIllum = SELFILLUM;
		info.selfIllumMask = SELFILLUMMASK;
		info.selfIllumMaskFrame = SELFILLUMMASKFRAME;
		info.selfIllumTint = SELFILLUMTINT;
		info.eyelid = EYELID;
		info.eyelidBlend = EYELIDBLEND;
		info.specBrightness = SPECULARBRIGHTNESS;
		info.specSaturation = SPECULARSATURATION;
		info.fallbackBrightness = FALLBACKBRIGHTNESS;
		info.eyeOrigin = EYEORIGIN;
		info.irisU = IRISU;
		info.irisV = IRISV;
		info.flashlightTexture = FLASHLIGHTTEXTURE;
	}

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS(MATERIAL_VAR_MODEL);
		// Toon eyes read best with a soft terminator, and decals on an eyeball look wrong.
		SET_FLAGS(MATERIAL_VAR_HALFLAMBERT);
		SET_FLAGS(MATERIAL_VAR_SUPPRESS_DECALS);

		SET_PARAM_FLOAT_IF_NOT_DEFINED(EYELIDBLEND, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARBRIGHTNESS, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARSATURATION, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(ENVMAPSCALE, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(IRISSCALE, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(FALLBACKBRIGHTNESS, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(IRISMASKREFERENCE, 0.5f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(IRISMASKSOFTNESS, 0.0f);

		NPRSetFlashlightTexturePath(params);
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		ToonEye_Vars_t info;
		SetupVars(info);
		LoadTexture(info.flashlightTexture);
		if (params[info.baseTexture]->IsDefined()) LoadTexture(info.baseTexture);
		if (params[info.shadeTexture]->IsDefined()) LoadTexture(info.shadeTexture);
		if (params[info.specularTexture]->IsDefined()) LoadTexture(info.specularTexture);
		if (params[info.lightWarpTexture]->IsDefined()) LoadTexture(info.lightWarpTexture);
		if (params[info.selfIllumMask]->IsDefined()) LoadTexture(info.selfIllumMask);
		if (params[info.envMap]->IsDefined()) LoadCubeMap(info.envMap);

		NPRSetModelFlags(params);
	}

	SHADER_DRAW
	{
		ToonEye_Vars_t info;
		SetupVars(info);

		bool hasBase = params[info.baseTexture]->IsTexture();
		bool hasShade = params[info.shadeTexture]->IsTexture();
		bool hasSpecular = params[info.specularTexture]->IsTexture();
		bool hasLightWarp = params[info.lightWarpTexture]->IsTexture();
		bool selfIllum = params[info.selfIllum]->GetIntValue() != 0;
		bool hasSelfIllumMask = params[info.selfIllumMask]->IsTexture();
		bool hasEnvMap = params[info.envMap]->IsTexture();
		bool flashlight = UsingFlashlight(params);
		bool eyelid = params[info.eyelid]->GetIntValue() != 0;

		// The overlay never runs under the flashlight (its additive pass carries no alpha).
		int passes = (eyelid && !flashlight) ? 2 : 1;
		for (int pass = 0; pass < passes; ++pass)
		{
			bool overlay = (pass == 1);
			bool envmap = hasEnvMap && !flashlight;
			int shadowFilter = NPRShadowFilterMode(flashlight);

			if (IsSnapshotting())
			{
				if (overlay)
				{
					// Iris-over-hair overlay: the VS pushes it toward the camera so it beats
					// the bangs on the depth test while still losing to nearer geometry.
					// Blended, no depth or alpha writes.
					pShaderShadow->EnableCulling(!IS_FLAG_SET(MATERIAL_VAR_NOCULL));
					pShaderShadow->EnableDepthWrites(false);
					pShaderShadow->EnableBlending(true);
					pShaderShadow->BlendFunc(SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA);
					pShaderShadow->EnableAlphaWrites(false);
				}
				else
				{
					NPRSnapshotPassState(pShaderShadow, params, false, false,
						flashlight, false, false, 0.0f);
				}

				pShaderShadow->EnableTexture(SHADER_SAMPLER0, true); // base
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0, true);
				pShaderShadow->EnableTexture(SHADER_SAMPLER1, true); // shade
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER1, true);
				pShaderShadow->EnableTexture(SHADER_SAMPLER2, true); // specular shine (color)
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER2, true);
				pShaderShadow->EnableTexture(SHADER_SAMPLER8, true); // selfillum mask
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER8, false);
				if (hasLightWarp)
				{
					pShaderShadow->EnableTexture(SHADER_SAMPLER3, true);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER3, false);
				}
				if (envmap)
				{
					pShaderShadow->EnableTexture(SAMPLER_ENVMAP, true);
					if (g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE)
						pShaderShadow->EnableSRGBRead(SAMPLER_ENVMAP, true);
				}
				if (flashlight)
					NPRSnapshotFlashlightSamplers(pShaderShadow, shadowFilter);

				pShaderShadow->EnableSRGBWrite(true);
				pShaderShadow->VertexShaderVertexFormat(
					VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED, 1, 0, 0);

				DECLARE_STATIC_VERTEX_SHADER(pulse_tooneye_vs30);
				SET_STATIC_VERTEX_SHADER_COMBO(EYELID, overlay);
				SET_STATIC_VERTEX_SHADER(pulse_tooneye_vs30);

				DECLARE_STATIC_PIXEL_SHADER(pulse_tooneye_ps30);
				SET_STATIC_PIXEL_SHADER_COMBO(BASESHADE, hasShade);
				SET_STATIC_PIXEL_SHADER_COMBO(LIGHTWARP, hasLightWarp);
				SET_STATIC_PIXEL_SHADER_COMBO(ENVMAP, envmap);
				SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, flashlight);
				SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, shadowFilter);
				SET_STATIC_PIXEL_SHADER_COMBO(EYELID, overlay);
				SET_STATIC_PIXEL_SHADER(pulse_tooneye_ps30);

				DefaultFog();
				NPRWriteLightingCommandBuffer();
			}
			else
			{
				if (hasBase) BindTexture(SHADER_SAMPLER0, info.baseTexture, info.baseTextureFrame);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER0, TEXTURE_WHITE);
				if (hasShade) BindTexture(SHADER_SAMPLER1, info.shadeTexture, info.baseTextureFrame);
				else if (hasBase) BindTexture(SHADER_SAMPLER1, info.baseTexture, info.baseTextureFrame);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER1, TEXTURE_WHITE);
				if (hasSpecular) BindTexture(SHADER_SAMPLER2, info.specularTexture, info.specularTextureFrame);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER2, TEXTURE_BLACK);
				if (hasLightWarp) BindTexture(SHADER_SAMPLER3, info.lightWarpTexture, 0);
				if (hasSelfIllumMask) BindTexture(SHADER_SAMPLER8, info.selfIllumMask, info.selfIllumMaskFrame);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER8, TEXTURE_BLACK);
				if (envmap) BindTexture(SAMPLER_ENVMAP, info.envMap, 0);

				// c0: $color (.a = $alpha), c49: $color2 - both linear, both stack in the PS.
				float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				params[info.baseColor]->GetVecValue(baseColor, 3);
				baseColor[0] = GammaToLinear(baseColor[0]);
				baseColor[1] = GammaToLinear(baseColor[1]);
				baseColor[2] = GammaToLinear(baseColor[2]);
				baseColor[3] = params[info.alpha]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant(PSREG_SELFILLUMTINT, baseColor);

				float baseColor2[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
				params[info.baseColor2]->GetVecValue(baseColor2, 3);
				baseColor2[0] = GammaToLinear(baseColor2[0]);
				baseColor2[1] = GammaToLinear(baseColor2[1]);
				baseColor2[2] = GammaToLinear(baseColor2[2]);
				pShaderAPI->SetPixelShaderConstant(49, baseColor2);

				// .a carries $irisscale; clamped so the pixel shader's divide can't blow up.
				float irisColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				params[info.irisColor]->GetVecValue(irisColor, 3);
				irisColor[0] = GammaToLinear(irisColor[0]);
				irisColor[1] = GammaToLinear(irisColor[1]);
				irisColor[2] = GammaToLinear(irisColor[2]);
				irisColor[3] = MAX(params[info.irisScale]->GetFloatValue(), 0.0001f);
				pShaderAPI->SetPixelShaderConstant(48, irisColor);

				float eyeWhiteColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				params[info.eyeWhiteColor]->GetVecValue(eyeWhiteColor, 3);
				eyeWhiteColor[0] = GammaToLinear(eyeWhiteColor[0]);
				eyeWhiteColor[1] = GammaToLinear(eyeWhiteColor[1]);
				eyeWhiteColor[2] = GammaToLinear(eyeWhiteColor[2]);
				pShaderAPI->SetPixelShaderConstant(50, eyeWhiteColor);

				float selfIllumTint[4] = { 1.0f, 1.0f, 1.0f,
					selfIllum ? 1.0f : 0.0f };
				params[info.selfIllumTint]->GetVecValue(selfIllumTint, 3);
				selfIllumTint[0] = GammaToLinear(selfIllumTint[0]);
				selfIllumTint[1] = GammaToLinear(selfIllumTint[1]);
				selfIllumTint[2] = GammaToLinear(selfIllumTint[2]);
				pShaderAPI->SetPixelShaderConstant(51, selfIllumTint);

				if (envmap)
				{
					float envTint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
					params[info.envMapTint]->GetVecValue(envTint, 3);
					envTint[0] = GammaToLinear(envTint[0]);
					envTint[1] = GammaToLinear(envTint[1]);
					envTint[2] = GammaToLinear(envTint[2]);
					envTint[3] = params[info.envMapScale]->GetFloatValue();
					pShaderAPI->SetPixelShaderConstant(52, envTint);
				}

				// c10: .x shine brightness, .y ambient floor (no local lights), .z shine saturation.
				float specular[4] = {
					params[info.specBrightness]->GetFloatValue(),
					params[info.fallbackBrightness]->GetFloatValue(),
					params[info.specSaturation]->GetFloatValue(),
					0.0f
				};
				pShaderAPI->SetPixelShaderConstant(10, specular);

				// c3: .x eyelidblend, .y iris/eyewhite tint reference, .z extra boundary width,
				// .w selfillum reads the mask texture (1) or the iris (0) when no mask is set.
				float eyeParams[4] = {
					MIN(MAX(params[info.eyelidBlend]->GetFloatValue(), 0.0f), 1.0f),
					params[info.irisMaskReference]->GetFloatValue(),
					MAX(params[info.irisMaskSoftness]->GetFloatValue(), 0.0f),
					hasSelfIllumMask ? 1.0f : 0.0f };
				pShaderAPI->SetPixelShaderConstant(3, eyeParams);

				// c26/c27: the iris projection planes, straight from the engine-written vars.
				float irisU[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
				float irisV[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
				params[info.irisU]->GetVecValue(irisU, 4);
				params[info.irisV]->GetVecValue(irisV, 4);
				pShaderAPI->SetPixelShaderConstant(26, irisU);
				pShaderAPI->SetPixelShaderConstant(27, irisV);

				float eyeOrigin[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				params[info.eyeOrigin]->GetVecValue(eyeOrigin, 3);
				pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, eyeOrigin);

				float eyePosition[4];
				pShaderAPI->GetWorldSpaceCameraPosition(eyePosition);
				pShaderAPI->SetPixelShaderConstant(PSREG_EYEPOS_SPEC_EXPONENT, eyePosition);

				LightState_t lightState;
				pShaderAPI->GetDX9LightState(&lightState);
				int numLights = lightState.m_nNumLights;
				int numBones = pShaderAPI->GetCurrentNumBones();
				int fogIndex = NPRFogIndex(pShaderAPI);

				bool flashlightShadows = flashlight && NPRBindFlashlightState(pShaderAPI);

				DECLARE_DYNAMIC_VERTEX_SHADER(pulse_tooneye_vs30);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW,
					pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, numLights);
				SET_DYNAMIC_VERTEX_SHADER(pulse_tooneye_vs30);

				DECLARE_DYNAMIC_PIXEL_SHADER(pulse_tooneye_ps30);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, flashlight ? 0 : numLights);
				// The overlay blends through its own alpha, so it must not have that alpha
				// overwritten by the water-fog-to-dest-alpha path.
				SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA,
					!flashlight && !overlay && pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA,
					!flashlight && !overlay
						&& pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA));
				SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
				SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, flashlightShadows);
				SET_DYNAMIC_PIXEL_SHADER(pulse_tooneye_ps30);

				pShaderAPI->SetPixelShaderFogParams(PSREG_FOG_PARAMS);
				NPRApplyLightingOnly(pShaderAPI, params);
			}

			Draw();
		}
	}
END_SHADER;
