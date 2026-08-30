# Valve stdshaders HLSL headers

Unmodified Valve Source SDK shader headers, copied here so the shader build has
no submodule to restore. Do not edit them - if one needs changing, the change
belongs in our own header and the divergence gets recorded.

Source: `thexa4/source-pbr`, branch `feature/pbr-base`, commit
`b8c4b76882241ea8cb506e89a61a1f5448d24e71` (2022-12-27), path
`mp/src/materialsystem/stdshaders/`. That tree carried these verbatim from
`ValveSoftware/source-sdk-2013`.

These 8 files are the complete transitive closure of what `src/shaders/*.fxc`
includes - they reference only each other. The four our shaders name directly
are `common_ps_fxc.h`, `common_flashlight_fxc.h`, `common_vs_fxc.h` and
`shader_constant_register_map.h`; the rest arrive through those.

## Not the Alien Swarm versions

`sdk/alienswarm-sdk` ships different copies of all 8 (161 differing lines in
`common_ps_fxc.h`, 400 in `common_flashlight_fxc.h`). These are the SDK-2013
lineage, which is what the shaders were written and tested against.

The two disagree about constant registers, and the engine follows Alien Swarm,
not these. `common_ps_fxc.h` here leaves c32 free; Alien Swarm reserves it for
`cScreenSize` on ps_3_0. **When claiming a new constant register, check the
Alien Swarm copy, not this one.** See `docs/STATE.md`.
