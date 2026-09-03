# PulseShader

<p align="center">
  <img alt="Status: Beta" src="https://img.shields.io/badge/STATUS-BETA-F36D33?style=for-the-badge">
  <img alt="Source Engine branch: L4D2" src="https://img.shields.io/badge/SOURCE_ENGINE_BRANCH-L4D2-0078D4?style=for-the-badge">
  <img alt="DirectX: 9" src="https://img.shields.io/badge/DIRECTX-9-107C10?style=for-the-badge">
</p>

Custom PBR and NPR model shaders for Left 4 Dead 2. PulseShader installs as a
`game_shader_generic_*` module without replacing Valve files. It supports models
and ps_3_0 only.

- **PulsePBR** - OpenPBR Surface for realistic models.
- **PulseGirlsFrontline** - PulsePBR with optional lighting ramps, face shading,
  hair shadows, stockings, and outlines.
- **PulseNPR** - cel shading with outlines and matcaps.
- **PulseUmamusume** - character shading with authored lit and shadow masks.
- **PulseToonEye** - gaze-tracked toon eyes with catchlights and eyelid overlays.

> [!WARNING]
> Valve does not guarantee that third-party DLLs are VAC-safe. Use this mod on
> secure servers at your own risk, and use `-insecure` while testing.

## Install

Download a release, or use your own `dist/` build output, then copy its contents
into your Left 4 Dead 2 folder:

```text
<steam>/steamapps/common/Left 4 Dead 2/
  left4dead2/bin/game_shader_generic_pulse.dll
  left4dead2/bin/game_shader_generic_pulse        (extensionless, required)
  left4dead2/shaders/fxc/pulse_*.vcs
```

Use `left4dead2/bin/` inside the `left4dead2` folder, not the `bin/` beside the
game executable. Both `game_shader_generic_pulse` files are required.

On a clean install, these are all new files. If Windows asks to overwrite
anything, continue only if you are updating an existing PulseShader install.
Otherwise, stop and check the existing file first.

## Uninstall

Delete only these files:

```text
left4dead2/bin/game_shader_generic_pulse.dll
left4dead2/bin/game_shader_generic_pulse
left4dead2/shaders/fxc/pulse_*.vcs
```

Steam's **Verify integrity of game files** does not remove extra mod files, and
reinstalling without deleting the game folder may leave them behind. Delete them
manually. You can run Verify afterward to confirm that Valve's files are intact.

Materials still using a `Pulse*` shader will show the error material until they
are changed back to a stock shader.

## How it works

Source loads `game_shader_generic*` modules from `left4dead2/bin`. The
extensionless file is the name Source discovers; it appends `.dll` and loads
`ShaderDLL004`. PulseShader does not inject or hook code, patch the executable,
replace Valve files, or change gameplay.

All shader and compiled file names use a `pulse_` prefix, so PulseShader can be
installed alongside other shader mods, including NekoShaders. If Valve closes
this interface or rejects third-party shader modules, this project will be
archived rather than switching to injection or patched binaries.

## Build

Clone the repository with its pinned SDK submodules:

```text
git clone --recurse-submodules https://github.com/ToppiOfficial/PulseShader-Left4Dead.git
```

For an existing clone, run `git submodule update --init`. Install Visual Studio
C++ x86 build tools, CMake, the Windows SDK, Python 3, and Git for Windows, then
run `build.bat`. Distributable files are written to `dist/`. The Valve HLSL
headers in `sdk/valve-stdshaders/` are already vendored.

Shader source layout and variant instructions are in
[`src/shaders/README.md`](src/shaders/README.md). Each shader's VMT parameters
are declared at the top of its `src/shaders/*_dx9.cpp` file.

## License

This project derives from thexa4/source-pbr and Valve's Source SDK. It is
governed by the Source 1 SDK License: **free distribution only, no commercial
use**. See [`NOTICE`](NOTICE) for provenance and license details.
