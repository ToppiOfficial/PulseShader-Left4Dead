//==================================================================================================
//
// Girls' Frontline character shader: PBR plus four opt-in features - the game's
// own lighting ramp, authored directional face shading, stocking rendering, and
// an inverted-hull outline. Without any of them it renders exactly as PulsePBR.
//
//==================================================================================================

#include "pbr_common_dx9.h"

// Includes for PS30
#include "pulse_girlsfrontline_vs30.inc"
#include "pulse_girlsfrontline_ps30.inc"

const Sampler_t SAMPLER_RAMP = SHADER_SAMPLER8;
const Sampler_t SAMPLER_FACEMAP = SHADER_SAMPLER9;

// Variables for this shader
struct PBR_Vars_t
{
    PBR_Vars_t()
    {
        memset(this, 0xFF, sizeof(*this));
    }

    int baseTexture;
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
    int stockingCenterColor;
    int stockingFalloffColor;
    int stockingFalloffPower;
    int rampTexture;
    int faceMapTexture;
    int faceMapTextureTransform;
    int faceMapSmoothness;
    int noseSpecularStrength;
    int castHairShadow;
    int hairShadowOffset;
    int hairShadowStrength;
    int faceYaw;
    int eyelid;
    int eyelidBlend;
    int faceMode;
    int stockingMode;
    int outlineWidth;
    int outlineAngle;
    int outlineColor;
    int outlineBaseBlend;
};


// Mirrors ShaderStencilState_t from the SDK's ishaderapi.h. Plain POD: the bool
// pads to 4, then four enums, the ref value and two masks.
struct PulseStencilState_t
{
    bool m_bEnable;
    int  m_FailOp;
    int  m_ZFailOp;
    int  m_PassOp;
    int  m_CompareFunc;
    int  m_nReferenceValue;
    unsigned int m_nTestMask;
    unsigned int m_nWriteMask;
};

// D3D values, matching ShaderStencilOp_t / ShaderStencilFunc_t on PC.
enum { PULSE_STENCILOP_KEEP = 1, PULSE_STENCILOP_ZERO = 2, PULSE_STENCILOP_REPLACE = 3 };
enum { PULSE_STENCILFUNC_NEVER = 1, PULSE_STENCILFUNC_EQUAL = 3, PULSE_STENCILFUNC_ALWAYS = 8 };

// IShaderAPI::SetStencilState. The SDK header puts it at 179; L4D2's
// IShaderDynamicAPI is shorter, which shifts every derived slot down to 170.
// Recovered, not assumed - the neighbouring slots' argument counts run
// 4, 20, 0, 8 exactly as the header declares, slot 170 disassembles to a
// one-argument thiscall forwarder, and 171 builds a rect and calls Clear,
// which is ClearStencilBufferRectangle.
static const int PULSE_VT_SETSTENCILSTATE = 170;

// Kill switch for the whole feature. Ceiling: hair behind the head still stamps
// stencil, because Source draws a model's meshes in material order and the face
// has not written depth when the caster runs, so the depth test cannot reject
// it. $hairshadowoffset toward the camera keeps most of it off the face.
#define PULSE_HAIRSHADOW 1

// One bit, so the rest of the stencil buffer stays with whoever else uses it.
static const int PULSE_STENCIL_HAIRSHADOW = 0x40;

static void PulseSetStencilState(IShaderDynamicAPI *pShaderAPI, const PulseStencilState_t &state)
{
    typedef void(__thiscall * SetStencilStateFn)(void *, const PulseStencilState_t *);
    void **vt = *(void ***)pShaderAPI;
    ((SetStencilStateFn)vt[PULSE_VT_SETSTENCILSTATE])(pShaderAPI, &state);
}

static void PulseDisableStencil(IShaderDynamicAPI *pShaderAPI)
{
    PulseStencilState_t off;
    off.m_bEnable = false;
    off.m_FailOp = off.m_ZFailOp = off.m_PassOp = PULSE_STENCILOP_KEEP;
    off.m_CompareFunc = PULSE_STENCILFUNC_ALWAYS;
    off.m_nReferenceValue = 0;
    off.m_nTestMask = off.m_nWriteMask = 0xFFFFFFFF;
    PulseSetStencilState(pShaderAPI, off);
}

// Beginning the shader
BEGIN_PBR_SHADER(PulseGirlsFrontline, "PBR with optional face shading, stocking, and outline")

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
        SHADER_PARAM(OUTLINEWIDTH, SHADER_PARAM_TYPE_FLOAT, "0", "Inverted-hull outline width in model units, 0 disables the outline pass");
        SHADER_PARAM(OUTLINEANGLE, SHADER_PARAM_TYPE_FLOAT, "0", "Minimum view angle for outline expansion in degrees, 0 disables angle fading");
        SHADER_PARAM(OUTLINECOLOR, SHADER_PARAM_TYPE_COLOR, "[0 0 0]", "Outline tint");
        SHADER_PARAM(OUTLINEBASEBLEND, SHADER_PARAM_TYPE_FLOAT, "0", "Base texture contribution to the outline");
        SHADER_PARAM(STOCKING, SHADER_PARAM_TYPE_BOOL, "0", "Render as a stocking: view-angle tint between $stockingcentercolor and $stockingfalloffcolor");
        SHADER_PARAM(STOCKINGCENTERCOLOR, SHADER_PARAM_TYPE_COLOR, "[0.38 0.30 0.28]", "Stocking color facing the camera");
        SHADER_PARAM(STOCKINGFALLOFFCOLOR, SHADER_PARAM_TYPE_COLOR, "[0.06 0.035 0.03]", "Stocking color at grazing angles");
        SHADER_PARAM(STOCKINGFALLOFFPOWER, SHADER_PARAM_TYPE_FLOAT, "1.5", "Stocking view-angle falloff power");
        SHADER_PARAM(DIFFUSERAMPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Girls' Frontline 2 lighting ramp; overrides $lightwarptexture");
        SHADER_PARAM(FACEMAPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Face SDF in R, nose highlight in GB, face mask in A");
        SHADER_PARAM(FACEMAPTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "Face shadow texture coordinate transform");
        SHADER_PARAM(FACEMAPSMOOTHNESS, SHADER_PARAM_TYPE_FLOAT, "0.24", "Face shadow transition width");
        SHADER_PARAM(NOSESPECULARSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1", "Nose highlight strength");
        SHADER_PARAM(FACEYAW, SHADER_PARAM_TYPE_FLOAT, "270", "Degrees the reference pose faces away from +X. 270 is -Y, what these ports export; 0 is the studiomdl convention");
        SHADER_PARAM(EYELID, SHADER_PARAM_TYPE_BOOL, "0", "Draw over the front hair: for eyelash and eyebrow meshes that read through the bangs");
        SHADER_PARAM(EYELIDBLEND, SHADER_PARAM_TYPE_FLOAT, "0.5", "Opacity of the $eyelid overlay where hair covers it, 1 hides the hair outright");
        SHADER_PARAM(FACE, SHADER_PARAM_TYPE_BOOL, "0", "Shade as a face: $facemaptexture drives the terminator and the nose highlight");
        SHADER_PARAM(CASTHAIRSHADOW, SHADER_PARAM_TYPE_BOOL, "0", "Cast this mesh's silhouette into the stencil buffer for $face materials to receive");
        SHADER_PARAM(HAIRSHADOWOFFSET, SHADER_PARAM_TYPE_VEC3, "[0 0 -2]", "World-space displacement of the cast silhouette, in model units");
        SHADER_PARAM(HAIRSHADOWSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.5", "On the $casthairshadow material: how far the shadow darkens the face, 0 is invisible and 1 is black");
    END_SHADER_PARAMS;

    // Needed at shadow-state time, before info is populated.
    int GetDetailBlendMode(IMaterialVar **params) const
    {
        return params[DETAILBLENDMODE]->IsDefined() ? params[DETAILBLENDMODE]->GetIntValue() : 0;
    }

    // $face and $stocking are mutually exclusive - they drive the same lighting
    // path and the pixel shader compiles them as exclusive combos.
    bool IsFaceMaterial(IMaterialVar **params, const PBR_Vars_t &info) const
    {
        return info.faceMode != -1 && params[info.faceMode]->GetIntValue() != 0;
    }

    bool IsStockingMaterial(IMaterialVar **params, const PBR_Vars_t &info) const
    {
        return info.stockingMode != -1 && params[info.stockingMode]->GetIntValue() != 0
            && !IsFaceMaterial(params, info);
    }

    // Setting up variables for this shader
    void SetupVars(PBR_Vars_t &info)
    {
        info.baseTexture = BASETEXTURE;
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
        info.stockingCenterColor = STOCKINGCENTERCOLOR;
        info.stockingFalloffColor = STOCKINGFALLOFFCOLOR;
        info.stockingFalloffPower = STOCKINGFALLOFFPOWER;
        info.rampTexture = DIFFUSERAMPTEXTURE;
        info.faceYaw = FACEYAW;
        info.eyelid = EYELID;
        info.eyelidBlend = EYELIDBLEND;
        info.faceMapTexture = FACEMAPTEXTURE;
        info.faceMapTextureTransform = FACEMAPTEXTURETRANSFORM;
        info.faceMapSmoothness = FACEMAPSMOOTHNESS;
        info.noseSpecularStrength = NOSESPECULARSTRENGTH;
        info.castHairShadow = CASTHAIRSHADOW;
        info.hairShadowOffset = HAIRSHADOWOFFSET;
        info.hairShadowStrength = HAIRSHADOWSTRENGTH;
        info.faceMode = FACE;
        info.stockingMode = STOCKING;
        info.outlineWidth = OUTLINEWIDTH;
        info.outlineAngle = OUTLINEANGLE;
        info.outlineColor = OUTLINECOLOR;
        info.outlineBaseBlend = OUTLINEBASEBLEND;
    };

    // Initializing parameters
    SHADER_INIT_PARAMS()
    {
        PulseLog( "PulseGirlsFrontline: SHADER_INIT_PARAMS enter" );
        // Models-only shader: assert the flag rather than requiring every VMT to
        // carry $model 1. Without it the engine would set this material up for
        // the lightmapped path, which no longer exists in the HLSL.
        SET_FLAGS(MATERIAL_VAR_MODEL);

        SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEWIDTH, 0.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEANGLE, 0.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(OUTLINEBASEBLEND, 0.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(METALNESS, 0.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(ROUGHNESS, 1.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(AMBIENTOCCLUSION, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARIOR, 1.5f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(SPECULARWEIGHT, 1.0f);
		SET_PARAM_FLOAT_IF_NOT_DEFINED(BASEDIFFUSEROUGHNESS, 0.0f);
		if (!params[SPECULARTINT]->IsDefined())
			params[SPECULARTINT]->SetVecValue(1.0f, 1.0f, 1.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(FACEMAPSMOOTHNESS, 0.24f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(NOSESPECULARSTRENGTH, 1.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(EYELIDBLEND, 0.5f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(FACEYAW, 270.0f);
        SET_PARAM_FLOAT_IF_NOT_DEFINED(HAIRSHADOWSTRENGTH, 0.5f);

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
        PulseLog( "PulseGirlsFrontline: SHADER_FALLBACK" );
        return 0;
    };

    SHADER_INIT
    {
        PulseLog( "PulseGirlsFrontline: SHADER_INIT enter" );
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

        if (params[info.rampTexture]->IsDefined())
            LoadTexture(info.rampTexture);

        if (IsFaceMaterial(params, info) && params[info.faceMapTexture]->IsDefined())
            LoadTexture(info.faceMapTexture);

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
        bool bHasSpecularTexture = (info.specularTexture != -1) && params[info.specularTexture]->IsTexture();
        bool bHasDetailTexture = (info.detailTexture != -1) && params[info.detailTexture]->IsTexture();
        bool bHasLightWarpTexture = (info.lightWarpTexture != -1) && params[info.lightWarpTexture]->IsTexture();
        bool bStocking = IsStockingMaterial(params, info);
        // The inverted hull draws behind the model, so a $translucent or
        // $alphatest surface blends the outline through itself instead of
        // hiding it. No sort order fixes that, so the pass is locked off.
        // An eyelid hull would outline the lashes against the hair they draw over.
        bool bEyelid = params[info.eyelid]->GetIntValue() != 0;
        bool bHasRampTexture = params[info.rampTexture]->IsTexture();
        bool bFace = IsFaceMaterial(params, info) && params[info.faceMapTexture]->IsTexture();

        // Determining whether we're dealing with a fully opaque material
        BlendType_t nBlendType = EvaluateBlendRequirements(info.baseTexture, true);
        bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested;
        bool bOutlineEnabled = !bHasFlashlight && !bEyelid && bFullyOpaque
            && params[info.outlineWidth]->GetFloatValue() > 0.0f;

        // Pass 2 is the stencil hair shadow. The caster re-draws its own mesh
        // displaced, writing only stencil; the receiver draws a tint through a
        // stencil test. Both use the same flat HAIRSHADOWPASS shader.
        // The flashlight pass redraws the whole model, so without this the drawer
        // multiplies the face down a second time and the shadow doubles up.
        bool bCastHairShadow = PULSE_HAIRSHADOW && !bHasFlashlight
            && params[info.castHairShadow]->GetIntValue() != 0;
        bool bMarkHairShadow = PULSE_HAIRSHADOW && !bHasFlashlight && bFace;

        // Three passes is the ceiling - a fourth snapshot takes L4D2's shader system
        // down - so the lash overlay shares the post-main slot with the hair shadow.
        // A mesh is one or the other, and $eyelid claims it.
        for (int pass = 0; pass < 3; ++pass)
        {
        bool bOutline = pass == 0;
        // The lash draws normally in pass 1 and again here, pushed toward the camera
        // and blended. Where hair covers it the blend is against the hair; where
        // nothing does it is against the lash pass 1 already drew, so nothing changes.
        bool bEyelidPass = pass == 2 && bEyelid;
        bool bHairShadow = pass == 2 && !bEyelid;
        // The face marks its own visible pixels in stencil; the hair then draws the
        // shadow onto them. Both happen in one frame, and by the time the hair runs
        // the face has written depth, so strands behind the skull fail and are rejected.
        bool bHairShadowDraw = bHairShadow && bCastHairShadow;
        bool bHairShadowMark = bHairShadow && !bCastHairShadow && bMarkHairShadow;
        bool bHasRamp = bHasRampTexture && !bOutline && !bHairShadow;
        if (bOutline && !bOutlineEnabled)
        {
            Draw(false);
            continue;
        }
        if (bHairShadow && !bHairShadowDraw && !bHairShadowMark)
        {
            Draw(false);
            continue;
        }

        if (IsSnapshotting())
        {
            // The outline draws the back faces of the expanded hull.
            pShaderShadow->EnableCulling(!bOutline && !IS_FLAG_SET(MATERIAL_VAR_NOCULL));

            if (bHairShadow)
            {
                // Both leave depth alone; the face is already at the right depth.
                // Only the drawer writes colour - the marker just stamps stencil.
                pShaderShadow->EnableDepthWrites(false);
                pShaderShadow->EnableColorWrites(bHairShadowDraw);
                pShaderShadow->EnableAlphaWrites(false);
            }

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

            // Multiplies the face down rather than replacing it, so the shadow keeps
            // the skin's own colour. Set after the default blend state overwrites it.
            if (bHairShadowDraw)
            {
                pShaderShadow->EnableBlending(true);
                pShaderShadow->BlendFunc(SHADER_BLEND_DST_COLOR, SHADER_BLEND_ZERO);
            }

            // After the default blend state, which would otherwise overwrite these.
            if (bEyelidPass)
            {
                pShaderShadow->EnableDepthWrites(false);
                pShaderShadow->EnableBlending(true);
                pShaderShadow->BlendFunc(SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA);
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

            if (bHasRamp)
            {
                pShaderShadow->EnableTexture(SAMPLER_RAMP, true);
                pShaderShadow->EnableSRGBRead(SAMPLER_RAMP, false);
            }

            if (bFace)
            {
                pShaderShadow->EnableTexture(SAMPLER_FACEMAP, true);
                pShaderShadow->EnableSRGBRead(SAMPLER_FACEMAP, false);
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
            DECLARE_STATIC_VERTEX_SHADER(pulse_girlsfrontline_vs30);
            SET_STATIC_VERTEX_SHADER_COMBO(FACE, !bOutline && !bHairShadow && bFace);
            SET_STATIC_VERTEX_SHADER_COMBO(OUTLINE, bOutline);
            SET_STATIC_VERTEX_SHADER_COMBO(HAIRSHADOWPASS, bHairShadow);
            SET_STATIC_VERTEX_SHADER_COMBO(EYELID, bEyelidPass);
            SET_STATIC_VERTEX_SHADER(pulse_girlsfrontline_vs30);

            // Setting up static pixel shader
            DECLARE_STATIC_PIXEL_SHADER(pulse_girlsfrontline_ps30);
            SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, !bHairShadow && bHasFlashlight);
            SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode);
            // The outline hull reads no material feature, and the .fxc skips
            // those combos under it. Requesting one that was skipped would
            // resolve to the wrong compiled shader.
            SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, !bOutline && !bHairShadow && bHasEmissionTexture);
            SET_STATIC_PIXEL_SHADER_COMBO(SPECULAR, !bOutline && !bHairShadow && bHasSpecularTexture);
            SET_STATIC_PIXEL_SHADER_COMBO(DETAILTEXTURE, !bOutline && !bHairShadow && bHasDetailTexture);
            SET_STATIC_PIXEL_SHADER_COMBO(LIGHTWARPTEXTURE,
                    bHasRamp ? 2 : ((!bOutline && !bHairShadow && bHasLightWarpTexture) ? 1 : 0));
            SET_STATIC_PIXEL_SHADER_COMBO(STOCKING, !bOutline && !bHairShadow && bStocking);
            SET_STATIC_PIXEL_SHADER_COMBO(FACE, !bOutline && !bHairShadow && bFace);
            SET_STATIC_PIXEL_SHADER_COMBO(OUTLINE, bOutline);
            SET_STATIC_PIXEL_SHADER_COMBO(HAIRSHADOWPASS, bHairShadow);
            SET_STATIC_PIXEL_SHADER(pulse_girlsfrontline_ps30);

            // Setting up fog
            DefaultFog(); // I think this is correct

            // HACK HACK HACK - enable alpha writes all the time so that we have them for underwater stuff
            // The hair-shadow passes masked writes off above; underwater alpha is
            // the main pass's job, and the marker has its colour writes masked.
            pShaderShadow->EnableAlphaWrites(bFullyOpaque && !bHairShadow);
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

            float color[4] = {1.f, 1.f, 1.f, IS_FLAG_SET(MATERIAL_VAR_HALFLAMBERT) ? 1.f : 0.f};
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

            if (bHasRamp)
                BindTexture(SAMPLER_RAMP, info.rampTexture, 0);

            if (bFace)
                BindTexture(SAMPLER_FACEMAP, info.faceMapTexture, 0);

            if (bStocking)
            {
                float center[4] = { 0.38f, 0.30f, 0.28f, 1.5f };
                float falloff[4] = { 0.06f, 0.035f, 0.03f, 0.0f };
                params[info.stockingCenterColor]->GetVecValue(center, 3);
                params[info.stockingFalloffColor]->GetVecValue(falloff, 3);
                center[3] = MAX(params[info.stockingFalloffPower]->GetFloatValue(), 0.001f);
                pShaderAPI->SetPixelShaderConstant(3, center, 1);
                pShaderAPI->SetPixelShaderConstant(19, falloff, 1);
            }

            else if (bFace)
            {
                float faceParams[4] = {
                    MAX(params[info.faceMapSmoothness]->GetFloatValue(), 0.0001f),
                    MAX(params[info.noseSpecularStrength]->GetFloatValue(), 0.0f),
                    0.0f, 0.0f
                };
                pShaderAPI->SetPixelShaderConstant(19, faceParams, 1);
                SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_4,
                                                info.faceMapTextureTransform);
            }

            if (bHasDetailTexture)
            {
                BindTexture(PBR_SAMPLER_DETAIL, info.detailTexture, info.detailFrame);

                // $detailscale is folded into the transform here, so the vertex
                // shader needs no separate scale constant.
                SetVertexShaderTextureScaledTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_2,
                                                      info.detailTextureTransform, info.detailScale);

                PBRSetDetailTint(pShaderAPI, params, info.detailTint, info.detailBlendFactor);
            }

            if (bOutline)
            {
                PulseSetOutlineConstants(pShaderAPI, VERTEX_SHADER_SHADER_SPECIFIC_CONST_6,
                                         params[info.outlineWidth]->GetFloatValue(),
                                         params[info.outlineAngle]->GetFloatValue());

                float outlineColor[4] = { 0.0f, 0.0f, 0.0f,
                    MIN(MAX(params[info.outlineBaseBlend]->GetFloatValue(), 0.0f), 1.0f) };
                params[info.outlineColor]->GetVecValue(outlineColor, 3);
                pShaderAPI->SetPixelShaderConstant(46, outlineColor, 1);
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

            if (bHairShadow)
            {
                // Only the drawer is displaced; the marker stamps where the face is.
                float offset[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                if (bHairShadowDraw)
                    params[info.hairShadowOffset]->GetVecValue(offset, 3);
                pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, offset, 1);

                // Multiplied into the face, so 1 leaves it alone and 0 takes it to
                // black. The marker writes no colour, so its value is irrelevant.
                float shadowColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                if (bHairShadowDraw)
                {
                    float k = MIN(MAX(params[info.hairShadowStrength]->GetFloatValue(), 0.0f), 1.0f);
                    shadowColor[0] = shadowColor[1] = shadowColor[2] = 1.0f - k;
                }
                pShaderAPI->SetPixelShaderConstant(34, shadowColor, 1);
            }

            if (bFace)
            {
                // Bone matrices map reference-pose model space to world, so the head's
                // live basis comes from skinning these. A reference pose is upright, so
                // up is +Z and only the facing needs stating.
                float yaw = params[info.faceYaw]->GetFloatValue() * (3.14159265f / 180.0f);
                float refForward[4] = { cosf(yaw), sinf(yaw), 0.0f, 0.0f };
                float refRight[4] = { sinf(yaw), -cosf(yaw), 0.0f, 0.0f };
                pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, refForward);
                pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_9, refRight);
            }

            float eyelidBlend[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
            if (bEyelidPass)
                eyelidBlend[0] = MIN(MAX(params[info.eyelidBlend]->GetFloatValue(), 0.0f), 1.0f);
            pShaderAPI->SetPixelShaderConstant(36, eyelidBlend, 1);

            PBRSetOpenPBRParams(pShaderAPI, params, info.specularIor, info.specularWeight,
                                info.baseDiffuseRoughness, info.specularTint);

            // Setting up dynamic vertex shader
            DECLARE_DYNAMIC_VERTEX_SHADER(pulse_girlsfrontline_vs30);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW, pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
            SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
            SET_DYNAMIC_VERTEX_SHADER(pulse_girlsfrontline_vs30);

            // Setting up dynamic pixel shader
            DECLARE_DYNAMIC_PIXEL_SHADER(pulse_girlsfrontline_ps30);
            // The flashlight pass runs no light loop, so it always uses the zero-light combo;
            // the shader skips the others.
            SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, bHasFlashlight ? 0 : lightState.m_nNumLights);
            SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
            SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
            SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
            SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
            SET_DYNAMIC_PIXEL_SHADER(pulse_girlsfrontline_ps30);

            // Setting up base texture transform
            SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.baseTextureTransform);




            PBRApplyDebugOverrides(pShaderAPI, params);

            // Sending fog info to the pixel shader
            pShaderAPI->SetPixelShaderFogParams(PSREG_FOG_PARAMS);

            // Modulation color (c1) is set by the helper above.

        }

        // The caster stamps PULSE_STENCIL_HAIRSHADOW, the receiver draws only where
        // that bit stands. Confined to one bit so L4D2's own stencil users - the
        // survivor glow among them - keep the rest of the buffer.
        // A participating mesh's main pass clears the bit as well, so anything
        // drawn over the face un-marks it and the shadow lands on skin the hair
        // has not covered. The face's own marker runs in pass 2, after its clear.
        // Materials in neither role never touch stencil: setting and then
        // blanket-disabling it wipes the interior mask L4D2's survivor glow
        // writes there, and the whole model fills solid with the glow colour.
        bool bMainPass = pass == 1;
        bool bStencilPass = !IsSnapshotting()
            && (bHairShadow || (bMainPass && (bFace || bCastHairShadow)));
        if (bStencilPass)
        {
            // One shot each: enough to tell a pass that never runs from a stencil
            // bit that does not survive.
            static bool s_bLoggedDraw = false, s_bLoggedMark = false;
            if (bHairShadowDraw && !s_bLoggedDraw)
            {
                s_bLoggedDraw = true;
                PulseLog("hairshadow: DRAWER running, strength=%.2f",
                         params[info.hairShadowStrength]->GetFloatValue());
            }
            if (bHairShadowMark && !s_bLoggedMark)
            {
                s_bLoggedMark = true;
                PulseLog("hairshadow: MARKER running");
            }

            // Only a fragment that passes both tests touches the bit. Clearing on
            // ZFail instead would let a hidden fragment wipe a stamp a visible one
            // just made - overlapping hair strands on the caster, and the inner
            // mouth behind the face on the receiver, both punch holes that way.
            PulseStencilState_t st;
            st.m_bEnable = true;
            st.m_FailOp = st.m_ZFailOp = PULSE_STENCILOP_KEEP;
            st.m_nReferenceValue = PULSE_STENCIL_HAIRSHADOW;
            st.m_nTestMask = PULSE_STENCIL_HAIRSHADOW;
            st.m_nWriteMask = PULSE_STENCIL_HAIRSHADOW;
            if (bMainPass)
            {
                st.m_CompareFunc = PULSE_STENCILFUNC_ALWAYS;
                st.m_PassOp = PULSE_STENCILOP_ZERO;
            }
            else if (bHairShadowMark)
            {
                st.m_CompareFunc = PULSE_STENCILFUNC_ALWAYS;
                st.m_PassOp = PULSE_STENCILOP_REPLACE;
            }
            else
            {
                // Consume the bit as it is read, so a second hair mesh cannot darken
                // the same pixel twice.
                st.m_CompareFunc = PULSE_STENCILFUNC_EQUAL;
                st.m_PassOp = PULSE_STENCILOP_ZERO;
            }
            PulseSetStencilState(pShaderAPI, st);
        }

        // Actually draw the shader
        Draw();

        // Never leave stencil on - the engine's own passes share this state.
        if (bStencilPass)
            PulseDisableStencil(pShaderAPI);
        }
    };

// Closing it off
END_SHADER;
