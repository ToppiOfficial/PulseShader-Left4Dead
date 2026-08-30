// Replaces Alien Swarm's shaderlib_cvar.cpp.
//
// A game_shader_generic module is loaded alongside the stock shader DLL, which
// has already registered mat_fullbright, mat_specular and r_flashlightbrightness.
// Valve's version re-registers them unconditionally, which kills L4D2 during
// material-system init. Names the engine already owns are skipped instead.
//
// A skipped ConVar keeps its compiled-in default rather than tracking the
// engine's value. Fine for defaults; switch the shared ones to ConVarRef if
// they need to follow the user's setting.
#include <windows.h>
#include <string.h>
#include "icvar.h"
#include "tier1/tier1.h"
#include "log.h"

#include "tier0/memdbgon.h"

// Off by default: L4D2's ICvar vtable does not match this SDK's icvar.h, so any
// call through g_pCVar lands on the wrong slot and crashes material-system init.
// Measured: FindCommandBase("mat_specular") returned NULL and
// AllocateDLLIdentifier returned a pointer. ICvar derives from IAppSystem, whose
// virtual count differs between branches, which shifts every slot below it.
//
// Our ConVars keep their compiled-in defaults, which is what the shader wants.
// Opt in with -pulse_cvars only to re-test this against a corrected layout.
static bool PulseShaderCVarsEnabled()
{
	const char *pCmd = GetCommandLineA();
	return pCmd && strstr( pCmd, "-pulse_cvars" ) != NULL;
}

class CShaderLibConVarAccessor : public IConCommandBaseAccessor
{
public:
	virtual bool RegisterConCommandBase( ConCommandBase *pCommand )
	{
		const char *pName = pCommand->GetName();

		if ( g_pCVar->FindCommandBase( pName ) )
		{
			PulseLog( "cvar: '%s' already owned by the engine - skipped", pName );
			return true;
		}

		PulseLog( "cvar: registering '%s'...", pName );
		g_pCVar->RegisterConCommand( pCommand );

		const char *pValue = g_pCVar->GetCommandLineValue( pName );
		if ( pValue && !pCommand->IsCommand() )
		{
			( (ConVar *)pCommand )->SetValue( pValue );
		}
		PulseLog( "cvar: '%s' registered", pName );
		return true;
	}
};

CShaderLibConVarAccessor g_ConVarAccessor;

void InitShaderLibCVars( CreateInterfaceFn cvarFactory )
{
	PulseLog( "InitShaderLibCVars: g_pCVar=%p", g_pCVar );
	if ( !g_pCVar )
		return;

	if ( !PulseShaderCVarsEnabled() )
	{
		PulseLog( "InitShaderLibCVars: skipped (ICvar layout mismatch); using defaults" );
		return;
	}

	PulseLog( "ConVar_Register..." );
	ConVar_Register( FCVAR_MATERIAL_SYSTEM_THREAD, &g_ConVarAccessor );
	PulseLog( "InitShaderLibCVars: complete" );
}
