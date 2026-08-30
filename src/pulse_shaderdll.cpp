// Replaces Alien Swarm's shaderlib ShaderDLL.cpp.
//
// Valve's CShaderDLL inherits from both IShaderDLLInternal and IShaderDLL and
// declares two extra virtuals (Connect(factory), Disconnect()) that belong to
// neither. Under L4D2 that produced a second Connect call with a garbage
// factory pointer and killed startup.
//
// The interfaces are kept on separate single-inheritance objects here, so each
// vtable holds exactly the methods its interface declares - the layout our
// probe DLL already validated against a running L4D2.
#include <windows.h>

#include "shaderlib/ShaderDLL.h"
#include "IShaderSystem.h"

// Alien Swarm's IShaderDLLInternal declares 4 methods; L4D2's has 6. The engine
// asks for combo semantics right after Connect and before building the shader
// dictionary, so a 4-slot vtable sends it past the end into garbage. This is
// the 6-method layout our probe DLL validated against the running game.
#undef  PULSE_USE_SDK_SHADERDLLINTERNAL
struct ShaderComboSemantics_t;
abstract_class IPulseShaderDLLInternal
{
public:
	virtual bool Connect( CreateInterfaceFn factory, bool bIsMaterialSystem ) = 0;
	virtual void Disconnect( bool bIsMaterialSystem ) = 0;
	virtual int ShaderCount() const = 0;
	virtual IShader *GetShader( int nShader ) = 0;
	virtual int ShaderComboSemanticsCount() const = 0;
	virtual const ShaderComboSemantics_t *GetComboSemantics( int n ) = 0;
};
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/IShader.h"
#include "tier1/tier1.h"
#include "tier1/utlvector.h"
#include "log.h"

void InitShaderLibCVars( CreateInterfaceFn cvarFactory );

// Consumed by BaseShader.cpp and the shaders themselves.
IMaterialSystemHardwareConfig *g_pHardwareConfig = NULL;
const MaterialSystem_Config_t *g_pConfig = NULL;
IShaderSystem *g_pSLShaderSystem = NULL;

static CUtlVector<IShader *> &ShaderList()
{
	static CUtlVector<IShader *> s_list;
	return s_list;
}

// Shaders self-register through this during static construction, before any
// engine interface exists, so it must not touch anything but the list.
class CPulseShaderDLLPublic : public IShaderDLL
{
public:
	virtual void InsertShader( IShader *pShader )
	{
		if ( !pShader )
			return;
		ShaderList().AddToTail( pShader );
		PulseLog( "InsertShader: registered (count now %d)", ShaderList().Count() );
	}
};

static CPulseShaderDLLPublic s_ShaderDLLPublic;

IShaderDLL *GetShaderDLL()
{
	return &s_ShaderDLLPublic;
}

class CPulseShaderDLLInternal : public IPulseShaderDLLInternal
{
public:
	virtual bool Connect( CreateInterfaceFn factory, bool bIsMaterialSystem )
	{
		PulseLog( "Connect: factory=%p bIsMaterialSystem=%d", factory, (int)bIsMaterialSystem );
		if ( !factory )
			return false;

		g_pHardwareConfig = (IMaterialSystemHardwareConfig *)factory( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, NULL );
		g_pConfig = (const MaterialSystem_Config_t *)factory( MATERIALSYSTEM_CONFIG_VERSION, NULL );
		g_pSLShaderSystem = (IShaderSystem *)factory( SHADERSYSTEM_INTERFACE_VERSION, NULL );
		PulseLog( "Connect: hwcfg=%p cfg=%p shadersys=%p", g_pHardwareConfig, g_pConfig, g_pSLShaderSystem );

		if ( !bIsMaterialSystem )
		{
			ConnectTier1Libraries( &factory, 1 );
			InitShaderLibCVars( factory );
		}

		bool bOK = ( g_pConfig != NULL ) && ( g_pHardwareConfig != NULL ) && ( g_pSLShaderSystem != NULL );
		PulseLog( "Connect: returning %d", (int)bOK );
		return bOK;
	}

	virtual void Disconnect( bool bIsMaterialSystem )
	{
		PulseLog( "Disconnect( %d )", (int)bIsMaterialSystem );
		if ( !bIsMaterialSystem )
			DisconnectTier1Libraries();

		g_pHardwareConfig = NULL;
		g_pConfig = NULL;
		g_pSLShaderSystem = NULL;
	}

	virtual int ShaderCount() const
	{
		PulseLog( "ShaderCount: enter" );
		int n = ShaderList().Count();
		PulseLog( "ShaderCount -> %d", n );
		return n;
	}

	virtual IShader *GetShader( int nShader )
	{
		PulseLog( "GetShader: enter %d", nShader );
		if ( nShader < 0 || nShader >= ShaderList().Count() )
			return NULL;
		IShader *p = ShaderList()[nShader];
		PulseLog( "GetShader( %d ) -> %s", nShader, p ? p->GetName() : "(null)" );
		return p;
	}

	virtual int ShaderComboSemanticsCount() const
	{
		PulseLog( "ShaderComboSemanticsCount -> 0" );
		return 0;
	}

	virtual const ShaderComboSemantics_t *GetComboSemantics( int n )
	{
		PulseLog( "GetComboSemantics( %d ) -> NULL", n );
		return NULL;
	}
};

static CPulseShaderDLLInternal s_ShaderDLLInternal;

IPulseShaderDLLInternal *GetPulseShaderDLLInternal()
{
	return &s_ShaderDLLInternal;
}

extern "C" __declspec(dllexport) void *CreateInterface( const char *pName, int *pReturnCode )
{
	PulseLog( "CreateInterface( \"%s\" )", pName ? pName : "(null)" );

	if ( pName && !strcmp( pName, SHADER_DLL_INTERFACE_VERSION ) )
	{
		if ( pReturnCode ) *pReturnCode = 0;
		return static_cast<IPulseShaderDLLInternal *>( &s_ShaderDLLInternal );
	}

	if ( pReturnCode ) *pReturnCode = 1;
	return NULL;
}

BOOL WINAPI DllMain( HINSTANCE, DWORD reason, LPVOID )
{
	if ( reason == DLL_PROCESS_ATTACH )
	{
		PulseLog( "==== game_shader_generic_pulse (PBR) loaded ====" );
		PulseLog( "shaders registered at load: %d", ShaderList().Count() );
	}
	return TRUE;
}
