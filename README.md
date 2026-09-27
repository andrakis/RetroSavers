# RetroSavers

Classic Microsoft screen savers rebuilt for Windows 11 as native `.scr` files.
C++20, Direct3D 11, no third-party code — only the Windows SDK (`scrnsave.lib`, `d3d11`, DirectXMath).

| Saver | Original | Notes |
|---|---|---|
| `Starfield.scr` | Starfield Simulation (Win 3.1) | Warp-speed star field |
| `Mystify95.scr` | Mystify Your Mind (Win 3.1 / 95) | Two bouncing polygons with trails; "Clear screen" off draws over the live desktop |
| `Beziers.scr` | Béziers (2000 / XP) | Closed loop of cubic Béziers with a colour-cycling trail |
| `Pipes3D.scr` | 3D Pipes (NT 4 / 95 Plus!) | Elbow / ball / mixed joints, textured mode, the teapot |
| `Aurora.scr` | Aurora (Vista) | Pixel-shader curtains with bloom |
| `Maze3D.scr` | 3D Maze (NT 4 / 95 Plus!) | Right-hand-rule walk, smiley, OpenGL logo, rats, the rock that flips the world |
| `Matrix.scr` | Matrix digital rain | Mirrored half-width katakana, bright heads, fading trails, optional afterglow |
| `DVDBounce.scr` | Bouncing DVD-style logo | Generic disc logo (or your image) recolours on each bounce and counts corner hits |
| `FlyingWindows.scr` | Flying Windows (Win 3.1 / 95) | Generic four-pane emblems (recolourable, or your image) stream out of the vanishing point |
| `Marquee.scr` | Marquee (Win 3.1 - XP) | Scrolling text in any font, random or centred height, colours, mirror |
| `Fireworks.scr` | Fireworks | Rockets burst into peonies, chrysanthemums, rings, willows and crackles with trails and bloom |
| `BoingBall.scr` | Amiga Boing Ball (1984) | Checkered sphere on a tilted axis bouncing round a purple wire room, shadow on the wall |
| `PersianRug.scr` | Persian Rug (Win 3.x era) | "Persian recursion" rug woven line by line as spaced dots (or solid), palette cycling, square / stretch / tile |
| `Text3D.scr` | 3D Text (95 Plus! / NT / XP) | Extruded text or clock in any font (DirectWrite outlines), spin / see-saw / wobble / tumble, solid / marble / checker / image |
| `BubblesVista.scr` | Bubbles (Vista / 7) | Iridescent soap bubbles with elastic collisions floating over (and refracting) the desktop |
| `Photos.scr` | Photos slideshow | Any folder (recursive, shuffled), WIC decode on a worker thread, crossfade / cut / through black, fit or fill, slow pan and zoom, captions |
| `Flurry.scr` | Flurry (macOS) | Wandering glowing streams shedding particles through video feedback; Classic / RGB / Fire / Water / Psychedelic / Binary |
| `FlowerBox.scr` | 3D FlowerBox (95 Plus! / NT / XP) | Subdivided cube breathing between pinched cube, sphere and six-lobed flower; per-face colours, checker, cycling hues |
| `Plasma.scr` | Plasma (demoscene) | Sum-of-sines plasma through cycling cosine palettes; rainbow / fire / ocean / neon / greyscale |
| `Life.scr` | Conway's Game of Life | GPU ping-pong at a fixed tick, cells coloured by age, toroidal or bounded, reseeds when the world goes still |
| `Attractors.scr` | Strange attractors | Lorenz, Rössler, Aizawa, Thomas, Halvorsen: thousands of RK4 particles, orbiting camera, colour by speed / height / hue |
| `Gears.scr` | glxgears | The three meshing gears from gears.c, drifting view, wireframe, frame counter |
| `RibbonsVista.scr` | Ribbons (Vista / 7) | Wide glossy ribbons twisting across the screen, steered by noise, trails that slowly fade |
| `FlyingObjects.scr` | 3D Flying Objects (NT / 95 Plus!) | Ribbon, two ribbons, twist, splash, explode, textured flag, logo; rainbow / solid / checker |
| `Energy.scr` | Windows Energy (Vista) | A bundle of glowing streamers waving across the screen with bloom and motion streaks; tintable |
| `AfterDark.scr` | After Dark classics (homage) | Starry Night (city windows lighting up, meteors), Warp, Rain (ripples refracting the desktop), Flying Toasters |
| `Aquarium.scr` | Fish! / SereneScreen-style tank | Eight procedural species that school, wander, dart and nibble the kelp; dunes, rocks, treasure chest, caustics, light shafts, bubbles; tintable water |
| `Celestials.scr` | Space-sim star gazing | Slow orbit round a lensing black hole, boson star, white hole, Thorne-Zytkow object, strange star, cold neutron star or an ordinary star (red dwarf / sun-like / blue giant / red giant) with flares, CMEs and prominences; an info card describes each object with estimated figures; warp-jumps to a new object every N seconds |

## Building

Requirements: Visual Studio 2026 (v145 toolset), Windows SDK 10.0.26100, x64.

```powershell
# everything, Release, collected into artifacts\
.\build\publish.ps1

# a single saver from the terminal
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe" Savers\Pipes3D\Pipes3D.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Outputs land in `bin\x64-<Config>\` (`<Name>.scr`, `Core.lib`, `PreviewHost.exe`).
`RetroSavers.slnx` opens in Visual Studio and builds the same projects.

## Installing

```powershell
.\build\install.ps1                 # copy the .scr files to %LOCALAPPDATA%\RetroSavers
.\build\install.ps1 -Saver Pipes3D  # ...and make 3D Pipes the active screen saver (opens its settings)
.\build\install.ps1 -System         # (elevated) also copy into C:\Windows\System32 so they appear in the
                                    # Screen Saver Settings dropdown beside Bubbles / Ribbons
```

Any `.scr` can also be right-clicked in Explorer → **Install**.

## Developing

`bin\x64-Debug\PreviewHost.exe <path\to\Saver.scr>` hosts a saver in a resizable window using the
same `/p <hwnd>` protocol as the Windows preview. Buttons run the config dialog (`/c:<hwnd>`),
restart the preview, or launch fullscreen (`/s`). F5 restarts. A fullscreen run started from PreviewHost ignores
mouse movement (it sets `RETROSAVERS_CLICK_TO_EXIT=1`); click or press a key to leave it.

Command line switches (standard for screen savers):

| Switch | Meaning |
|---|---|
| `/s` | Run fullscreen; exits on mouse move, key, or click |
| `/p <hwnd>` | Render inside the given window (preview) |
| `/c` or `/c:<hwnd>` | Show the settings dialog |

Debug builds turn on the D3D11 debug layer when the *Graphics Tools* optional feature is installed.
Errors on the render thread go to `%LOCALAPPDATA%\RetroSavers\log.txt`.
Set `RETROSAVERS_FAKE_MONITORS=2` to split the primary monitor into two viewports and exercise the
multi-monitor path on a single display.

Settings live under `HKCU\Software\RetroSavers\<Saver>`.

## Layout

```
Core/            static library: scrnsave host + render thread, D3D11 wrappers, primitives,
                 procedural textures, line renderer, billboards, post-processing, settings
Savers/<Name>/   one project per saver: Main.cpp (scrnsave entry points), <Name>Saver.*,
                 Config.cpp (dialog), <Name>.rc, resource.h, Shaders/
tools/PreviewHost/   windowed dev harness
build/           publish.ps1, install.ps1
assets/          shared icon
```

HLSL under a project's `Shaders\` folder is compiled by the VS HLSL build step into byte-code headers
(`*_vs.hlsl` → vertex, `*_ps.hlsl` → pixel) included as `"Shaders/<Name>.h"` exposing `g_<Name>`.
No shaders are compiled at runtime.
