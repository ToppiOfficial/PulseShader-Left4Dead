# Contributing

PRs are welcome for two things: fixing an existing shader, or adding a new one.

## Hard rules

- **Nothing that breaks fair play.** No wallhacks, no depth-test or
  `$ignorez`-style see-through-geometry tricks, no chams, glow, or outline
  applied to make players/infected visible through walls, no ESP of any kind.
  This applies even if it is "just for singleplayer" or gated behind a param.
  Such a PR is closed, no discussion.
- **No new DLLs, no edits to engine DLLs.** Everything ships inside
  `game_shader_generic_pulse.dll`. A PR that adds another binary, patches a
  Valve DLL, hooks the engine, or injects code will be closed.
- **Use Source's standard shader plumbing.** Params, combos, snapshot/dynamic
  state and command buffers go through the normal `CBaseShader` /
  `IShaderShadow` / `IShaderDynamicAPI` path, same as the existing shaders.
  Follow `src/shaders/README.md` for the layout: a variant is a sibling
  (`BEGIN_PBR_SHADER` / `BEGIN_NPR_SHADER`), never a subclass of another
  variant.
- **No Valve file is overwritten.** Install stays "copy `dist/` in, delete to
  uninstall".
- **Keep the VMT surface small.** A param has to earn its place. One-off knobs
  that fine-tune a single number are not acceptable in a merged shader - fold
  the value into the shader, derive it from an existing param, or reuse a stock
  Source one (`$color`, `$phongboost`, ...) instead of inventing a new name.
  Temporary tuning params are fine while you develop: get the value right, hard
  code it, and delete the param before the PR.

## Soft rules

- **Use Valve's names.** If Source already has a param for the thing, use it:
  `$basetexture`, not `$colortexture` or `$diffusetexture`; `$bumpmap`, not
  `$normaltexture`. Only invent a name when nothing stock covers it, and then
  keep it short and conventional - `$mraotexture` is the shape to copy.
- **Reuse before rewriting.** If the lighting you need already exists in the
  PBR (`openpbr_common_ps3_x.h`) or NPR (`npr_common_ps3_x.h`) base, build on
  it instead of writing another BRDF or toon model from scratch. A new variant
  should mostly be its own params and its own look, not its own copy of the
  shading. If the base almost fits, add a hook to it (as `PBR_BRDF_RAMP` does)
  rather than forking it - and never add a param to a `*_common_dx9.h` to serve
  one variant.

## Required testing before you open a PR

1. `build.bat` completes clean, and the DLL timestamp actually updated - a
   failed build leaves the old DLL in place.
2. **Live dedicated-server compatibility test.** Join an official Valve or
   community dedicated server with the shader installed and play long enough
   to confirm it loads, renders, and causes no file-consistency kick.
   `-insecure` is fine for iteration but not for the final check.
   - Test with **no other mods installed**. Custom test *models* are fine;
     other addons are not.
   - Test on the **current retail version** of L4D2. Results from an older
     build do not count.
3. **Interaction with the game's own effects.** If your shader (or your change
   to one) breaks a stock engine effect on the model - the glow outline, an
   effect a scripted event or map logic triggers, burning, freeze, or any
   material proxy/override the game applies - say so in the PR. State whether
   it is a `FIXME` you intend to come back to or something the shader cannot
   support, and note it in the shader's README section so users are not
   surprised. An undocumented break is a rejected PR; a documented one is
   usually fine.
4. **No regressions on existing models.** Your change must not break models
   that already work - mine, yours, or ones published on the Steam Workshop.
   If you touch a shared header (`*_common_dx9.h`, `*_common_ps3_x.h`,
   `pulse_outline_*`), check every variant that includes it, not just the one
   you care about.

## What to put in the PR

- What it looks like to the user (fixed X, added shader Y).
- The VMT params it adds or changes.
- Build + in-game checks you ran, including the dedicated-server test and game
  version.
- Before/after screenshots for anything visual.
- Which existing shaders/models you re-checked for regressions.

Commits: short imperative summaries, one focused change each. Do not commit
`build/`, `dist/`, game files, or `.reference/` material.
