#include "npr_common_dx9.h"

#include "pulse_umamusume_vs30.inc"
#include "pulse_umamusume_ps30.inc"

struct NPR_Vars_t
{
	NPR_Vars_t()
	{
		memset(this, 0xFF, sizeof(*this));
	}

	int baseTexture;
	int baseTextureFrame;
	int baseTextureTransform;
	int baseColor;
	int baseColor2;
	int alpha;
	int toonMap;
	int tripleMaskMap;
	int optionMaskMap;
	int reflectionTexture;
	int envMap;
	int envMapTint;
	int reflectionStrength;
	int reflectionAddColor;
	int reflectionMultiplyColor;
	int useAohaAlpha;
	int alphaTestReference;
	int highlightColor;
	int highlightBrightness;
	int specularSmoothness;
	int specularBrightness;
	int specSize;
	int fallbackBrightness;
	int rimLightWidth;
	int rimLightBrightness;
	int outlineWidth;
	int outlineColor;
	int outlineBaseBlend;
	int detailTexture;
	int detailFrame;
	int detailScale;
	int detailBlendMode;
	int detailBlendFactor;
	int detailTint;
	int detailTextureTransform;
	int aoStrength;
	int faceMode;
	int faceCheekSpread;
	int faceYaw;
	int eyelid;
	int flashlightTexture;
};

BEGIN_NPR_SHADER(PulseUmamusume, "Umamusume character rendering for models")
	BEGIN_SHADER_PARAMS;
		SHADER_PARAM(BASESHADETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Authored shaded base texture");
		SHADER_PARAM(AOHATEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "AO, highlight, and alpha texture");
		SHADER_PARAM(RSRFLTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Rim, specular, and reflection texture");
		SHADER_PARAM(ENVMAP, SHADER_PARAM_TYPE_ENVMAP, "", "Cubemap reflected on RSRFL green, tinting by base color like metalness");
		SHADER_PARAM(ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Envmap reflection tint");
		SHADER_PARAM(REFLECTIONTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Packed additive red and multiplicative green reflection texture");
		SHADER_PARAM(REFLECTIONSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1", "Reflection strength");
		SHADER_PARAM(REFLECTIONADDCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Additive reflection tint");
		SHADER_PARAM(REFLECTIONMULTIPLYCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Multiplicative reflection tint");
		SHADER_PARAM(USEAOHAALPHA, SHADER_PARAM_TYPE_BOOL, "1", "Use AOHA blue as opacity");
		SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0.5", "Cutout threshold");
		SHADER_PARAM(HIGHLIGHTCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Highlight color");
		SHADER_PARAM(HIGHLIGHTBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "3", "Highlight brightness");
		SHADER_PARAM(SPECULARSMOOTHNESS, SHADER_PARAM_TYPE_FLOAT, "1.03", "Specular edge smoothness");
		SHADER_PARAM(SPECULARBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "30", "Specular brightness");
		SHADER_PARAM(SPECSIZE, SHADER_PARAM_TYPE_FLOAT, "5", "Specular size");
		SHADER_PARAM(FALLBACKBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "0.001", "Brightness without direct lights");
		SHADER_PARAM(RIMLIGHTWIDTH, SHADER_PARAM_TYPE_FLOAT, "0", "NPR rim light width, 0 disables the rim light");
		SHADER_PARAM(RIMLIGHTBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "2", "NPR rim light brightness");
		SHADER_PARAM(OUTLINEWIDTH, SHADER_PARAM_TYPE_FLOAT, "0", "Outline width in model units");
		SHADER_PARAM(OUTLINECOLOR, SHADER_PARAM_TYPE_COLOR, "[0 0 0]", "Outline tint");
		SHADER_PARAM(OUTLINEBASEBLEND, SHADER_PARAM_TYPE_FLOAT, "0", "Base texture contribution to the outline");
		SHADER_PARAM(DETAIL, SHADER_PARAM_TYPE_TEXTURE, "", "Detail texture");
		SHADER_PARAM(DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $detail");
		SHADER_PARAM(DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "Detail texture scale");
		SHADER_PARAM(DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "Detail texture blend mode");
		SHADER_PARAM(DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1", "Detail texture blend strength");
		SHADER_PARAM(DETAILTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Detail texture tint");
		SHADER_PARAM(DETAILTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$detail texture transform");
		SHADER_PARAM(AOSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1", "Lit AOHA red-channel strength");
		SHADER_PARAM(FACE, SHADER_PARAM_TYPE_BOOL, "0", "Shade as a face: AOHA red drives the terminator and green masks the cheek and nose overlays");
		SHADER_PARAM(FACECHEEKSPREAD, SHADER_PARAM_TYPE_FLOAT, "1", "Widens the light angles the cheek wing reads at; 1 matches the original, 3 covers most of the wing");
		SHADER_PARAM(EYELID, SHADER_PARAM_TYPE_BOOL, "0", "Draw over the front hair: for eyelash and eyebrow meshes that read through the bangs");
		SHADER_PARAM(FACEYAW, SHADER_PARAM_TYPE_FLOAT, "270", "Degrees the reference pose faces away from +X. 270 is -Y, what these ports export; 0 is the studiomdl convention");
	END_SHADER_PARAMS;

	void SetupVars(NPR_Vars_t &info)
	{
		info.baseTexture = BASETEXTURE;
		info.baseTextureFrame = FRAME;
		info.baseTextureTransform = BASETEXTURETRANSFORM;
		info.baseColor = COLOR;
		info.baseColor2 = COLOR2;
		info.alpha = ALPHA;
		info.toonMap = BASESHADETEXTURE;
		info.tripleMaskMap = AOHATEXTURE;
		info.optionMaskMap = RSRFLTEXTURE;
		info.reflectionTexture = REFLECTIONTEXTURE;
		info.envMap = ENVMAP;
		info.envMapTint = ENVMAPTINT;
		info.reflectionStrength = REFLECTIONSTRENGTH;
		info.reflectionAddColor = REFLECTIONADDCOLOR;
		info.reflectionMultiplyColor = REFLECTIONMULTIPLYCOLOR;
		info.useAohaAlpha = USEAOHAALPHA;
		info.alphaTestReference = ALPHATESTREFERENCE;
		info.highlightColor = HIGHLIGHTCOLOR;
		info.highlightBrightness = HIGHLIGHTBRIGHTNESS;
		info.specularSmoothness = SPECULARSMOOTHNESS;
		info.specularBrightness = SPECULARBRIGHTNESS;
		info.specSize = SPECSIZE;
		info.fallbackBrightness = FALLBACKBRIGHTNESS;
		info.rimLightWidth = RIMLIGHTWIDTH;
		info.rimLightBrightness = RIMLIGHTBRIGHTNESS;
		info.outlineWidth = OUTLINEWIDTH;
		info.outlineColor = OUTLINECOLOR;
		info.outlineBaseBlend = OUTLINEBASEBLEND;
		info.detailTexture = DETAIL;
		info.detailFrame = DETAILFRAME;
		info.detailScale = DETAILSCALE;
		info.detailBlendMode = DETAILBLENDMODE;
		info.detailBlendFactor = DETAILBLENDFACTOR;
		info.detailTint = DETAILTINT;
		info.detailTextureTransform = DETAILTEXTURETRANSFORM;
		info.aoStrength = AOSTRENGTH;
		info.faceMode = FACE;
		info.faceCheekSpread = FACECHEEKSPREAD;
		info.faceYaw = FACEYAW;
		info.eyelid = EYELID;
		info.flashlightTexture = FLASHLIGHTTEXTURE;
	}

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS(MATERIAL_VAR_MODEL);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(ALPHATESTREFERENCE, 0.5f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(HIGHLIGHTBRIGHTNESS, 3.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARSMOOTHNESS, 1.03f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARBRIGHTNESS, 30.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECSIZE, 5.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(FALLBACKBRIGHTNESS, 0.001f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(RIMLIGHTWIDTH, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(RIMLIGHTBRIGHTNESS, 2.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEWIDTH, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEBASEBLEND, 0.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(AOSTRENGTH, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(FACECHEEKSPREAD, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(FACEYAW, 270.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(REFLECTIONSTRENGTH, 1.0f);
		SET_PARAM_INT_IF_NOT_DEFINED(USEAOHAALPHA, 1);

		if (!params[BASESHADETEXTURE]->IsDefined() && params[BASETEXTURE]->IsDefined())
			params[BASESHADETEXTURE]->SetStringValue(params[BASETEXTURE]->GetStringValue());

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
		if (params[info.toonMap]->IsDefined()) LoadTexture(info.toonMap);
		if (params[info.tripleMaskMap]->IsDefined()) LoadTexture(info.tripleMaskMap);
		if (params[info.optionMaskMap]->IsDefined()) LoadTexture(info.optionMaskMap);
		if (params[info.reflectionTexture]->IsDefined()) LoadTexture(info.reflectionTexture);
		if (params[info.envMap]->IsDefined()) LoadCubeMap(info.envMap);
		if (params[info.detailTexture]->IsDefined()) LoadTexture(info.detailTexture);

		NPRSetModelFlags(params);
	}

	SHADER_DRAW
	{
		NPR_Vars_t info;
		SetupVars(info);
		bool hasBase = params[info.baseTexture]->IsTexture();
		bool hasToon = params[info.toonMap]->IsTexture();
		bool hasTriple = params[info.tripleMaskMap]->IsTexture();
		bool hasOption = params[info.optionMaskMap]->IsTexture();
		bool hasReflection = params[info.reflectionTexture]->IsTexture();
		bool hasEnvMap = params[info.envMap]->IsTexture();
		bool hasDetail = params[info.detailTexture]->IsTexture();
		bool flashlight = UsingFlashlight(params);
		bool face = params[info.faceMode]->GetIntValue() != 0;
		bool eyelid = params[info.eyelid]->GetIntValue() != 0;
		bool translucent = IS_FLAG_SET(MATERIAL_VAR_TRANSLUCENT);
		bool alphaTest = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST);
		// An eyelid hull would outline the lashes against the hair they draw over, so
		// $eyelid suppresses the pass rather than relying on the width being left at 0.
		bool outlineEnabled = !flashlight && !eyelid
			&& params[info.outlineWidth]->GetFloatValue() > 0.0f;

		for (int pass = 0; pass < 2; ++pass)
		{
			bool outline = pass == 0;
			bool envmap = hasEnvMap && !outline && !face && !flashlight;
			if (outline && !outlineEnabled)
			{
				Draw(false);
				continue;
			}

			if (IsSnapshotting())
			{
				NPRSnapshotPassState(pShaderShadow, params, outline, flashlight, translucent,
					alphaTest, params[info.alphaTestReference]->GetFloatValue());

				for (int sampler = SHADER_SAMPLER0; sampler <= SHADER_SAMPLER3; ++sampler)
					pShaderShadow->EnableTexture((Sampler_t)sampler, true);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0, true);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER1, true);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER2, false);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER3, false);
				pShaderShadow->EnableTexture(SHADER_SAMPLER8, true);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER8, false);
					if (envmap)
					{
						pShaderShadow->EnableTexture(SHADER_SAMPLER9, true);
						pShaderShadow->EnableSRGBRead(SHADER_SAMPLER9, true);
					}
				if (hasDetail)
				{
					pShaderShadow->EnableTexture(SHADER_SAMPLER7, true);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER7,
						params[info.detailBlendMode]->GetIntValue() != 0);
				}

				int shadowFilter = NPRShadowFilterMode(flashlight);
				if (flashlight)
					NPRSnapshotFlashlightSamplers(pShaderShadow, shadowFilter);

				pShaderShadow->EnableSRGBWrite(true);
				pShaderShadow->VertexShaderVertexFormat(
					VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED, 1, 0, 0);

				DECLARE_STATIC_VERTEX_SHADER(pulse_umamusume_vs30);
				SET_STATIC_VERTEX_SHADER_COMBO(OUTLINE, outline);
				SET_STATIC_VERTEX_SHADER_COMBO(FACE, face);
				SET_STATIC_VERTEX_SHADER_COMBO(EYELID, eyelid);
				SET_STATIC_VERTEX_SHADER(pulse_umamusume_vs30);

				DECLARE_STATIC_PIXEL_SHADER(pulse_umamusume_ps30);
				SET_STATIC_PIXEL_SHADER_COMBO(FACE, face);
				SET_STATIC_PIXEL_SHADER_COMBO(OUTLINE, outline);
				SET_STATIC_PIXEL_SHADER_COMBO(DETAILTEXTURE, hasDetail);
					SET_STATIC_PIXEL_SHADER_COMBO(ENVMAP, envmap);
				SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, flashlight);
				SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, shadowFilter);
				SET_STATIC_PIXEL_SHADER(pulse_umamusume_ps30);

				DefaultFog();
				NPRWriteLightingCommandBuffer();
			}
			else
			{
				if (hasBase) BindTexture(SHADER_SAMPLER0, info.baseTexture, info.baseTextureFrame);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER0, TEXTURE_WHITE);
				if (hasToon) BindTexture(SHADER_SAMPLER1, info.toonMap, 0);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER1, TEXTURE_WHITE);
				if (hasTriple) BindTexture(SHADER_SAMPLER2, info.tripleMaskMap, 0);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER2, TEXTURE_WHITE);
				if (hasOption) BindTexture(SHADER_SAMPLER3, info.optionMaskMap, 0);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER3, TEXTURE_BLACK);
				if (hasReflection) BindTexture(SHADER_SAMPLER8, info.reflectionTexture, 0);
				else pShaderAPI->BindStandardTexture(SHADER_SAMPLER8, TEXTURE_BLACK);
				if (hasDetail) BindTexture(SHADER_SAMPLER7, info.detailTexture, info.detailFrame);
					if (envmap)
					{
						BindTexture(SHADER_SAMPLER9, info.envMap, 0);
						float envTint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
						params[info.envMapTint]->GetVecValue(envTint, 3);
						envTint[0] = GammaToLinear(envTint[0]);
						envTint[1] = GammaToLinear(envTint[1]);
						envTint[2] = GammaToLinear(envTint[2]);
						pShaderAPI->SetPixelShaderConstant(52, envTint);
					}

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
				baseColor2[3] = params[info.useAohaAlpha]->GetIntValue() != 0 ? 1.0f : 0.0f;
				pShaderAPI->SetPixelShaderConstant(49, baseColor2);
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

				float highlight[4] = { 1.0f, 1.0f, 1.0f,
					params[info.highlightBrightness]->GetFloatValue() };
				params[info.highlightColor]->GetVecValue(highlight, 3);
				highlight[0] = GammaToLinear(highlight[0]);
				highlight[1] = GammaToLinear(highlight[1]);
				highlight[2] = GammaToLinear(highlight[2]);
				pShaderAPI->SetPixelShaderConstant(3, highlight);

				float specular[4] = {
					params[info.specSize]->GetFloatValue(),
					params[info.specularSmoothness]->GetFloatValue(),
					params[info.specularBrightness]->GetFloatValue(),
					params[info.fallbackBrightness]->GetFloatValue()
				};
				pShaderAPI->SetPixelShaderConstant(10, specular);

				NPRSetOutlineConstants(pShaderAPI, params[info.outlineWidth]->GetFloatValue());
				float outlineColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				params[info.outlineColor]->GetVecValue(outlineColor, 3);
				pShaderAPI->SetPixelShaderConstant(46, outlineColor);
				NPRSetDetailTint(pShaderAPI, params, info.detailTint, info.detailBlendFactor);
				// A zero width still leaves a hairline at the silhouette, so it gates the
				// brightness instead of relying on the edge term alone.
				float rimLightWidth = params[info.rimLightWidth]->GetFloatValue();
				float detailParams[4] = {
					(float)params[info.detailBlendMode]->GetIntValue(), translucent ? 1.0f : 0.0f,
					rimLightWidth, rimLightWidth > 0.0f
						? params[info.rimLightBrightness]->GetFloatValue() : 0.0f
				};
				pShaderAPI->SetPixelShaderConstant(48, detailParams);
				float aoAndOutline[4] = {
					MIN(MAX(params[info.aoStrength]->GetFloatValue(), 0.0f), 1.0f),
					MIN(MAX(params[info.outlineBaseBlend]->GetFloatValue(), 0.0f), 1.0f),
					face ? MAX(params[info.faceCheekSpread]->GetFloatValue(), 1.0f) : 0.0f, 0.0f
				};
				pShaderAPI->SetPixelShaderConstant(26, aoAndOutline);

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

				DECLARE_DYNAMIC_VERTEX_SHADER(pulse_umamusume_vs30);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW,
					pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, numLights);
				SET_DYNAMIC_VERTEX_SHADER(pulse_umamusume_vs30);

				DECLARE_DYNAMIC_PIXEL_SHADER(pulse_umamusume_ps30);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, numLights);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA,
					!flashlight && fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, false);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
				SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, flashlightShadows);
				SET_DYNAMIC_PIXEL_SHADER(pulse_umamusume_ps30);

				if (face)
				{
					// A reference pose is arbitrary - studiomdl bakes it into poseToBone - but
					// it is upright, so its up axis is +Z to within a few degrees and only the
					// facing needs stating. Measured: tilting up by 9 degrees moved 4.5% of the
					// cheek and nose gate decisions, all at the window edges.
					float yaw = params[info.faceYaw]->GetFloatValue() * (3.14159265f / 180.0f);
					float refForward[4] = { cosf(yaw), sinf(yaw), 0.0f, 0.0f };
					float refUp[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
					pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_8,
						refForward);
					pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_9,
						refUp);
				}

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
