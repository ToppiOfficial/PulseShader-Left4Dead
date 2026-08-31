//==================================================================================================
//
// Physically Based Rendering shader for brushes and models
//
//==================================================================================================

#include "pbr_common_dx9.h"

// Includes for PS30
#include "pulse_pbr_vs30.inc"
#include "pulse_pbr_ps30.inc"


// Variables for this shader
struct PBR_Vars_t
{
    PBR_Vars_t()
    {
        memset(this, 0xFF, sizeof(*this));
    }

    int baseTexture;
    int baseColor;
    int bumpMap;
    int envMap;
    int baseTextureFrame;
    int baseTextureTransform;
    int alphaTestReference;
    int flashlightTexture;
    int emissionTexture;
    int mraoTexture;
    int metalness;
    int roughness;
    int ambientOcclusion;
    int specularIor;
    int specularWeight;
    int specularTint;
    int baseDiffuseRoughness;
    int specularTexture;
    int detailTexture;
    int detailFrame;
    int detailScale;
    int detailBlendMode;
    int detailBlendFactor;
    int detailTint;
    int detailTextureTransform;
    int lightWarpTexture;
};


// Beginning the shader
BEGIN_PBR_SHADER(PulsePBR, "Physically based rendering for models")

    // Setting up vmt parameters
    BEGIN_SHADER_PARAMS;
        SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0", "");
        SHADER_PARAM(ENVMAP, SHADER_PARAM_TYPE_ENVMAP, "", "Set the cubemap for this material.");
        SHADER_PARAM(MRAOTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Texture with metalness in R, roughness in G, ambient occlusion in B.");
        SHADER_PARAM(METALNESS, SHADER_PARAM_TYPE_FLOAT, "0", "Flat metalness, used when there is no $mraotexture.");
        SHADER_PARAM(ROUGHNESS, SHADER_PARAM_TYPE_FLOAT, "1", "Flat roughness, used when there is no $mraotexture.");
        SHADER_PARAM(AMBIENTOCCLUSION, SHADER_PARAM_TYPE_FLOAT, "1", "Flat ambient occlusion, used when there is no $mraotexture.");
        SHADER_PARAM(SPECULARIOR, SHADER_PARAM_TYPE_FLOAT, "1.5", "Index of refraction the dielectric specular reflectance is derived from. 1.5 gives the usual 0.04.");
        SHADER_PARAM(SPECULARWEIGHT, SHADER_PARAM_TYPE_FLOAT, "1", "Scales the dielectric specular reflectance.");
        SHADER_PARAM(SPECULARTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Metal reflectance tint at 82 degrees. White is an untinted Schlick curve.");
        SHADER_PARAM(BASEDIFFUSEROUGHNESS, SHADER_PARAM_TYPE_FLOAT, "0", "Oren-Nayar roughness of the diffuse lobe. 0 is Lambert.");
        SHADER_PARAM(EMISSIONTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Emission texture");
        SHADER_PARAM(NORMALTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture (deprecated, use $bumpmap)");
        SHADER_PARAM(BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture");
        SHADER_PARAM(SPECULARTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Specular F0 RGB map");
        SHADER_PARAM(DETAIL, SHADER_PARAM_TYPE_TEXTURE, "", "Detail texture");
        SHADER_PARAM(DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $detail");
        SHADER_PARAM(DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "Scale of the detail texture");
        SHADER_PARAM(DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "How $detail combines with the base texture. See TextureCombine in common_ps_fxc.h; 0 = Mod2X.");
        SHADER_PARAM(DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1", "How strongly $detail is applied");
        SHADER_PARAM(DETAILTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Detail texture tint");
        SHADER_PARAM(DETAILTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$detail texcoord transform");
        SHADER_PARAM(LIGHTWARPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "1D ramp remapping the diffuse falloff of direct light");
    END_SHADER_PARAMS;

    // Needed at shadow-state time, before info is populated.
    int GetDetailBlendMode(IMaterialVar **params) const
    {
        return params[DETAILBLENDMODE]->IsDefined() ? params[DETAILBLENDMODE]->GetIntValue() : 0;
    }

    // Setting up variables for this shader
    void SetupVars(PBR_Vars_t &info)
    {
        info.baseTexture = BASETEXTURE;
        info.baseColor = COLOR;
        info.bumpMap = BUMPMAP;
        info.baseTextureFrame = FRAME;
        info.baseTextureTransform = BASETEXTURETRANSFORM;
        info.alphaTestReference = ALPHATESTREFERENCE;
        info.flashlightTexture = FLASHLIGHTTEXTURE;
        info.envMap = ENVMAP;
        info.emissionTexture = EMISSIONTEXTURE;
        info.mraoTexture = MRAOTEXTURE;
        info.metalness = METALNESS;
        info.roughness = ROUGHNESS;
        info.ambientOcclusion = AMBIENTOCCLUSION;
        info.specularIor = SPECULARIOR;
        info.specularWeight = SPECULARWEIGHT;
        info.specularTint = SPECULARTINT;
        info.baseDiffuseRoughness = BASEDIFFUSEROUGHNESS;
        info.specularTexture = SPECULARTEXTURE;
        info.detailTexture = DETAIL;
        info.detailFrame = DETAILFRAME;
        info.detailScale = DETAILSCALE;
        info.detailBlendMode = DETAILBLENDMODE;
        info.detailBlendFactor = DETAILBLENDFACTOR;
        info.detailTint = DETAILTINT;
        info.detailTextureTransform = DETAILTEXTURETRANSFORM;
        info.lightWarpTexture = LIGHTWARPTEXTURE;
    };

    // Initializing parameters
    SHADER_INIT_PARAMS()
    {
        PulseLog( "PulsePBR: SHADER_INIT_PARAMS enter" );
        // Models-only shader: assert the flag rather than requiring every VMT to
        // carry $model 1. Without it the engine would set this material up for
        // the lightmapped path, which no longer exists in the HLSL.
        SET_FLAGS(MATERIAL_VAR_MODEL);

        SET_PARAM_FLOAT_IF_NOT_DEFINED(METALNESS, 0.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(ROUGHNESS, 1.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(AMBIENTOCCLUSION, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARIOR, 1.5f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARWEIGHT, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(BASEDIFFUSEROUGHNESS, 0.0f);
		if (!params[SPECULARTINT]->IsDefined())
			params[SPECULARTINT]->SetVecValue(1.0f, 1.0f, 1.0f);

        // Fallback for changed parameter
        if (params[NORMALTEXTURE]->IsDefined())
            params[BUMPMAP]->SetStringValue(params[NORMALTEXTURE]->GetStringValue());

        // Dynamic lights need a bumpmap
        if (!params[BUMPMAP]->IsDefined())
            params[BUMPMAP]->SetStringValue("dev/flat_normal");

        // PBR relies heavily on envmaps
        if (!params[ENVMAP]->IsDefined())
            params[ENVMAP]->SetStringValue("env_cubemap");

        PBRSetFlashlightTexturePath(params);
    };

    // Define shader fallback
    SHADER_FALLBACK
    {
        PulseLog( "PulsePBR: SHADER_FALLBACK" );
        return 0;
    };

    SHADER_INIT
    {
        PulseLog( "PulsePBR: SHADER_INIT enter" );
        PBR_Vars_t info;
        SetupVars(info);

        Assert(info.flashlightTexture >= 0);
        LoadTexture(info.flashlightTexture);

        Assert(info.bumpMap >= 0);
        LoadBumpMap(info.bumpMap);

        Assert(info.envMap >= 0);
        LoadCubeMap(info.envMap);

        if (info.emissionTexture >= 0 && params[EMISSIONTEXTURE]->IsDefined())
            LoadTexture(info.emissionTexture);

        Assert(info.mraoTexture >= 0);
        LoadTexture(info.mraoTexture);

        if (params[info.baseTexture]->IsDefined())
        {
            LoadTexture(info.baseTexture);
        }

        if (params[info.specularTexture]->IsDefined())
        {
            LoadTexture(info.specularTexture);
        }

        if (info.lightWarpTexture != -1 && params[info.lightWarpTexture]->IsDefined())
            LoadTexture(info.lightWarpTexture);

        if (info.detailTexture != -1 && params[info.detailTexture]->IsDefined())
        {
            PulseLog( "detail: loading %s", params[info.detailTexture]->GetStringValue() );
            LoadTexture(info.detailTexture);
            PulseLog( "detail: loaded, istexture=%d", (int)params[info.detailTexture]->IsTexture() );
        }

        PBRSetModelFlags(params);
    };

    // Drawing the shader
    SHADER_DRAW
    {
        // One-shot capture: vtable contents plus module bases. Under __thiscall
        // each method ends in `ret N`, so N/4 gives its argument count. Dumping
        // the tables lets that be recovered offline instead of by trial.
        PBR_Vars_t info;
        SetupVars(info);

        // Setting up booleans
        bool bHasBaseTexture = (info.baseTexture != -1) && params[info.baseTexture]->IsTexture();
        bool bHasNormalTexture = (info.bumpMap != -1) && params[info.bumpMap]->IsTexture();
        bool bHasMraoTexture = (info.mraoTexture != -1) && params[info.mraoTexture]->IsTexture();
        bool bHasEmissionTexture = (info.emissionTexture != -1) && params[info.emissionTexture]->IsTexture();
        bool bHasEnvTexture = (info.envMap != -1) && params[info.envMap]->IsTexture();
        bool bIsAlphaTested = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) != 0;
        bool bHasFlashlight = UsingFlashlight(params);
        bool bHasColor = (info.baseColor != -1) && params[info.baseColor]->IsDefined();
        bool bHasSpecularTexture = (info.specularTexture != -1) && params[info.specularTexture]->IsTexture();
        bool bHasDetailTexture = (info.detailTexture != -1) && params[info.detailTexture]->IsTexture();
        bool bHasLightWarpTexture = (info.lightWarpTexture != -1) && params[info.lightWarpTexture]->IsTexture();

        // Determining whether we're dealing with a fully opaque material
        BlendType_t nBlendType = EvaluateBlendRequirements(info.baseTexture, true);
        bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested;

        if (IsSnapshotting())
        {
            // If alphatest is on, enable it
            pShaderShadow->EnableAlphaTest(bIsAlphaTested);

            if (info.alphaTestReference != -1 && params[info.alphaTestReference]->GetFloatValue() > 0.0f)
            {
                pShaderShadow->AlphaFunc(SHADER_ALPHAFUNC_GEQUAL, params[info.alphaTestReference]->GetFloatValue());
            }

            if (bHasFlashlight )
            {
                pShaderShadow->EnableBlending(true);
                pShaderShadow->BlendFunc(SHADER_BLEND_ONE, SHADER_BLEND_ONE); // Additive blending
            }
            else
            {
                SetDefaultBlendingShadowState(info.baseTexture, true);
            }

            int nShadowFilterMode = PBRShadowFilterMode(bHasFlashlight);

            PBRSnapshotCoreSamplers(pShaderShadow);
            if (bHasLightWarpTexture)
            {
                pShaderShadow->EnableTexture(PBR_SAMPLER_LIGHTWARP, true);
                // The ramp is a lighting curve, not surface colour - sampling it
                // through sRGB would bend the curve it exists to define.
                pShaderShadow->EnableSRGBRead(PBR_SAMPLER_LIGHTWARP, false);
            }

            if (bHasDetailTexture)
            {
                pShaderShadow->EnableTexture(PBR_SAMPLER_DETAIL, true);
                // Mod2X needs 128 to read as "neutral", so it takes the map as
                // authored; every other mode wants a real gamma decode.
                if (GetDetailBlendMode(params) != 0)
                    pShaderShadow->EnableSRGBRead(PBR_SAMPLER_DETAIL, true);
            }
            // If the flashlight is on, set up its textures
            if (bHasFlashlight)
                PBRSnapshotFlashlightSamplers(pShaderShadow, nShadowFilterMode);

            // Setting up envmap
            if (bHasEnvTexture)
            {
                pShaderShadow->EnableTexture(PBR_SAMPLER_ENVMAP, true); // Envmap
                if (g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE)
                {
                    pShaderShadow->EnableSRGBRead(PBR_SAMPLER_ENVMAP, true); // Envmap is only sRGB with HDR disabled?
                }
            }

            // Enabling sRGB writing
            // See common_ps_fxc.h line 349
            // PS2b shaders and up write sRGB
            pShaderShadow->EnableSRGBWrite(true);

            // Position, normal, UV, and tangent space for normal mapping.
            unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
            pShaderShadow->VertexShaderVertexFormat(flags, 1, 0, 4);

            // ps_3_0 only: L4D2 reports dxlevel 100, so the shader-model 2b
            // fallback this was forked with is unreachable and has been removed.
        
            // Setting up static vertex shader
            DECLARE_STATIC_VERTEX_SHADER(pulse_pbr_vs30);
            SET_STATIC_VERTEX_SHADER(pulse_pbr_vs30);

            // Setting up static pixel shader
            DECLARE_STATIC_PIXEL_SHADER(pulse_pbr_ps30);
            SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, bHasFlashlight);
            SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode);
            SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, bHasEmissionTexture);
            SET_STATIC_PIXEL_SHADER_COMBO(SPECULAR, bHasSpecularTexture);
            SET_STATIC_PIXEL_SHADER_COMBO(DETAILTEXTURE, bHasDetailTexture);
            SET_STATIC_PIXEL_SHADER_COMBO(LIGHTWARPTEXTURE, bHasLightWarpTexture);
            SET_STATIC_PIXEL_SHADER(pulse_pbr_ps30);

            // Setting up fog
            DefaultFog(); // I think this is correct

            // HACK HACK HACK - enable alpha writes all the time so that we have them for underwater stuff
            pShaderShadow->EnableAlphaWrites(bFullyOpaque);
            // Per-instance lighting is baked into a command buffer at snapshot
            // time on this branch and replayed by the engine per instance - the
            // PI_ helpers write into that buffer, so they are invalid outside
            // this bracket (they faulted writing to an unset buffer when called
            // from the dynamic pass).
            PBRWriteLightingCommandBuffer();
        }
        else // Not snapshotting -- begin dynamic state
        {
            // Setting up albedo texture
            if (bHasBaseTexture)
            {
                BindTexture(PBR_SAMPLER_BASETEXTURE, info.baseTexture, info.baseTextureFrame);
            }
            else
            {
                pShaderAPI->BindStandardTexture(PBR_SAMPLER_BASETEXTURE, TEXTURE_GREY);
            }

            // Setting up vmt color
            float color[4] = {1.f, 1.f, 1.f, IS_FLAG_SET(MATERIAL_VAR_HALFLAMBERT) ? 1.f : 0.f};
            if (bHasColor)
            {
                params[info.baseColor]->GetVecValue(color, 3);
            }
            pShaderAPI->SetPixelShaderConstant(PSREG_SELFILLUMTINT, color);

            // Setting up environment map
            if (bHasEnvTexture)
            {
                BindTexture(PBR_SAMPLER_ENVMAP, info.envMap, 0);
            }
            else
            {
                pShaderAPI->BindStandardTexture(PBR_SAMPLER_ENVMAP, TEXTURE_BLACK);
            }

            // Setting up emissive texture
            if (bHasEmissionTexture)
            {
                BindTexture(PBR_SAMPLER_EMISSIVE, info.emissionTexture, 0);
            }
            else
            {
                pShaderAPI->BindStandardTexture(PBR_SAMPLER_EMISSIVE, TEXTURE_BLACK);
            }

            // Setting up normal map
            if (bHasNormalTexture)
            {
                BindTexture(PBR_SAMPLER_NORMAL, info.bumpMap, 0);
            }
            else
            {
                pShaderAPI->BindStandardTexture(PBR_SAMPLER_NORMAL, TEXTURE_NORMALMAP_FLAT);
            }

            // Setting up mrao map
            if (bHasMraoTexture)
            {
                BindTexture(PBR_SAMPLER_MRAO, info.mraoTexture, 0);
            }
            else
            {
                pShaderAPI->BindStandardTexture(PBR_SAMPLER_MRAO, TEXTURE_WHITE);
            }


            if (bHasSpecularTexture)
            {
                BindTexture(PBR_SAMPLER_SPECULAR, info.specularTexture, 0);
            }
            else
            {
                pShaderAPI->BindStandardTexture(PBR_SAMPLER_SPECULAR, TEXTURE_BLACK);
            }

            // Getting the light state
            LightState_t lightState;
            pShaderAPI->GetDX9LightState(&lightState);

            // Setting up the flashlight related textures and variables
            bool bFlashlightShadows = bHasFlashlight && PBRBindFlashlightState(pShaderAPI);

            // Getting fog info
            MaterialFogMode_t fogType = pShaderAPI->GetSceneFogMode();
            int fogIndex = PBRFogIndex(pShaderAPI);

            // Getting skinning info
            int numBones = pShaderAPI->GetCurrentNumBones();

            // Some debugging stuff
            bool bWriteDepthToAlpha = false;
            bool bWriteWaterFogToAlpha = false;
            if (bFullyOpaque)
            {
                // L4D2's IShaderDynamicAPI has no ShouldWriteDepthToDestAlpha.
                // Leaving this off is the conservative choice: the depth-to-alpha
                // combo is compiled, so it can be re-enabled once an equivalent
                // query is identified on this branch.
                bWriteDepthToAlpha = false;
                bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
                AssertMsg(!(bWriteDepthToAlpha && bWriteWaterFogToAlpha),
                        "Can't write two values to alpha at the same time.");
            }

            PBRSetEyePositionAndEnvMapLOD(pShaderAPI, params[info.envMap]);

            if (bHasLightWarpTexture)
                BindTexture(PBR_SAMPLER_LIGHTWARP, info.lightWarpTexture, 0);

            if (bHasDetailTexture)
            {
                BindTexture(PBR_SAMPLER_DETAIL, info.detailTexture, info.detailFrame);

                // $detailscale is folded into the transform here, so the vertex
                // shader needs no separate scale constant.
                SetVertexShaderTextureScaledTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_2,
                                                      info.detailTextureTransform, info.detailScale);

                PBRSetDetailTint(pShaderAPI, params, info.detailTint, info.detailBlendFactor);
            }

            // c26: x = detail blend mode, yzw = flat metalness/roughness/AO.
            // Those multiply the MRAO sample, so against the white fallback
            // texture they become the values themselves; 1 leaves a real map be.
            float miscConst[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
            if (bHasDetailTexture && info.detailBlendMode != -1)
                miscConst[0] = (float)params[info.detailBlendMode]->GetIntValue();
            if (!bHasMraoTexture)
            {
                if (info.metalness != -1)         miscConst[1] = params[info.metalness]->GetFloatValue();
                if (info.roughness != -1)         miscConst[2] = params[info.roughness]->GetFloatValue();
                if (info.ambientOcclusion != -1)  miscConst[3] = params[info.ambientOcclusion]->GetFloatValue();
            }
            pShaderAPI->SetPixelShaderConstant(PBR_PSREG_MISC, miscConst, 1);

            PBRSetOpenPBRParams(pShaderAPI, params, info.specularIor, info.specularWeight,
                                info.baseDiffuseRoughness, info.specularTint);

            // Setting up dynamic vertex shader
            DECLARE_DYNAMIC_VERTEX_SHADER(pulse_pbr_vs30);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW, pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
            SET_DYNAMIC_VERTEX_SHADER(pulse_pbr_vs30);

            // Setting up dynamic pixel shader
            DECLARE_DYNAMIC_PIXEL_SHADER(pulse_pbr_ps30);
            // The flashlight pass runs no light loop, so it always uses the zero-light combo;
            // the shader skips the others.
            SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, bHasFlashlight ? 0 : lightState.m_nNumLights);
            SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
            SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
            SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
            SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
            SET_DYNAMIC_PIXEL_SHADER(pulse_pbr_ps30);

            // Setting up base texture transform
            SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.baseTextureTransform);




            PBRApplyDebugOverrides(pShaderAPI, params);

            // Sending fog info to the pixel shader
            pShaderAPI->SetPixelShaderFogParams(PSREG_FOG_PARAMS);

            // Modulation color (c1) is set by the helper above.

        }

        // Actually draw the shader
        Draw();
    };

// Closing it off
END_SHADER;
