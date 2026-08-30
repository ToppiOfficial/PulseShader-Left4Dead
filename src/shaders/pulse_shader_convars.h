#ifndef PULSE_SHADER_CONVARS_H
#define PULSE_SHADER_CONVARS_H

// Debug ConVars shared by every Pulse shader. One definition for the whole DLL:
// a per-file `static ConVar` re-registers the same name once per translation
// unit, which grows with every shader variant added.

#include "tier1/convar.h"

extern ConVar mat_fullbright;
extern ConVar mat_specular;

#endif // PULSE_SHADER_CONVARS_H
