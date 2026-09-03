// PulseNPR - the generic cel shader of the NPR family.
//
// Deliberately carries no authored mask layout: no AOHA, no RSRFL, no rim
// light. Those belong to Umamusume. This is what a variant starts from, and
// what a material can use directly when it only needs cel shading, an outline,
// and a matcap.

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
	int alphaTestReference;
	int reflectionTexture;
	int reflectionStrength;
	int reflectionAddColor;
	int reflectionMultiplyColor;
	int specularSmoothness;
	int specularBrightness;
	int specSize;
	int fallbackBrightness;
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
	int flashlightTexture;
};

BEGIN_NPR_SHADER(PulseNPR, "Cel character rendering for models")
	BEGIN_SHADER_PARAMS;
		SHADER_PARAM(SHADOWCOLOR, SHADER_PARAM_TYPE_COLOR, "[0.6 0.6 0.7]", "Tint applied to the base map on the shadow side of the cel step");
		SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0", "Cutout threshold");
		SHADER_PARAM(REFLECTIONTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Matcap indexed by the view-space normal: additive in red, multiplicative in green");
		SHADER_PARAM(REFLECTIONSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1", "Reflection strength");
		SHADER_PARAM(REFLECTIONADDCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Additive reflection tint");
		SHADER_PARAM(REFLECTIONMULTIPLYCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Multiplicative reflection tint");
		SHADER_PARAM(SPECULARSMOOTHNESS, SHADER_PARAM_TYPE_FLOAT, "1.03", "Specular edge smoothness");
		SHADER_PARAM(SPECULARBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "0", "Specular brightness, 0 disables the highlight");
		SHADER_PARAM(SPECSIZE, SHADER_PARAM_TYPE_FLOAT, "5", "Specular size");
		SHADER_PARAM(FALLBACKBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "0.001", "Brightness without direct lights");
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
	END_SHADER_PARAMS;

	void SetupVars(NPR_Vars_t &info)
	{
		info.baseTexture = BASETEXTURE;
		info.baseTextureFrame = FRAME;
		info.baseTextureTransform = BASETEXTURETRANSFORM;
		info.shadowColor = SHADOWCOLOR;
		info.alphaTestReference = ALPHATESTREFERENCE;
		info.reflectionTexture = REFLECTIONTEXTURE;
		info.reflectionStrength = REFLECTIONSTRENGTH;
		info.reflectionAddColor = REFLECTIONADDCOLOR;
		info.reflectionMultiplyColor = REFLECTIONMULTIPLYCOLOR;
		info.specularSmoothness = SPECULARSMOOTHNESS;
		info.specularBrightness = SPECULARBRIGHTNESS;
		info.specSize = SPECSIZE;
		info.fallbackBrightness = FALLBACKBRIGHTNESS;
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
		info.flashlightTexture = FLASHLIGHTTEXTURE;
	}

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS(MATERIAL_VAR_MODEL);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARSMOOTHNESS, 1.03f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARBRIGHTNESS, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECSIZE, 5.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(FALLBACKBRIGHTNESS, 0.001f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEWIDTH, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEANGLE, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEBASEBLEND, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(REFLECTIONSTRENGTH, 1.0f);

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
		if (params[info.reflectionTexture]->IsDefined()) LoadTexture(info.reflectionTexture);
		if (params[info.detailTexture]->IsDefined()) LoadTexture(info.detailTexture);

		NPRSetModelFlags(params);
	}

	SHADER_DRAW
	{
		NPR_Vars_t info;
		SetupVars(info);
		bool hasBase = params[info.baseTexture]->IsTexture();
		bool hasReflection = params[info.reflectionTexture]->IsTexture();
		bool hasDetail = params[info.detailTexture]->IsTexture();
		bool flashlight = UsingFlashlight(params);
		BlendType_t blendType = EvaluateBlendRequirements(info.baseTexture, true,
			info.detailTexture);
		bool translucent = blendType == BT_BLEND || blendType == BT_BLENDADD;
		bool alphaTest = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST);
		bool outlineEnabled = !flashlight && params[info.outlineWidth]->GetFloatValue() > 0.0f;

		for (int pass = 0; pass < 2; ++pass)
		{
			bool outline = pass == 0;
			if (outline && !outlineEnabled)
			{
				Draw(false);
				continue;
			}

			if (IsSnapshotting())
			{
				NPRSnapshotPassState(pShaderShadow, params, outline, flashlight, translucent,
					alphaTest, params[info.alphaTestReference]->GetFloatValue());

				pShaderShadow->EnableTexture(NPR_SAMPLER_BASE, true);
				pShaderShadow->EnableSRGBRead(NPR_SAMPLER_BASE, true);
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
				SET_STATIC_VERTEX_SHADER(pulse_npr_vs30);

				DECLARE_STATIC_PIXEL_SHADER(pulse_npr_ps30);
				SET_STATIC_PIXEL_SHADER_COMBO(OUTLINE, outline);
				SET_STATIC_PIXEL_SHADER_COMBO(DETAILTEXTURE, hasDetail);
				SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, flashlight);
				SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, shadowFilter);
				SET_STATIC_PIXEL_SHADER(pulse_npr_ps30);

				DefaultFog();
				NPRWriteLightingCommandBuffer();
			}
			else
			{
				if (hasBase) BindTexture(NPR_SAMPLER_BASE, info.baseTexture, info.baseTextureFrame);
				else pShaderAPI->BindStandardTexture(NPR_SAMPLER_BASE, TEXTURE_WHITE);
				if (hasReflection) BindTexture(NPR_SAMPLER_REFLECTION, info.reflectionTexture, 0);
				else pShaderAPI->BindStandardTexture(NPR_SAMPLER_REFLECTION, TEXTURE_BLACK);
				if (hasDetail) BindTexture(NPR_SAMPLER_DETAIL, info.detailTexture, info.detailFrame);

				float shadowColor[4] = { 0.6f, 0.6f, 0.7f, 0.0f };
				params[info.shadowColor]->GetVecValue(shadowColor, 3);
				shadowColor[0] = GammaToLinear(shadowColor[0]);
				shadowColor[1] = GammaToLinear(shadowColor[1]);
				shadowColor[2] = GammaToLinear(shadowColor[2]);
				pShaderAPI->SetPixelShaderConstant(49, shadowColor);

				float reflectionAdd[4] = { 1.0f, 1.0f, 1.0f,
					hasReflection ? params[info.reflectionStrength]->GetFloatValue() : 0.0f };
				params[info.reflectionAddColor]->GetVecValue(reflectionAdd, 3);
				reflectionAdd[0] = GammaToLinear(reflectionAdd[0]);
				reflectionAdd[1] = GammaToLinear(reflectionAdd[1]);
				reflectionAdd[2] = GammaToLinear(reflectionAdd[2]);
				pShaderAPI->SetPixelShaderConstant(50, reflectionAdd);

				float reflectionMultiply[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
				params[info.reflectionMultiplyColor]->GetVecValue(reflectionMultiply, 3);
				reflectionMultiply[0] = GammaToLinear(reflectionMultiply[0]);
				reflectionMultiply[1] = GammaToLinear(reflectionMultiply[1]);
				reflectionMultiply[2] = GammaToLinear(reflectionMultiply[2]);
				pShaderAPI->SetPixelShaderConstant(51, reflectionMultiply);

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

				// c48: x detail blend mode, z outline base blend.
				float detailParams[4] = {
					(float)params[info.detailBlendMode]->GetIntValue(),
					0.0f,
					MIN(MAX(params[info.outlineBaseBlend]->GetFloatValue(), 0.0f), 1.0f),
					0.0f
				};
				pShaderAPI->SetPixelShaderConstant(48, detailParams);

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
					!flashlight && fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, false);
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
