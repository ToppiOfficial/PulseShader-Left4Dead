# PulseShader

Custom PBR and NPR model shaders for Left 4 Dead 2, shipped as a
`game_shader_generic_*` module. It adds new Pulse shaders and modifies no Valve
file - nothing shipped is overwritten. Models only, ps_3_0 only.

The families:

- **PulsePBR** - OpenPBR Surface (base + specular lobes) for realistic models.
- **PulseGirlsFrontline** - PulsePBR plus opt-in character features (lighting
  ramp, face shading, hair shadow cast, stockings, outline).
- **PulseNPR** - generic cel shader with outline and matcap.
- **PulseUmamusume** - Umamusume character shader with authored lit/shadow masks.
- **PulseToonEye** - toon eyeball: eyerefract's gaze-tracked iris projection
  with flat anime shading, catchlight, and an eyelid overlay.

`src/shaders/README.md` covers the source layout and how to add a variant. Each
shader's VMT params are declared at the top of its `src/shaders/*_dx9.cpp`.

## Is this safe? (VAC)

It loads through Source's own shader interface: no injection, no hooking, no
patched exe, no Valve file replaced, no gameplay change. Nothing here does what
VAC looks for.

That said, Valve gives no VAC-safety guarantee for any third-party DLL, so
using it on secure servers is your call. Use `-insecure` while developing.

If a game update ever closes that shader interface, or Valve takes a position
against third-party shader modules, this project is archived - no workarounds,
no injection, no patched binaries.

## How to install - READ THIS

Grab a release (or your own `build.bat` output) and copy the contents of
`dist/` into your Left 4 Dead 2 folder, keeping the layout:

    <steam>/steamapps/common/Left 4 Dead 2/
      left4dead2/bin/game_shader_generic_pulse.dll
      left4dead2/bin/game_shader_generic_pulse      <- extensionless, required
      left4dead2/shaders/fxc/pulse_*.vcs

Note the path: `left4dead2/bin/`, **inside** the `left4dead2` folder. The
`bin/` next to the exe at the game root is the engine/SDK one (hammer,
studiomdl, ...) - nothing goes there.

Both files in `bin/` must be there. The extensionless one is not a mistake and
is not a leftover - the engine will not load the shaders without it.

## How to uninstall

Delete exactly those files:

    left4dead2/bin/game_shader_generic_pulse.dll
    left4dead2/bin/game_shader_generic_pulse
    left4dead2/shaders/fxc/pulse_*.vcs

That is the whole uninstall - nothing else was touched, and any VMT still
naming a `Pulse*` shader falls back to the game's error material until you
point it at a stock shader again.

**"Verify integrity of game files" does not remove this.** Steam only restores
missing or altered Valve files; these are extra files it does not know about,
so it leaves them exactly where they are. Same for reinstalling the game
without deleting the folder. Delete the files by hand.

Once you have deleted them, running Verify is still worth doing - it will not
remove anything of ours, but it confirms your Valve files are intact and rules
this mod out if you are chasing an unrelated problem.

> **If file explorer/Windows asks you to overwrite anything on a fresh install, stop.**
> This project ships zero Valve files. On a clean game folder every file above
> is new, so an overwrite prompt means one of two things: you already have a
> copy of PulseShader installed (fine - replacing it is the update path), or
> something else on your system is using these exact names, which is a red
> flag. Do not click through it. Verify what is already there first, and if you
> did not put it there, open an issue.

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

## Licence

Derived from thexa4/source-pbr and Valve's Source SDK, so it is governed by the
Source 1 SDK License: **free distribution only, no commercial use**. See `NOTICE`
for the full chain of derivation.
