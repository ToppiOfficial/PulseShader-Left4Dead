// PulseNPR - generic cel shading with optional specular and rim masks.
// Variants keep their own authored mask layouts.

#include "npr_common_dx9.h"

#include "pulse_npr_vs30.inc"
#include "pulse_npr_ps30.inc"

struct NPR_Vars_t
{
	NPR_Vars_t()
	{
		memset(this, 0xFF, sizeof(*this));
	}

	int baseTexture;
	int baseTextureFrame;
	int baseTextureTransform;
	int shadowColor;
	int celShadeSteps;
	int specularMaskTexture;
	int emissionTexture;
	int emissionTextureFrame;
	int emissionTint;
	int emissionStrength;
	int alphaTestReference;
	int envMap;
	int envMapMask;
	int envMapTint;
	int envMapAlbedoTint;
	int envMapAlbedoBoost;
	int reflectionTexture;
	int reflectionTextureFrame;
	int reflectionStrength;
	int reflectionAddColor;
	int reflectionMultiplyColor;
	int specularSmoothness;
	int specularBrightness;
	int specSize;
	int fallbackBrightness;
	int rimLightWidth;
	int rimLightBrightness;
	int outlineWidth;
	int outlineAngle;
	int outlineColor;
	int outlineBaseBlend;
	int detailTexture;
	int detailFrame;
	int detailScale;
	int detailBlendMode;
	int detailBlendFactor;
	int detailTint;
	int detailTextureTransform;
	int eyelid;
	int flashlightTexture;
	int renderBackface;
};

BEGIN_NPR_SHADER(PulseNPR, "Cel character rendering for models")
	BEGIN_SHADER_PARAMS;
		SHADER_PARAM(SHADOWCOLOR, SHADER_PARAM_TYPE_COLOR, "[0.3 0.3 0.3]", "Tint applied to the base map on the shadow side of the cel step");
		SHADER_PARAM(CELSHADESTEPS, SHADER_PARAM_TYPE_INTEGER, "0", "Intermediate cel-shading bands, clamped from 0 to 4");
		SHADER_PARAM(SPECULARMASKTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Packed highlight mask: specular in red, rim light in green");
		SHADER_PARAM(EMISSIONTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Emission texture");
		SHADER_PARAM(EMISSIONTEXTUREFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $emissiontexture");
		SHADER_PARAM(EMISSIONTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Emission texture tint");
		SHADER_PARAM(EMISSIONSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1", "Emission strength");
		SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0", "Cutout threshold");
		SHADER_PARAM(ENVMAP, SHADER_PARAM_TYPE_ENVMAP, "", "Cubemap reflection; overrides $reflectiontexture");
		SHADER_PARAM(ENVMAPMASK, SHADER_PARAM_TYPE_TEXTURE, "", "Grayscale envmap mask; white when omitted");
		SHADER_PARAM(ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Envmap reflection tint");
		SHADER_PARAM(ENVMAPALBEDOTINT, SHADER_PARAM_TYPE_BOOL, "0", "Tint the envmap by the base color instead of $envmaptint");
		SHADER_PARAM(ENVMAPALBEDOBOOST, SHADER_PARAM_TYPE_FLOAT, "0", "Brightness added to the albedo envmap tint");
		SHADER_PARAM(REFLECTIONTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Matcap indexed by the view-space normal: additive in red, multiplicative in green");
		SHADER_PARAM(REFLECTIONTEXTUREFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $reflectiontexture");
		SHADER_PARAM(REFLECTIONSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1", "Reflection strength");
		SHADER_PARAM(REFLECTIONADDCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Additive reflection tint");
		SHADER_PARAM(REFLECTIONMULTIPLYCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Multiplicative reflection tint");
		SHADER_PARAM(SPECULARSMOOTHNESS, SHADER_PARAM_TYPE_FLOAT, "1.03", "Specular edge smoothness");
		SHADER_PARAM(SPECULARBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "0", "Specular brightness, 0 disables the highlight");
		SHADER_PARAM(SPECSIZE, SHADER_PARAM_TYPE_FLOAT, "5", "Specular size");
		SHADER_PARAM(FALLBACKBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "0.001", "Brightness without direct lights");
		SHADER_PARAM(RIMLIGHTWIDTH, SHADER_PARAM_TYPE_FLOAT, "0", "NPR rim light width, 0 disables the rim light");
		SHADER_PARAM(RIMLIGHTBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "2", "NPR rim light brightness");
		SHADER_PARAM(OUTLINEWIDTH, SHADER_PARAM_TYPE_FLOAT, "0", "Outline width in model units, 0 disables the outline pass");
		SHADER_PARAM(OUTLINEANGLE, SHADER_PARAM_TYPE_FLOAT, "0", "Minimum view angle for outline expansion in degrees, 0 disables angle fading");
		SHADER_PARAM(OUTLINECOLOR, SHADER_PARAM_TYPE_COLOR, "[0 0 0]", "Outline tint");
		SHADER_PARAM(OUTLINEBASEBLEND, SHADER_PARAM_TYPE_FLOAT, "0", "Base texture contribution to the outline");
		SHADER_PARAM(DETAIL, SHADER_PARAM_TYPE_TEXTURE, "", "Detail texture");
		SHADER_PARAM(DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $detail");
		SHADER_PARAM(DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "Detail texture scale");
		SHADER_PARAM(DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "Detail texture blend mode");
		SHADER_PARAM(DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1", "Detail texture blend strength");
		SHADER_PARAM(DETAILTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Detail texture tint");
		SHADER_PARAM(DETAILTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$detail texture transform");
		SHADER_PARAM(EYELID, SHADER_PARAM_TYPE_BOOL, "0", "Draw over the front hair without blending and suppress the outline pass");
		SHADER_PARAM(RENDERBACKFACE, SHADER_PARAM_TYPE_BOOL, "0", "Draw backfaces with reversed shading normals");
	END_SHADER_PARAMS;

	void SetupVars(NPR_Vars_t &info)
	{
		info.baseTexture = BASETEXTURE;
		info.baseTextureFrame = FRAME;
		info.baseTextureTransform = BASETEXTURETRANSFORM;
		info.shadowColor = SHADOWCOLOR;
		info.celShadeSteps = CELSHADESTEPS;
		info.specularMaskTexture = SPECULARMASKTEXTURE;
		info.emissionTexture = EMISSIONTEXTURE;
		info.emissionTextureFrame = EMISSIONTEXTUREFRAME;
		info.emissionTint = EMISSIONTINT;
		info.emissionStrength = EMISSIONSTRENGTH;
		info.alphaTestReference = ALPHATESTREFERENCE;
		info.envMap = ENVMAP;
		info.envMapMask = ENVMAPMASK;
		info.envMapTint = ENVMAPTINT;
		info.envMapAlbedoTint = ENVMAPALBEDOTINT;
		info.envMapAlbedoBoost = ENVMAPALBEDOBOOST;
		info.reflectionTexture = REFLECTIONTEXTURE;
		info.reflectionTextureFrame = REFLECTIONTEXTUREFRAME;
		info.reflectionStrength = REFLECTIONSTRENGTH;
		info.reflectionAddColor = REFLECTIONADDCOLOR;
		info.reflectionMultiplyColor = REFLECTIONMULTIPLYCOLOR;
		info.specularSmoothness = SPECULARSMOOTHNESS;
		info.specularBrightness = SPECULARBRIGHTNESS;
		info.specSize = SPECSIZE;
		info.fallbackBrightness = FALLBACKBRIGHTNESS;
		info.rimLightWidth = RIMLIGHTWIDTH;
		info.rimLightBrightness = RIMLIGHTBRIGHTNESS;
		info.outlineWidth = OUTLINEWIDTH;
		info.outlineAngle = OUTLINEANGLE;
		info.outlineColor = OUTLINECOLOR;
		info.outlineBaseBlend = OUTLINEBASEBLEND;
		info.detailTexture = DETAIL;
		info.detailFrame = DETAILFRAME;
		info.detailScale = DETAILSCALE;
		info.detailBlendMode = DETAILBLENDMODE;
		info.detailBlendFactor = DETAILBLENDFACTOR;
		info.detailTint = DETAILTINT;
		info.detailTextureTransform = DETAILTEXTURETRANSFORM;
		info.eyelid = EYELID;
		info.flashlightTexture = FLASHLIGHTTEXTURE;
		info.renderBackface = RENDERBACKFACE;
	}

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS(MATERIAL_VAR_MODEL);
		if (!params[SHADOWCOLOR]->IsDefined())
			params[SHADOWCOLOR]->SetVecValue(0.3f, 0.3f, 0.3f);
		SET_PARAM_INT_IF_NOT_DEFINED(CELSHADESTEPS, 0);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARSMOOTHNESS, 1.03f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARBRIGHTNESS, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECSIZE, 5.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(FALLBACKBRIGHTNESS, 0.001f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(EMISSIONSTRENGTH, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(RIMLIGHTWIDTH, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(RIMLIGHTBRIGHTNESS, 2.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEWIDTH, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEANGLE, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEBASEBLEND, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(REFLECTIONSTRENGTH, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(ENVMAPALBEDOBOOST, 0.0f);

		NPRSetFlashlightTexturePath(params);
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		NPR_Vars_t info;
		SetupVars(info);
		LoadTexture(info.flashlightTexture);
		if (params[info.baseTexture]->IsDefined()) LoadTexture(info.baseTexture);
		if (params[info.specularMaskTexture]->IsDefined()) LoadTexture(info.specularMaskTexture);
		if (params[info.emissionTexture]->IsDefined()) LoadTexture(info.emissionTexture);
		if (params[info.envMap]->IsDefined()) LoadCubeMap(info.envMap);
		if (params[info.envMapMask]->IsDefined()) LoadTexture(info.envMapMask);
		if (params[info.reflectionTexture]->IsDefined()) LoadTexture(info.reflectionTexture);
		if (params[info.detailTexture]->IsDefined()) LoadTexture(info.detailTexture);

		NPRSetModelFlags(params);
	}

	SHADER_DRAW
	{
		NPR_Vars_t info;
		SetupVars(info);
		bool hasBase = params[info.baseTexture]->IsTexture();
		bool hasSpecularMask = params[info.specularMaskTexture]->IsTexture();
		bool hasEmission = params[info.emissionTexture]->IsTexture();
		bool hasEnvMap = params[info.envMap]->IsTexture();
		bool hasEnvMapMask = params[info.envMapMask]->IsTexture();
		bool hasReflection = params[info.reflectionTexture]->IsTexture();
		bool hasDetail = params[info.detailTexture]->IsTexture();
		bool flashlight = UsingFlashlight(params);
		bool eyelid = params[info.eyelid]->GetIntValue() != 0;
		BlendType_t blendType = EvaluateBlendRequirements(info.baseTexture, true,
			info.detailTexture);
		bool alphaTest = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST);
		bool fullyOpaque = blendType == BT_NONE && !alphaTest;
		bool outlineEnabled = !flashlight && !eyelid
			&& params[info.outlineWidth]->GetFloatValue() > 0.0f;
		bool renderBackface = params[info.renderBackface]->GetIntValue() != 0
			&& !IS_FLAG_SET(MATERIAL_VAR_NOCULL);

		for (int pass = 0; pass < 2; ++pass)
		{
			bool outline = pass == 0;
			bool renderBackfacePass = renderBackface && !outline;
			bool envmap = hasEnvMap && !outline && !flashlight;
			if (outline && !outlineEnabled)
			{
				Draw(false);
				continue;
			}

			if (IsSnapshotting())
			{
				NPRSnapshotPassState(pShaderShadow, params, outline, renderBackfacePass,
					flashlight, blendType,
					alphaTest, params[info.alphaTestReference]->GetFloatValue());

				pShaderShadow->EnableTexture(NPR_SAMPLER_BASE, true);
				pShaderShadow->EnableSRGBRead(NPR_SAMPLER_BASE, true);
				if (hasSpecularMask)
				{
					pShaderShadow->EnableTexture(SHADER_SAMPLER1, true);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER1, false);
				}
				if (!outline && !flashlight && hasEmission)
				{
					pShaderShadow->EnableTexture(SHADER_SAMPLER2, true);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER2, true);
				}
				if (envmap)
				{
					pShaderShadow->EnableTexture(SHADER_SAMPLER3, true);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER3, false);
					pShaderShadow->EnableTexture(SHADER_SAMPLER9, true);
					if (g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE)
						pShaderShadow->EnableSRGBRead(SHADER_SAMPLER9, true);
				}
				pShaderShadow->EnableTexture(NPR_SAMPLER_REFLECTION, true);
				pShaderShadow->EnableSRGBRead(NPR_SAMPLER_REFLECTION, false);
				if (hasDetail)
				{
					pShaderShadow->EnableTexture(NPR_SAMPLER_DETAIL, true);
					pShaderShadow->EnableSRGBRead(NPR_SAMPLER_DETAIL,
						params[info.detailBlendMode]->GetIntValue() != 0);
				}

				int shadowFilter = NPRShadowFilterMode(flashlight);
				if (flashlight)
					NPRSnapshotFlashlightSamplers(pShaderShadow, shadowFilter);

				pShaderShadow->EnableSRGBWrite(true);
				pShaderShadow->VertexShaderVertexFormat(
					VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED, 1, 0, 0);

				DECLARE_STATIC_VERTEX_SHADER(pulse_npr_vs30);
				SET_STATIC_VERTEX_SHADER_COMBO(OUTLINE, outline);
				SET_STATIC_VERTEX_SHADER_COMBO(EYELID, eyelid);
				SET_STATIC_VERTEX_SHADER(pulse_npr_vs30);

				DECLARE_STATIC_PIXEL_SHADER(pulse_npr_ps30);
				SET_STATIC_PIXEL_SHADER_COMBO(OUTLINE, outline);
				SET_STATIC_PIXEL_SHADER_COMBO(DETAILTEXTURE, hasDetail);
				SET_STATIC_PIXEL_SHADER_COMBO(SPECULARMASKTEXTURE, !outline && hasSpecularMask);
				SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, !outline && !flashlight && hasEmission);
				SET_STATIC_PIXEL_SHADER_COMBO(ENVMAP, envmap);
				SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, flashlight);
				SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, shadowFilter);
				SET_STATIC_PIXEL_SHADER(pulse_npr_ps30);

				DefaultFog();
				NPRWriteLightingCommandBuffer(params);
			}
			else
			{
				if (hasBase) BindTexture(NPR_SAMPLER_BASE, info.baseTexture, info.baseTextureFrame);
				else pShaderAPI->BindStandardTexture(NPR_SAMPLER_BASE, TEXTURE_WHITE);
				if (hasSpecularMask) BindTexture(SHADER_SAMPLER1, info.specularMaskTexture, 0);
				if (hasEmission)
				{
					BindTexture(SHADER_SAMPLER2, info.emissionTexture, info.emissionTextureFrame);
					SetPixelShaderConstant(PSREG_CONSTANT_35, info.emissionTint, info.emissionStrength);
				}
				if (envmap)
				{
					BindTexture(SHADER_SAMPLER9, info.envMap, 0);
					if (hasEnvMapMask) BindTexture(SHADER_SAMPLER3, info.envMapMask, 0);
					else pShaderAPI->BindStandardTexture(SHADER_SAMPLER3, TEXTURE_WHITE);
				}
				if (hasReflection) BindTexture(NPR_SAMPLER_REFLECTION, info.reflectionTexture, info.reflectionTextureFrame);
				else pShaderAPI->BindStandardTexture(NPR_SAMPLER_REFLECTION, TEXTURE_BLACK);
				if (hasDetail) BindTexture(NPR_SAMPLER_DETAIL, info.detailTexture, info.detailFrame);

				float shadowColor[4] = { 0.3f, 0.3f, 0.3f, 0.0f };
				params[info.shadowColor]->GetVecValue(shadowColor, 3);
				pShaderAPI->SetPixelShaderConstant(49, shadowColor);

				float reflectionAdd[4] = { 1.0f, 1.0f, 1.0f,
					hasReflection ? params[info.reflectionStrength]->GetFloatValue() : 0.0f };
				params[info.reflectionAddColor]->GetVecValue(reflectionAdd, 3);
				pShaderAPI->SetPixelShaderConstant(50, reflectionAdd);

				float reflectionMultiply[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
				params[info.reflectionMultiplyColor]->GetVecValue(reflectionMultiply, 3);
				pShaderAPI->SetPixelShaderConstant(51, reflectionMultiply);

				if (envmap)
				{
					float envTint[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
					params[info.envMapTint]->GetVecValue(envTint, 3);
					if (params[info.envMapAlbedoTint]->GetIntValue() != 0)
						envTint[3] = 1.0f + MAX(params[info.envMapAlbedoBoost]->GetFloatValue(), 0.0f);
					pShaderAPI->SetPixelShaderConstant(52, envTint);
				}

				float specular[4] = {
					params[info.specSize]->GetFloatValue(),
					params[info.specularSmoothness]->GetFloatValue(),
					params[info.specularBrightness]->GetFloatValue(),
					params[info.fallbackBrightness]->GetFloatValue()
				};
				pShaderAPI->SetPixelShaderConstant(10, specular);

				NPRSetOutlineConstants(pShaderAPI, params[info.outlineWidth]->GetFloatValue(),
					params[info.outlineAngle]->GetFloatValue());

				float outlineColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				params[info.outlineColor]->GetVecValue(outlineColor, 3);
				pShaderAPI->SetPixelShaderConstant(46, outlineColor);

				NPRSetDetailTint(pShaderAPI, params, info.detailTint, info.detailBlendFactor);
				NPRSetRenderBackface(pShaderAPI, renderBackfacePass);

				float rimLightWidth = params[info.rimLightWidth]->GetFloatValue();
				float detailParams[4] = {
					(float)params[info.detailBlendMode]->GetIntValue(),
					rimLightWidth,
					MIN(MAX(params[info.outlineBaseBlend]->GetFloatValue(), 0.0f), 1.0f),
					rimLightWidth > 0.0f
						? params[info.rimLightBrightness]->GetFloatValue() : 0.0f
				};
				pShaderAPI->SetPixelShaderConstant(48, detailParams);

				float celShadeParams[4] = {
					(float)MIN(MAX(params[info.celShadeSteps]->GetIntValue(), 0), 4),
					0.0f, 0.0f, 0.0f
				};
				pShaderAPI->SetPixelShaderConstant(54, celShadeParams);
				SetPixelShaderConstant(PSREG_CONSTANT_37, COLOR2);

				float eyePosition[4];
				pShaderAPI->GetWorldSpaceCameraPosition(eyePosition);
				pShaderAPI->SetPixelShaderConstant(PSREG_EYEPOS_SPEC_EXPONENT, eyePosition);

				LightState_t lightState;
				pShaderAPI->GetDX9LightState(&lightState);
				int numLights = outline ? 0 : lightState.m_nNumLights;
				int numBones = pShaderAPI->GetCurrentNumBones();
				MaterialFogMode_t fogType = pShaderAPI->GetSceneFogMode();
				int fogIndex = NPRFogIndex(pShaderAPI);

				bool flashlightShadows = flashlight && NPRBindFlashlightState(pShaderAPI);

				DECLARE_DYNAMIC_VERTEX_SHADER(pulse_npr_vs30);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW,
					pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, numLights);
				SET_DYNAMIC_VERTEX_SHADER(pulse_npr_vs30);

				DECLARE_DYNAMIC_PIXEL_SHADER(pulse_npr_ps30);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, numLights);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA,
					!flashlight && fullyOpaque && fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA,
					!flashlight && fullyOpaque
						&& pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA));
				SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
				SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, flashlightShadows);
				SET_DYNAMIC_PIXEL_SHADER(pulse_npr_ps30);

				SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0,
					info.baseTextureTransform);
				VMatrix viewMatrix, transposedViewMatrix;
				pShaderAPI->GetMatrix(MATERIAL_VIEW, viewMatrix.Base());
				MatrixTranspose(viewMatrix, transposedViewMatrix);
				pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_5,
					transposedViewMatrix.Base(), 3);
				if (hasDetail)
					SetVertexShaderTextureScaledTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_3,
						info.detailTextureTransform, info.detailScale);
				pShaderAPI->SetPixelShaderFogParams(PSREG_FOG_PARAMS);
				NPRApplyLightingOnly(pShaderAPI, params);
			}

			Draw();
		}
	}
END_SHADER;
