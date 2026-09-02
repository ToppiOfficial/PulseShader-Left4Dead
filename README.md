# PulseSrcShader

Custom PBR and NPR model shaders for Left 4 Dead 2, shipped as a
`game_shader_generic_*` module. It adds new Pulse shaders and modifies no Valve
file - nothing shipped is overwritten. Models only, ps_3_0 only.

The families:

- **PulsePBR** - OpenPBR Surface (base + specular lobes) for realistic models.
- **PulseGirlsFrontline** - PulsePBR plus opt-in character features (game
  lighting ramp, directional face shading, hair shadow cast, stockings, outline).
- **PulseNPR** - generic cel shader with outline and matcap.
- **PulseUmamusume** - Umamusume character shader with authored lit/shadow masks.
- **PulseToonEye** - toon eyeball shader: the eyerefract iris projection (gaze
  tracks via engine `$eyeorigin`/`$irisu`/`$irisv`) with flat anime shading -
  `$baseshadetexture` shade blend, `$speculartexture` catchlight, `$iriscolor`/
  `$eyewhitecolor` mask tints, cube `$envmap` on the iris, `$selfillum`, and the
  `$eyelid`/`$eyelidblend` read-through-hair overlay. Base map alpha is the iris
  (0) / eyewhite (1) mask.

## How it loads

`CShaderSystem::LoadModShaderDLLs` globs `game_shader_generic*` in
`left4dead2/bin`, appends `.dll`, and asks each hit for `ShaderDLL004`. That is
the whole mechanism - no injection, no hooking, no patched exe. The
extensionless marker file next to the DLL is **required**: the engine appends
`.dll` to whatever the glob returns, so only that name resolves.

The shader names and compiled filenames are all prefixed, so this installs
alongside other shader mods (NekoShaders included) without either clobbering the
other.

## Build

Clone with its pinned SDK submodules:

    git clone --recurse-submodules https://github.com/ToppiOfficial/PulseSourceShader-Left4Dead.git

For an existing clone: `git submodule update --init`. The Valve HLSL headers are
vendored in `sdk/valve-stdshaders/` and not fetched.

Install Visual Studio with the C++ x86 build tools, CMake, the Windows SDK,
Python 3, and Git for Windows, then run `build.bat`. Outputs land in `dist/`.

## Install

Copy the contents of `dist/` into the game folder, keeping the layout. Uninstall
by deleting those files - none of them are Valve files.

## VAC notice

PulseSrcShader is an open-source custom shader module loaded through Source's
shader interface. It does not replace Valve files, inject code, hook engine
functions, or modify gameplay.

Valve does not provide a VAC-safety guarantee for third-party DLLs. Use on
VAC-secured servers at your own risk; `-insecure` is recommended for development
and debugging.

This project exists only because L4D2 still exposes `LoadModShaderDLLs`. If a
game update removes or closes that interface, or Valve takes a position against
third-party shader modules, the project is closed and archived - no workarounds,
no injection, no patched engine binaries.

## Licence

Derived from thexa4/source-pbr and Valve's Source SDK, so it is governed by the
Source 1 SDK License: **free distribution only, no commercial use**. See `NOTICE`
for the full chain of derivation.
