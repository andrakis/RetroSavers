# RetroSavers — Backlog

Prioritised list of screen savers still to build, drawn from three families: the remaining
Windows built-ins, other-platform classics (Amiga / Mac / After Dark / xscreensaver), and
internet-famous savers. Work through the tiers in order; tick a saver off (and add its README
row) when it ships.

Rules that apply to everything below:

- **No third-party code — Windows SDK only.** DirectWrite, Direct2D geometry, WIC and
  `IFileDialog` are all SDK and fair game; nothing else is.
- Anything built around a real trademark (Windows flag, DVD logo, toasters) uses a **generic
  procedurally drawn stand-in** with a user-supplied image override in its settings.
- Every saver follows the boilerplate checklist at the bottom of this file.

---

## Shared Core additions

Built once; listed in the order Tier 1 needs them. Tier 1 needs C1, C2, C3, C9, C7, C10
(+ C11 for nicer text). C4–C6 and C8 are pulled in by Tier 2/3.

| # | Addition | What | Unlocks | Done |
|---|---|---|---|---|
| C1 | `Core\Gfx\SpriteBatch2D` + `Sprite2D_vs/ps.hlsl` | Generalises Starfield's SV_VertexID instanced quads. Instance `{pos, size, uvRect, color, rotation}` in a `DynamicVertexBuffer<>`; `Begin(w,h)` / `Push()` / `End(device, texture-or-white, blend, psOverride)`. | Matrix, DVD, Flying Windows, Marquee, Fireworks, Flurry, Toasters, Starry Night, Lorenz, Bubbles | [x] |
| C2 | `Core\Gfx\GlyphAtlas` + `TextureFactory::TextImage` | GDI text drawn white-on-black, luma→alpha (fixes the magenta-key aliasing). Atlas = grid of glyphs + `Find(wchar_t)` → uvRect/advance. `TextImage(text, LOGFONTW, colour)` for whole strings. | Matrix, Marquee, DVD fallback, corner counter, Photos captions | [x] |
| C3 | `Core\Gfx\TrailBuffer` + `Feedback_ps.hlsl`, `PostProcess::Fill` | Mystify's persistent target made reusable, on fp16 (8-bit fade never reaches black). `Begin(fade, zoom, blur)` fades in place or ping-pongs through a feedback PS; `Present()` copies to the swap chain. | Fireworks, Flurry, Ribbons, Energy, Warp, Lorenz | [x] |
| C9 | `DialogUtil`: `PickFont`, `PickImageFile`, `PickFolder`; `Host\FontSettings.h` (`LOGFONTW` ↔ registry) | | Marquee, 3D Text, DVD, Flying Windows, Photos | [x] |
| C7 | `Core\Gfx\ImageLoader` (WIC → `Image`, EXIF rotate, downscale) + `CoInitializeEx` on the render thread | Falls back to `BmpReader`. Decode off-thread for Photos; upload on the render thread only. | DVD / Flying Windows image override, Photos, Flag, 3D Text texture | [x] |
| C10 | `TextureFactory`: `Emblem` (generic wavy four-pane flag), `SoftDot` (radial alpha), `Checker(w,h,cellsX,cellsY,a,b)`, `DiscLogo`; later `Toaster`/`Toast` sprites and `Primitives::Gear` | | Flying Windows, DVD, Boing, Fireworks/Flurry, Toasters, glxgears | [x] |
| C11 | `States`: `PremultipliedAlpha()`, `PointWrap()` | Trivial. | AA text, Bubbles, Life | [x] |
| C4 | `LineRenderer2D`: `Triangle`/`Quad`/`Strip` as a second `TRIANGLELIST` batch | | Ribbons, Energy, Starry Night, Boing grid/shadow | [x] |
| C5 | `Mesh::CreateDynamic` + `Mesh::Update(MeshData)` | `DYNAMIC`, `WRITE_DISCARD`. | Flying Objects, CPU FlowerBox | [x] |
| C6 | `Forward`: public `BindMaterial(ctx, material, world)` + `PixelShader()` getter | A saver can bind its own VS but reuse `PerFrame`/`PerObject` and `Phong_ps`. | FlowerBox morph VS | [x] |
| C8 | `Core\Gfx\FontMesh` (DirectWrite outline → D2D `Tessellate` + `Simplify` → extruded `MeshData`) | See 3D Text. | 3D Text | [x] |

---

## Tier 1 — next batch of six (in order)

Each entry: what it renders · algorithm · Core reuse · settings. Effort S/M/L.

- [x] **Matrix** (M) — *Matrix digital rain.* Columns of falling half-width katakana + digits +
  Latin, mirrored, green with near-white heads and a fading trail; cells re-randomise with a small
  probability (flicker). Atlas via **C2** from `MS Gothic` (fallback Consolas), U+FF66–FF9D +
  `0-9A-Z`, cell size × `dpiScale`. Per column: head y, speed, trail length, glyph index per cell.
  Draw via **C1** with `AlphaBlend`; trail brightness `(1 - i/len)^1.5`; optional afterglow via **C3**.
  Settings: Speed, Density, Glyph size, Colour, Charset, Bold heads, Afterglow.
- [x] **DVDBounce** (S) — *Bouncing DVD-style logo.* One sprite bounces off the edges, recolours
  on each bounce, flashes + increments a counter on corner hits. Default texture is a generic
  procedurally drawn disc logo (`DiscLogo`) drawn white so the sprite colour tints it; user image
  via `PickImageFile` + **C7**. Velocity components are chosen so corner hits happen at a
  configurable rate (position is evaluated analytically from time, so hits never drift).
  Settings: Image path, Size, Speed, Colour mode, Corner rate, Show corner counter, Background colour.
- [x] **FlyingWindows** (S) — *Flying Windows (Win 3.1 / 95).* Generic emblems fly at the viewer
  from the vanishing point. Starfield's z-model and `dpiScale` projection, **C1** instances with
  the `Emblem` uvRect, size ∝ 1/z, optional slow spin, optional background stars.
  Settings: Density (10–200), Warp speed, Image path, Pane colours ×4, Spin, Stars.
- [x] **Marquee** (S/M) — *Marquee (Win 3.1 – XP).* Text scrolls right-to-left, re-entering at a
  random (or centred) vertical position. `TextImage` (**C2**) at the chosen `LOGFONTW` × `dpiScale`;
  chunked at 8192 px for long strings. **C1** with `AlphaBlend`; background via `ClearColor()`.
  Mirror option = flipped uvRect.
  Settings: Text, Font, Size, Speed, Position, Text colour, Background colour, Mirror.
- [x] **Fireworks** (M) — *Fireworks.* SoA particle pool: rockets launch from the bottom at a
  Poisson rate and burst into peony / chrysanthemum (trailing sub-sparks) / ring / willow (long
  life) / crackle. Gravity, drag, life. **C1** + `SoftDot` + `Additive()` into **C3**, half-res
  Gaussian bloom composite (Aurora pass structure). Colour via `HsvToRgb`.
  Settings: Launch rate, Gravity, Trail persistence, Bloom, Burst size, Finale every N s.
- [x] **PersianRug** (S) — *Persian Rug (Win 3.x era).* The "Persian recursion": border in one colour,
  then each square's midlines take `(corner average + shift) mod N` and the quadrants recurse. Ops are
  planned up front and replayed progressively into an index image (`Texture::Update` per frame);
  palette cycling once complete; next rug weaves over the old one.
  Settings: Detail (2^6..2^9 + 1), Colours, Speed, Hold, Palette, Layout (square / stretch / tile), Cycle.
- [x] **BoingBall** (M) — *Amiga Boing Ball (1984).* Red/white checkered sphere, axis tilted ~17°,
  spinning, parabolic bounce with wall reflection, in a purple wire room. `Primitives::Sphere` +
  `Checker(256,128,16,8,red,white)`, `Forward` with high ambient / low spec for the flat look.
  Room = back wall + floor grid as perspective-projected `LineRenderer2D` lines. Shadow = flattened
  sphere with `AlphaBlend` + `DepthReadOnly`.
  Settings: Speed, Ball size, Ball colours ×2, Grid colour, Show shadow.

---

## Tier 2 — medium (next sessions, roughly in this order)

- [x] **Text3D** (L) — *3D Text*, incl. clock mode. Font → mesh via **C8**: DirectWrite
  `IDWriteFontFace::GetGlyphRunOutline` into an `ID2D1PathGeometry` (D2D factory only, no render
  target) → `Outline()` to union overlaps → `Tessellate()` for the front cap (handles holes /
  winding), mirrored copy for the back cap → `Simplify(LINES)` contours extruded into side quads,
  normal direction fixed by `FillContainsPoint(mid + n·ε)`. Rejected: `GetGlyphOutlineW` + own
  ear-clipping (hole bridging, no union, fragile). Clock mode caches per-glyph meshes for
  `0-9 : A P M` and composes per second. Rotation styles: None / Spin / See-saw / Wobble / Tumble /
  Random. Surface: solid, textured (**C7** or `Marble`); "Reflection" deferred (no env-map path in
  `Phong_ps`).
  Settings: Text, Show time (12/24h), Font, Size, Depth, Resolution, Rotation, Speed, Surface, Colour, Texture.
- [x] **Bubbles** (M) — shipped as `BubblesVista` (Windows has its own `Bubbles.scr`). — *Bubbles (Vista+).* `captureDesktop` from a settings read in `Main.cpp`
  (Mystify95 pattern), crop `Host::DesktopImage()` per viewport as `InitPersistentTarget` does;
  preview = dark gradient. Bubbles are **C1** quads with a PS override: `n = (d, sqrt(1-r²))`,
  Fresnel `0.04 + 0.96(1-n.z)^5`, thin-film cosine palette by `n.z` + per-bubble phase, desktop
  refraction by sampling the desktop texture at `screenUV + n.xy·k`, specular blob, rim darkening;
  `PremultipliedAlpha()` (**C11**). Circles with elastic pairwise collisions (≤40), radius wobble.
  Settings: Count, Size, Speed, Show on desktop, Colour mode, Wobble.
- [x] **Photos** (M/L) — *Photos slideshow.* Recursive enumeration of jpg/png/bmp/gif/tif,
  shuffled; a worker thread decodes the *next* image via **C7** (`maxDim` = 2× viewport) into an
  `Image` under a mutex; the render thread uploads with `Texture::FromImage`. Fullscreen
  `Photos_ps`: two textures + blend + per-image affine (fit/fill, slow Ken Burns), letterbox.
  Empty folder → `TextImage` notice. Per-monitor independent sequences.
  Settings: Folder (`PickFolder`), Interval, Transition, Fit/Fill, Shuffle, Subfolders, Show file name.
- [x] **Flurry** (M/L) — *macOS Flurry.* Streams on smooth random paths spawn 40–80 short-lived
  particles/frame with tangential velocity; `SoftDot` + `Additive()`; **C3 feedback mode**
  (`zoom 1.01`, `fade 0.93`, 4-tap blur) gives the glowing trails; optional bloom.
  Settings: Streams, Preset (Classic / RGB / Fire / Water / Psychedelic / Binary), Speed, Brightness, Trail.
- [x] **FlowerBox** (M) — built on the CPU path (C5 dynamic meshes, parametric normals) instead of the VS morph; C6 not needed. — *3D FlowerBox (95 Plus! / NT / XP).* Subdivided cube with custom vertex
  `{pos0,nrm0,pos1,nrm1,uv}` (cube point ↔ `normalize(pos0)`); `FlowerBox_vs.hlsl` blends by
  `t ∈ [-1,1.5]`, `t>1` adds spikes `nrm1·(t-1)·cos(πu)cos(πv)`; outputs `PhongPSIn` so `Phong_ps`
  is reused via **C6**. Tumbles and bounces inside the frustum; per-face colours or checker.
  Settings: Complexity, Shape (Cube/Sphere/Star/Cycle), Colour mode, Speed, Spin, Bounce, Size.
- [x] **Plasma** (S) — demoscene sum-of-sines with palette cycling; single fullscreen PS at half
  res (Aurora structure). Settings: Speed, Scale, Palette, Resolution.
- [x] **Life** (S/M) — Conway's Life as GPU ping-pong on two `R8G8B8A8` render textures
  (`{alive, age}`), `PointClamp`/`PointWrap` for toroidal, palette by age, fixed tick (Beziers
  accumulator), reseed on stagnation via a downsampled readback.
  Settings: Cell size, Tick rate, Density, Palette, Wrap, Reseed.
- [x] **Attractors** (M) — Lorenz / Rössler / Aizawa / Thomas / Halvorsen, RK4 on 10–30k
  particles, CPU-projected through `Camera::ViewProj()` into **C1** + `SoftDot` + `Additive` +
  **C3**; orbiting camera; colour by speed.
  Settings: Attractor (+Auto-cycle), Particles, Trail, Speed, Colour mode.
- [x] **Gears** (S/M) — *glxgears.* `Primitives::Gear` port of `gears.c` (front/back faces, tooth
  quads, bore), three gears with the classic ratios and offsets, `Forward` single white light,
  view `(20°,30°)` drifting; optional FPS via `TextImage`.
  Settings: Speed, Colours ×3, Wireframe, Auto-rotate, Show FPS.
- [x] **Ribbons** (M) — shipped as `RibbonsVista` (Windows has its own `Ribbons.scr`). — *Ribbons (Vista / 7).* Heads steered by fbm noise, width modulated by
  `sin(twist)` to fake the 3D turn, glossy cross-width shading, quads appended via **C4** into
  **C3** with very slow fade and periodic full fade-out.
  Settings: Count, Width, Persistence, Speed, Colour mode.

## Tier 3 — large or lower value

- [x] **FlyingObjects** (L) — *3D Flying Objects*, Style combo: Ribbon / Two Ribbons (ring buffer
  strip behind a Lissajous emitter), Twist (band rotated by `k·z + ωt`), Splash (grid ripple),
  Explode (de-indexed sphere triangles flying along normals and reassembling), Textured Flag (grid
  sine wave growing toward the free edge, `Emblem` or user image), Logo (four `Box` panes). All on
  **C5** dynamic meshes, `CullNone`, `ComputeNormals` per frame.
  Settings: Style, Colour mode, Texture, Resolution, Speed, Size, Wireframe.
- [x] **Energy** (M) — *Windows Energy (Vista).* 60–120 streamers `y = A·sin(kx+φ+ωt) + fbm` as
  thin strips (**C4**), additive into **C3**, half-res bloom; blue→cyan→white palette.
  Settings: Streamers, Amplitude, Speed, Tint, Bloom.
- [x] **AfterDark** (S–M) — one saver, Mode combo, to save boilerplate: *Starry Night* (twinkling
  stars + random skyline with windows lighting up), *Warp* (Starfield with streaks through **C3**),
  *Rain* (drops + expanding rings; over the captured desktop a fullscreen PS refracts by ≤32 rings),
  *Toasters homage* (generic winged appliance sprite sheet + toast, top-right → bottom-left,
  depth-sized).

---

## Tier 4 — Aquarium

- [x] **Aquarium** (XL) — shipped 2026-09-18 in one pass (sessions A–C below). Deviations from the
  plan: **C14** skipped (one `Forward::Draw` per fish with a per-fish cbuffer at `b2`; 30 draws is
  nothing); `FishMesh` lives in `Savers\Aquarium` rather than Core (nothing else wants it); kelp
  is built in the saver on a **C5** dynamic mesh, so no `Primitives::Strip`; the back wall is not
  geometry — `Water_ps` intersects each pixel's view ray with a wall plane (`z = 12`) so the
  gradient runs by *world height* and the fogged floor meets it seamlessly; `Billboard::Draw` /
  `DrawOriented` gained a blend-state parameter for the additive shafts. Fins are hard-edged
  geometry (no alpha test needed); `Fish_ps` flips the normal via `SV_IsFrontFace`.

A SereneScreen / After Dark "Fish!" style tank: a dozen procedurally built fish schooling and
wandering in front of a sandy floor, rocks and swaying plants, with caustics, light shafts and
bubbles. Everything is generated — no models, no textures on disk. Effort **XL**; plan it as three
sessions (Core, environment, fish) so each lands a runnable saver.

### Look and feel

- Camera: fixed, slightly below the tank's centre looking straight in (`fovY` ~35°), so the floor
  is seen from a shallow angle and the back wall fills the frame. Very slow ±3° yaw drift.
- Water: dark teal-blue gradient back wall (`Fill` + a `Water_ps` that adds slow sine "volume"
  bands), depth fog in `FrameConstants::fogColor/fogParams` so far fish and the back wall sink
  into the blue.
- Caustics: an animated `Caustics_ps` (sum of 3 warped voronoi/sines, tileable) rendered once per
  frame into a 256² `RenderTexture` and projected top-down onto the floor, rocks and fish
  (`Phong_ps` gets a second sampler slot for a "light map" that multiplies the diffuse term — see
  **C13**).
- Light shafts: 6–10 tall additive quads (`SpriteBatch2D`, `SoftDot` stretched) leaning ~15°,
  brightness modulated by a slow sine, drawn before the fish with `DepthReadOnly`.
- Bubbles: sprite streams rising from 2–3 floor vents with a wobble, `SoftDot` ring texture
  (`TextureFactory::BubbleRing`), pop near the surface. Occasional bubble trail from a fish.
- Glass: subtle vignette + a faint diagonal reflection band in the final composite PS; optional
  "tank frame" bars at the edges.

### Environment geometry

- Floor: `Primitives::Grid` with `Fbm` height noise (dunes), sand texture from `TextureFactory::
  Noise` tinted; caustics projected on top.
- Rocks: 5–8 `Sphere`s displaced by `Fbm` along their normals (new `Primitives::Rock(seed)`),
  greys/browns with a darker base; placed by rejection sampling so they never overlap.
- Plants: kelp/seaweed as `LineRenderer2D`-free 3D strips — chains of 8–14 quads on a **C5**
  dynamic mesh, each segment rotated by `sin(t·f + i·k)` accumulating up the stalk (the sway),
  green/olive with a lighter tip; 2-sided (`CullNone`). Grass tufts = thin triangles in bunches.
- Optional treasure chest / castle: three `Box`es and a `Cylinder` — a nod to the plastic
  ornaments in every 90s tank.

### Fish

- **Body**: a parametric mesh (`FishMesh::Build(species)`): elliptical cross-sections along the
  body axis `x ∈ [0,1]` with height/width profiles `h(x)`, `w(x)` from the species table, a flat
  tail fin (fan of quads, thin), dorsal/anal/pectoral fins as two-sided quads, an eye as a small
  dark sphere. ~600 vertices. Built once per species, instanced per fish.
- **Swim**: vertex animation in a custom `Fish_vs.hlsl` (needs **C6**): lateral displacement
  `z += A(x)·sin(k·x − ω·t + phase)` with `A` growing toward the tail, plus a tail-fin flick; `t`,
  phase and speed factor come from per-instance data (extend `InstanceData` with a `float4 anim`
  — **C14**). The instanced path draws a whole species in one call.
- **Patterns**: `Fish_ps.hlsl` colours by `uv` with per-species parameters (base + accent colour,
  stripe frequency / spot density / gradient mode, belly lightening by `nrm.y`, iridescent sheen
  `pow(1 − n·v, 3)`), multiplied by the projected caustics. No texture files.
- **Species table** (8 to start): clownfish (orange, 3 white bands), blue tang (blue, black
  outline, yellow tail), yellow tang (tall, flat), angelfish (very tall, striped, long fins),
  neon tetra (tiny, blue/red stripe, schools of 10–20), guppy (small, gradient tail), goldfish
  (round, orange, flowing tail), and a slow bottom-dwelling catfish/pleco.
- **Behaviour** (per fish, per frame): steering forces — wander (fbm-driven heading noise),
  tank-bounds avoidance (soft walls, strong near the glass), obstacle avoidance (rocks as spheres),
  schooling for schooling species (Reynolds: separation / alignment / cohesion within a radius),
  occasional "dart" impulses and idle hover. Heading turns are rate-limited; body roll/pitch follow
  the turn; speed sets the swim animation rate. Fish flip direction by turning, never by
  mirroring. A few fish nibble at plants (pause, small head bobs) — cheap and charming.
- **Depth sorting**: opaque fish, so only the back-to-front order of the translucent bits (fins,
  bubbles, shafts) matters; draw shafts → opaque scene → bubbles.

### Settings

Fish count (5–30), Species (checklist of the 8, or "Random mix"), Plants (0–10), Rocks (0–8),
Bubbles (on/off), Light shafts (on/off), Caustics strength, Water tint (colour), Speed, Show
ornament, Quality (caustics resolution / fish tessellation).

### Core additions it needs

| # | Addition | What | Done |
|---|---|---|---|
| C6 | `Forward::Draw(ctx, mesh, world, material, ID3D11VertexShader*, ID3D11InputLayout*, ID3D11PixelShader*)` + public `BindMaterial`, `PixelShader()` / `VertexShader()` / `Layout()` getters | Custom VS/PS while keeping `PerFrame`/`PerObject` and the white-texture / sampler binding. | [x] |
| C13 | `Phong_ps` light-map slot: `Material::lightMap/lightMapScale/lightMapStrength` → `PerObject.gLightMap`, sampled from `t1`/`s1` by `Lighting.hlsli` (`LightMapFactor`, `ShadePhong`, `ApplyFog` shared with saver shaders) | Multiplies diffuse by a projected texture (caustics) using world XZ → uv. | [x] |
| C14 | `InstanceData::anim` + `PhongInstanced_vs` pass-through | Per-instance animation params for the instanced fish path. | skipped — one draw per fish |
| C15 | `Primitives::Rock(radius, seed, roughness, squash)`, `TextureFactory::BubbleRing`; `FishMesh` and the kelp strip live in the saver | Geometry generators. | [x] |

### Build order

1. **Session A** — C6/C13/C14, `Water_ps`, `Caustics_ps`, floor + rocks + plants + shafts +
   bubbles. Ships as `Aquarium.scr` with an empty tank (already a pleasant saver).
2. **Session B** — `FishMesh` + `Fish_vs/ps` + the species table; fish placed and swimming in
   straight lines; pattern shader verified species by species with PreviewHost screenshots.
3. **Session C** — behaviours (wander, walls, rocks, schooling, darts, nibbling), depth fog tuning,
   glass composite, settings dialog, README row.

### Risks / notes

- `Phong_ps` has no alpha test: fins with soft edges need either a `clip()` in `Fish_ps` or
  hard-edged fin geometry (preferred — the originals were hard-edged too).
- Sorting 30 fish per frame is trivial; instancing per species is an optimisation, not a need —
  start with one draw per fish and add C14 only if a 4K profile shows the CPU bound.
- Keep everything DPI/resolution independent: fish size is in world units, only the caustics
  render texture scales with Quality.

---

## Framework facts that shape the designs

- `Core\Saver.h`: one `Saver` per monitor; `ClearColor()` → `nullopt` means the saver owns clearing.
- `Core\Gfx\Forward.h`: `Draw(..., vs, layout, ps)` (**C6**) runs a saver's own VS/PS on the Phong
  cbuffers; `Lighting.hlsli` gives such a PS the same shading, light map and fog as `Phong_ps`.
- `Core\Gfx\Mesh.cpp`: VB/IB are `IMMUTABLE` — no CPU-deformed geometry until **C5**.
- `Core\Gfx\LineRenderer2D`: `LINELIST` only, 1-px aliased.
- `Core\Gfx\RenderTexture`: fp16 default, no depth. `SwapChain`: `B8G8R8A8_UNORM`, no MSAA.
- Desktop capture is a one-shot GDI BitBlt at `WM_CREATE`, run-mode only, opted into per saver
  from `Main.cpp`.
- `build\publish.ps1` globs `Savers\*` and `install.ps1` globs `artifacts\*.scr` — no lists to
  edit. `Core\Core.vcxproj` lists sources explicitly; Core shaders are globbed.
- Project GUIDs follow `{5A7E0001-0000-4000-8000-000000NNNNNN}`; `IDC_DEFAULTS = 1010`.
- Templates to copy from: `Savers\Starfield` (instanced quads, custom HLSL), `Savers\Mystify95`
  (persistent target + desktop-capture crop, colour swatches in the dialog), `Savers\Aurora`
  (fullscreen PS + half-res bloom), `Savers\Pipes3D` (instanced Phong meshes).

## Boilerplate checklist per saver `<Name>`

1. `Savers\<Name>\`: `Main.cpp` (`szAppName`, `ScreenSaverProcW` → `rs::Host::Proc(..., L"<Name>",
   factory, captureDesktop)`, `ScreenSaverConfigureDialog`, `RegisterDialogClasses` →
   `rs::RunConfigDialog`), `<Name>Saver.h/.cpp` (`<Name>Settings{Load/Save}` with clamps; `Resize`
   must update `m_ctx`), `Config.cpp` (`Apply`/`Collect`/`Mirror`, `IDC_DEFAULTS`), `<Name>.rc`
   (`IDS_DESCRIPTION`, `ID_APP ICON "..\\..\\assets\\RetroSavers.ico"`, `DLG_SCRNSAVECONFIGURE`,
   VERSIONINFO), `resource.h` (IDs from 1001, `IDC_DEFAULTS 1010`), optional `Shaders\`.
2. `<Name>.vcxproj`: copy `Savers\Starfield\Starfield.vcxproj`; new GUID in the `{5A7E0001-…}`
   family; keep the `FxCompile` globs (harmless if no `Shaders\`). `..\Saver.props` provides
   `.scr`, libs, Core reference.
3. `RetroSavers.slnx`: add `<Project Path="Savers/<Name>/<Name>.vcxproj" />` under `/Savers/`.
4. Core additions → add to `Core\Core.vcxproj` `ClInclude`/`ClCompile` explicitly; Core HLSL is
   globbed; new `.hlsli` as `<None>`. Extra libs via `#pragma comment(lib, ...)` in the using
   `.cpp` (repo convention).
5. `README.md`: add a table row. `publish.ps1` / `install.ps1` need no edits.
6. This file: tick the saver off.

## Verification (per saver)

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe" Savers\<Name>\<Name>.vcxproj /p:Configuration=Debug /p:Platform=x64
bin\x64-Debug\PreviewHost.exe bin\x64-Debug\<Name>.scr
```

- PreviewHost: preview renders, **Configure** opens the dialog, Defaults resets, settings persist
  under `HKCU\Software\RetroSavers\<Name>`, **Fullscreen** (`/s`) exits on mouse/key.
- `$env:RETROSAVERS_FAKE_MONITORS=2` then `/s`: both viewports render independently, scissor clips.
- Resize the PreviewHost window: no stretching, sprites stay pixel-sized × DPI.
- `%LOCALAPPDATA%\RetroSavers\log.txt` has no render-thread exceptions; a Debug build with
  Graphics Tools installed shows no D3D11 debug-layer errors.
- Finally `.\build\publish.ps1` (all projects) and `.\build\install.ps1 -Saver <Name>`.

## Known limitations to carry into implementation

- No MSAA anywhere: 3D savers (Text3D, Gears, FlowerBox) show edge aliasing — accepted as retro.
- `Phong_ps` has no alpha-test; keyed textures on 3D meshes will fringe (use `Billboard` or add a flag).
- Desktop capture is decided statically in `Main.cpp`; toggling "show on desktop" needs a restart.
- `GlyphAtlas` caps its cell size so the Matrix charset fits in 4096²; `Marquee` chunks at 8192 px.
