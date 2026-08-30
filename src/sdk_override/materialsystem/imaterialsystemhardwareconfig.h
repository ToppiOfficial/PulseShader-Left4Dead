//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#ifndef IMATERIALSYSTEMHARDWARECONFIG_H
#define IMATERIALSYSTEMHARDWARECONFIG_H

#ifdef _WIN32
#pragma once
#endif


#include "tier1/interface.h"
#include "tier2/tier2.h"

#include "bitmap/imageformat.h"


//-----------------------------------------------------------------------------
// Material system interface version
//-----------------------------------------------------------------------------

// HDRFIXME NOTE: must match common_ps_fxc.h
enum HDRType_t
{
	HDR_TYPE_NONE,
	HDR_TYPE_INTEGER,
	HDR_TYPE_FLOAT,
};

// For now, vertex compression is simply "on or off" (for the sake of simplicity
// and MeshBuilder perf.), but later we may support multiple flavours.
enum VertexCompressionType_t
{
	// This indicates an uninitialized VertexCompressionType_t value
	VERTEX_COMPRESSION_INVALID = 0xFFFFFFFF,

	// 'VERTEX_COMPRESSION_NONE' means that no elements of a vertex are compressed
	VERTEX_COMPRESSION_NONE = 0,

	// Currently (more stuff may be added as needed), 'VERTEX_COMPRESSION_ON' means:
	//  - if a vertex contains VERTEX_ELEMENT_NORMAL, this is compressed
	//    (see CVertexBuilder::CompressedNormal3f)
	//  - if a vertex contains VERTEX_ELEMENT_USERDATA4 (and a normal - together defining a tangent
	//    frame, with the binormal reconstructed in the vertex shader), this is compressed
	//    (see CVertexBuilder::CompressedUserData)
	//  - if a vertex contains VERTEX_ELEMENT_BONEWEIGHTSx, this is compressed
	//    (see CVertexBuilder::CompressedBoneWeight3fv)
	VERTEX_COMPRESSION_ON = 1
};


// use DEFCONFIGMETHOD to define time-critical methods that we want to make just return constants
// on the 360, so that the checks will happen at compile time. Not all methods are defined this way
// - just the ones that I perceive as being called often in the frame interval.
#ifdef _X360
#define DEFCONFIGMETHOD( ret_type, method, xbox_return_value )		\
FORCEINLINE ret_type method const 									\
{																	\
	return xbox_return_value;										\
}


#else
#define DEFCONFIGMETHOD( ret_type, method, xbox_return_value )	\
virtual ret_type method const = 0;
#endif



//-----------------------------------------------------------------------------
// Material system configuration
//-----------------------------------------------------------------------------
// PULSE OVERRIDE: L4D2's vtable carries extra methods the published header
// lacks, so SDK indices are shifted - GetDXSupportLevel() read as 6 (really
// MaxUserClipPlanes), silently forcing the ps20b fallback path. Slots were
// recovered by calling each entry on the live vtable and matching known values
// (dxlevel 100, sampler count 16, 16384 texture dims, 224 ps constants, and the
// sole 2-argument method landing at slot 40). Only what we call is named.
class IMaterialSystemHardwareConfig
{
public:
	virtual void _pulse_pad_00() = 0;
	virtual void _pulse_pad_01() = 0;
	virtual void _pulse_pad_02() = 0;
	virtual void _pulse_pad_03() = 0;
	virtual void _pulse_pad_04() = 0;
	virtual void _pulse_pad_05() = 0;
	virtual void _pulse_pad_06() = 0;
	virtual void _pulse_pad_07() = 0;
	virtual void _pulse_pad_08() = 0;
	virtual void _pulse_pad_09() = 0;
	virtual void _pulse_pad_10() = 0;
	virtual void _pulse_pad_11() = 0;
	virtual void _pulse_pad_12() = 0;
	virtual void _pulse_pad_13() = 0;
	virtual void _pulse_pad_14() = 0;
	virtual void _pulse_pad_15() = 0;
	virtual void _pulse_pad_16() = 0;
	virtual void _pulse_pad_17() = 0;
	virtual int GetDXSupportLevel() const = 0;         // 18 - measured 100
	virtual void _pulse_pad_19() = 0;
	virtual void _pulse_pad_20() = 0;
	virtual void _pulse_pad_21() = 0;
	virtual void _pulse_pad_22() = 0;
	virtual void _pulse_pad_23() = 0;
	virtual void _pulse_pad_24() = 0;
	virtual void _pulse_pad_25() = 0;
	virtual void _pulse_pad_26() = 0;
	virtual void _pulse_pad_27() = 0;
	virtual void _pulse_pad_28() = 0;
	virtual void _pulse_pad_29() = 0;
	virtual void _pulse_pad_30() = 0;
	virtual void _pulse_pad_31() = 0;
	virtual void _pulse_pad_32() = 0;
	virtual void _pulse_pad_33() = 0;
	virtual void _pulse_pad_34() = 0;
	virtual HDRType_t GetHDRType() const = 0;          // 35 - measured 1
	virtual void _pulse_pad_36() = 0;
	virtual void _pulse_pad_37() = 0;
	virtual void _pulse_pad_38() = 0;
	virtual void _pulse_pad_39() = 0;
	virtual void _pulse_pad_40() = 0;
	// Returns ShadowFilterMode_t; typed int as that enum is declared later.
	virtual int GetShadowFilterMode() const = 0;       // 41 - measured 0
	virtual void _pulse_pad_42() = 0;
	virtual bool UsesSRGBCorrectBlending() const = 0;  // 43 - measured 1
	virtual bool HasFastVertexTextures() const = 0;    // 44 - measured 1
	virtual void _pulse_pad_45() = 0;
	virtual void _pulse_pad_46() = 0;
	virtual bool GetHDREnabled() const = 0;            // 47 - inferred
	virtual void _pulse_pad_48() = 0;
	virtual bool SupportsBorderColor() const = 0;      // 49 - inferred

	inline bool SupportsPixelShaders_2_b() const { return GetDXSupportLevel() >= 92; }
	inline bool SupportsShaderModel_3_0() const { return GetDXSupportLevel() >= 95; }
};


#endif // IMATERIALSYSTEMHARDWARECONFIG_H
