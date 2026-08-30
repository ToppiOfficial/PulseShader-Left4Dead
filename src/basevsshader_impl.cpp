// The three CBaseVSShader methods our PBR shader actually calls, plus the
// ConVar declared alongside them.
//
// Alien Swarm's BaseVSShader.cpp cannot be compiled as-is: it includes six
// generated .inc headers (flashlight_ps30, lightmappedgeneric_flashlight_vs30,
// vertexlit_and_unlit_generic_hdr_ps20/b, ...) whose .fxc sources ship in no
// public SDK. Those serve DrawFlashlight_dx90, which PBR never calls - it
// handles the flashlight inside its own pixel shader. Implementations are
// Valve's, unchanged.
//
// If a future feature needs more of CBaseVSShader, add the method here rather
// than trying to build the original file.
#include "BaseVSShader.h"
#include "shaderlib/cshader.h"
#include "shaderlib/baseshader.h"
#include "ConVar.h"
#include <renderparm.h>

ConVar r_flashlightbrightness( "r_flashlightbrightness", "0.25", FCVAR_CHEAT );

void CBaseVSShader::SetHWMorphVertexShaderState( int nDimConst, int nSubrectConst, VertexTextureSampler_t morphSampler )
{
	if ( !s_pShaderAPI->IsHWMorphingEnabled() )
		return;

	int nMorphWidth, nMorphHeight;
	s_pShaderAPI->GetStandardTextureDimensions( &nMorphWidth, &nMorphHeight, TEXTURE_MORPH_ACCUMULATOR );

	int nDim = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_4TUPLE_COUNT );
	float pMorphAccumSize[4] = { (float)nMorphWidth, (float)nMorphHeight, (float)nDim, 0.0f };
	s_pShaderAPI->SetVertexShaderConstant( nDimConst, pMorphAccumSize );

	int nXOffset = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_X_OFFSET );
	int nYOffset = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_Y_OFFSET );
	int nWidth = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_SUBRECT_WIDTH );
	int nHeight = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_SUBRECT_HEIGHT );
	float pMorphAccumSubrect[4] = { (float)nXOffset, (float)nYOffset, (float)nWidth, (float)nHeight };
	s_pShaderAPI->SetVertexShaderConstant( nSubrectConst, pMorphAccumSubrect );

	s_pShaderAPI->BindStandardVertexTexture( morphSampler, TEXTURE_MORPH_ACCUMULATOR );
}

void CBaseVSShader::SetVertexShaderTextureTransform( int vertexReg, int transformVar )
{
	Vector4D transformation[2];
	IMaterialVar *pTransformationVar = s_ppParams[transformVar];
	if ( pTransformationVar && ( pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX ) )
	{
		const VMatrix &mat = pTransformationVar->GetMatrixValue();
		transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
		transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
	}
	else
	{
		transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
		transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
	}
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 );
}

BlendType_t CBaseVSShader::EvaluateBlendRequirements( int textureVar, bool isBaseTexture,
													  int detailTextureVar )
{
	// Either we've got a constant modulation
	bool isTranslucent = IsAlphaModulating();

	// Or we've got a vertex alpha
	isTranslucent = isTranslucent || ( CurrentMaterialVarFlags() & MATERIAL_VAR_VERTEXALPHA );

	// Or we've got a texture alpha (for blending or alpha test)
	isTranslucent = isTranslucent || ( TextureIsTranslucent( textureVar, isBaseTexture ) &&
									   !( CurrentMaterialVarFlags() & MATERIAL_VAR_ALPHATEST ) );

	if ( ( detailTextureVar != -1 ) && ( !isTranslucent ) )
	{
		isTranslucent = TextureIsTranslucent( detailTextureVar, isBaseTexture );
	}

	if ( CurrentMaterialVarFlags() & MATERIAL_VAR_ADDITIVE )
	{
		return isTranslucent ? BT_BLENDADD : BT_ADD;	// Additive
	}
	else
	{
		return isTranslucent ? BT_BLEND : BT_NONE;		// Normal blending
	}
}

void CBaseVSShader::HashShadow2DJitter( const float fJitterSeed, float *fU, float *fV )
{
	int nTexWidth, nTexHeight;
	s_pShaderAPI->GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

	int nSeed = fmod( fJitterSeed, 1.0f ) * nTexWidth * nTexHeight;

	int nRow = nSeed / nTexHeight;
	int nCol = nSeed % nTexWidth;

	// Div and mod to get an individual texel in the fTexRes x fTexRes grid
	*fU = nRow / (float)nTexHeight;	// Row
	*fV = nCol / (float)nTexWidth;	// Column
}

void CBaseVSShader::SetVertexShaderTextureScaledTransform( int vertexReg, int transformVar, int scaleVar )
{
	Vector4D transformation[2];
	IMaterialVar *pTransformationVar = s_ppParams[transformVar];
	if ( pTransformationVar && ( pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX ) )
	{
		const VMatrix &mat = pTransformationVar->GetMatrixValue();
		transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
		transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
	}
	else
	{
		transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
		transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
	}

	Vector2D scale( 1, 1 );
	IMaterialVar *pScaleVar = s_ppParams[scaleVar];
	if ( pScaleVar )
	{
		if ( pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR )
			pScaleVar->GetVecValue( scale.Base(), 2 );
		else if ( pScaleVar->IsDefined() )
			scale[0] = scale[1] = pScaleVar->GetFloatValue();
	}

	// Apply the scaling
	transformation[0][0] *= scale[0];
	transformation[0][1] *= scale[1];
	transformation[1][0] *= scale[0];
	transformation[1][1] *= scale[1];
	transformation[0][3] *= scale[0];
	transformation[1][3] *= scale[1];
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 );
}

void CBaseVSShader::SetPixelShaderConstantGammaToLinear( int pixelReg, int constantVar )
{
	Assert( !IsSnapshotting() );
	if ( ( !s_ppParams ) || ( constantVar == -1 ) )
		return;

	IMaterialVar *pPixelVar = s_ppParams[constantVar];
	Assert( pPixelVar );

	float val[4];
	if ( pPixelVar->GetType() == MATERIAL_VAR_TYPE_VECTOR )
		pPixelVar->GetVecValue( val, 4 );
	else
		val[0] = val[1] = val[2] = val[3] = pPixelVar->GetFloatValue();

	val[0] = val[0] > 1.0f ? val[0] : GammaToLinear( val[0] );
	val[1] = val[1] > 1.0f ? val[1] : GammaToLinear( val[1] );
	val[2] = val[2] > 1.0f ? val[2] : GammaToLinear( val[2] );

	s_pShaderAPI->SetPixelShaderConstant( pixelReg, val );
}
