# Pulse shader sources

Descended from thexa4/source-pbr (branch `feature/pbr-base`), distributed
under the Valve Source SDK licence. The BRDF was since rewritten from scratch
against the OpenPBR Surface spec, so no thexa4 code remains here. The Valve
HLSL headers these include are vendored in `sdk/valve-stdshaders/` - do not
edit those.

Two shader families, each a shared base plus sibling variants.

PBR family:

- `pbr_common_dx9.h` - `CPBRShaderBase`, the shared C++ plumbing, and the
  `BEGIN_PBR_SHADER` macro every PBR variant opens with
- `openpbr_common_ps3_x.h` - the OpenPBR Surface BRDF (base and specular lobes
  only), including the `PBR_BRDF_RAMP` hook a variant wires its own lighting
  curves into
- `pbr_dx9.cpp` + `pulse_pbr_{vs,ps}30.fxc` - `PulsePBR`, plain PBR
- `girlsfrontline_dx9.cpp` + `pulse_girlsfrontline_{vs,ps}30.fxc` -
  `PulseGirlsFrontline`, adding `$face`, `$casthairshadow`, `$stocking`, and the outline

Shared by both families:

- `pulse_outline_dx9.h` / `pulse_outline_vs30.h` - inverted-hull outline
- `pulse_shader_convars.h` / `.cpp` - one definition of `mat_fullbright` and
  `mat_specular` for the whole DLL
- `npr_common_dx9.h` / `.cpp` - `CNPRShaderBase`, the shared C++ plumbing, and
  the `BEGIN_NPR_SHADER` macro every NPR variant opens with
- `npr_common_ps3_x.h` - shared NPR HLSL (flashlight, fog, cel step, light attenuation)
- `npr_dx9.cpp` + `pulse_npr_{vs,ps}30.fxc` - `PulseNPR`, the generic cel variant
- `umamusume_dx9.cpp` + `pulse_umamusume_{vs,ps}30.fxc` - `PulseUmamusume`

## Scope

Models only, ps_3_0 only. Both narrowings are deliberate and were applied by
removing code rather than disabling it:

- The lightmapped/brush path is gone, including the `LIGHTMAPPED` combo. Models
  never took it, and a declared-but-skipped combo still consumes static index
  space.
- The shader-model 2b fallback is gone. L4D2 reports dxlevel 100, so it could
  never run.

The NPR shaders follow the same model-only and ps_3_0 scope. They support
opaque, cutout, and translucent character surfaces plus base tint and detail
textures. Eye shaders remain outside its scope.

## Adding a variant

Variants are siblings, never subclasses of each other. `BEGIN_INHERITED_SHADER`
cannot declare `SHADER_PARAM`s, so an inherited variant would have to put its
params on the shared base, where they would collide with every other variant's.
`BEGIN_NPR_SHADER` / `BEGIN_PBR_SHADER` open a fresh namespace instead, giving
each variant its own params, combos, and constant registers.

A new variant is three files and two build lines:

1. `<name>_dx9.cpp` opening with `BEGIN_NPR_SHADER` or `BEGIN_PBR_SHADER`
2. `pulse_<name>_{vs,ps}30.fxc`, seeded by copying the family's base pair
3. `pulse_add_shader_pair(PREFIX pulse_<name> EXTRA_SOURCES <family common>.h)`
   and the `.cpp` added to `add_library` in `CMakeLists.txt`

Do not add a param to a `*_common_dx9.h` to serve one variant - that is the
coupling this layout exists to prevent. Only genuinely engine-facing state that
no variant would change belongs there. The same rule holds in HLSL: a shared
header takes a hook, as `openpbr_common_ps3_x.h` does with `PBR_BRDF_RAMP`, rather
than naming the variant that uses it.

Samplers are fixed family-wide in the common header; the leftover slots are the
variant's, which is where map layouts differ. NPR reserves s0 and s4-s8 and
leaves s1-s3; PBR reserves s0-s7 and s10-s12 and leaves s8-s9.

## Static combos

Adding a STATIC axis renumbers the whole static combo space, so the `.inc` and
the C++ must always be regenerated together - they are, since the `.inc` is a
build output.

If a combo makes other axes meaningless, skip them rather than compiling
duplicates. `pulse_girlsfrontline_ps30.fxc` does this for `OUTLINE`: the hull
reads no material feature, so eight axes are skipped under it and the outline
costs 30 compiled combos instead of half the shader. The C++ must then force
those combos off when it selects the outline pass - requesting a skipped combo
resolves to the wrong compiled shader.

## Naming

The shader registers as `PulsePBR` (from the `BEGIN_VS_SHADER` argument) and
the compiled archives are `pulse_pbr_*.vcs` (from the `.fxc` basenames). Those
are independent - renaming one does not require renaming the other - but both
are prefixed so this coexists with other PBR shader mods.

## Combo ranges

Never narrow a combo define's range to avoid compiling variants. The generated
`.inc` still declares the full radix, so a narrowed build makes every static
index resolve to the wrong shader. Skip the combos instead, or remove the
define entirely as `LIGHTMAPPED` was.
