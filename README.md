# Spectral Sheet Renderer

A real-time C++ / WebGL2 renderer for a GPU-simulated reflective sheet with physically inspired spectral color, diffraction, thin-film interference, roughness, polarization controls, and procedural material structure.

The project renders a shiny rainbow foil-like sheet without external scene assets. Meshes, materials, lighting, procedural environment reflections, wavelength sampling, debug views, and material presets are all generated in code.

## Live demo

Try it in the browser:

**[Launch Spectral Sheet Renderer](https://chintan-27.github.io/spectral-sheet-renderer/)**

## Preview

![Spectral Sheet Renderer preview](https://github.com/chintan-27/spectral-sheet-renderer/blob/main/docs/GIF.gif)

*A real-time spectral foil sheet with metallic reflection, diffraction, thin-film color shifts, roughness, folds, wrinkles, and procedural lighting.*

For a higher-quality capture, see:

[Watch the demo video](https://github.com/chintan-27/spectral-sheet-renderer/blob/main/docs/DEMO.mp4)

## What it does

* Renders a subdivided 3D sheet in WebGL2
* Simulates the sheet on the GPU with floating-point position and velocity textures
* Adds gravity, pinned-edge poster behavior, springs, damping, wind, floor collision, and interactive pulling
* Uses metallic reflection with wavelength-dependent optical constants
* Computes spectral color from visible wavelength samples
* Adds diffraction from microscopic groove structure
* Adds thin-film / layer interference color shifts
* Keeps ordinary metals visually distinct from the reflective rainbow sheet material
* Blends roughness, disorder, grain direction, polarization, and sheet imperfections
* Uses a procedural environment instead of image-based assets
* Keeps the microscopic model feasible for real-time rendering

## Philosophy

This is a  **structure-first renderer** , not an atom simulator.

The material model uses microscopic ideas such as grooves, layer thickness, grain direction, lattice-inspired disorder, and optical constants, but it does not simulate individual atoms. Atomic-scale ideas are reduced into practical shader parameters such as:

* refractive index `n`
* extinction coefficient `k`
* roughness
* groove spacing
* groove depth
* layer thickness
* grain direction
* disorder strength

Visible color comes from wavelength-dependent reflection, diffraction, and interference.

## Current status

Milestones **1 through 25** are complete.

The renderer currently supports:

* WebGL2 rendering through Emscripten
* A generated sheet mesh driven by GPU cloth state
* Camera controls
* Directional lighting
* Metallic shading
* Procedural environment reflections
* Spectral wavelength sampling
* Structure-derived optical behavior
* Conductor Fresnel reflection
* Groove-direction-based diffraction
* Thin-film interference
* Roughness-aware rainbow blur
* GPU cloth simulation with pinned top edge, springs, gravity, random wind, and collision
* Interactive sheet pulling behavior
* Code-defined lighting presets
* Runtime quality modes
* Optional mixed / s / p polarization modes

Next planned milestone:

* Milestone 26: final polish, documentation cleanup, screenshots, and demo capture refresh.

## Project layout

```text
.
├── environment.cpp / environment.h   # Procedural environment and reflection support
├── gpu.cpp / gpu.h                   # WebGL / GPU helpers
├── light.cpp / light.h               # Lighting setup and presets
├── material.cpp / material.h         # Material structure and optical parameters
├── math3d.cpp / math3d.h             # Vector and matrix math
├── shaders.cpp / shaders.h           # GLSL shader sources and shader logic
├── sheet_mesh.cpp / sheet_mesh.h     # Generated sheet geometry
├── validation.cpp                     # Native validation checks for optics and cloth math
├── main.cpp                          # Application entry point
├── index.html                        # Browser page
├── index.js                          # Emscripten output glue
├── index.wasm                        # WebAssembly output
├── Makefile                          # Build commands
├── plan.md                           # Project roadmap
├── notes/                            # Development notes
├── plots/                            # Optional generated plots
├── output/                           # Optional generated outputs
└── data/                             # Project data folder
```

## Build

This project targets the browser through Emscripten.

```bash
make
```

Then serve the directory with a local web server:

```bash
python3 -m http.server 8000
```

Open:

```text
http://localhost:8000
```

Run the native validation checks:

```bash
make test
```

## Requirements

* C++ compiler
* Emscripten
* WebGL2-capable browser
* Make

## Design constraints

The renderer intentionally avoids runtime asset dependencies.

Runtime behavior should come from:

* C++
* GLSL
* hardcoded material constants
* generated meshes
* procedural environments
* procedural structure and disorder

The project should not require:

* image textures
* downloaded material files
* external model files
* external scene assets
* large datasets

Small measured or representative optical tables may be written directly into source code with comments.

## Rendering model

The sheet appearance is built from several layers of behavior:

### 1. Geometry

* Generated subdivided sheet mesh
* GPU simulation using ping-ponged position and velocity textures
* Structural, shear, and bend springs
* Gravity, damping, wind/randomness, pinned top edge, and floor collision
* Interactive pulling force from mouse input

### 2. Lighting

* Directional light presets
* Normal-based shading
* Procedural environment reflections

### 3. Metal optics

* Wavelength-dependent conductor values
* Fresnel reflection
* Angle-sensitive reflectance

### 4. Microstructure

* Groove spacing
* Groove direction
* Groove depth
* Layer thickness
* Roughness and disorder

### 5. Spectral color

* Visible wavelength samples
* RGB reconstruction from sampled spectrum
* Diffraction and thin-film effects mixed with reflection

### 6. Runtime controls

* `1` aluminum foil
* `2` coated plastic
* `3` reflective rainbow sheet
* `4` copper
* `5` gold
* `6` silver
* `L` cycle light preset
* `K` cycle fast / balanced / quality mode
* `Z` cycle mixed / s / p polarization
* `F` reset the cloth drop/open simulation
* `Shift + drag` pull the cloth
* mouse drag orbit camera
* scroll zoom

## Completed milestone summary

* Basic C++ / WebGL2 project setup
* Generated 3D sheet mesh
* Animated sheet deformation
* Directional lighting
* Metallic material response
* Material structure model
* Microstructure debug views
* Procedural environment reflections
* Spectral wavelength sampling
* Structure-derived optical constants
* Real conductor Fresnel optics
* Tangent and groove direction support
* Diffraction from groove structure
* Feasibility checks for optical-scale structure
* Stable smooth diffraction bands
* Combined metal reflection and rainbow diffraction
* Thin-film / layer interference
* Roughness-controlled rainbow blur
* Lattice-inspired procedural disorder
* Sheet imperfections
* Improved motion, folds, wrinkles, and interactive shaping
* Material separation for ordinary metals vs the rainbow sheet target
* GPU cloth simulation
* Native validation checks
* Runtime performance modes
* Optional polarization controls

## Roadmap

### Milestone 26: Final polish

Remaining polish items:

* refresh README screenshots and video captures from the current GPU cloth renderer
* add a short note on what is physically approximate
* keep validation checks expanding as the physics model grows
* tune the debug views after visual testing

## Preview assets

Recommended files for the README:

```text
docs/
├── preview.gif   # short 5–10 second looping demo
└── demo.mp4      # optional higher-quality video
```

Suggested capture:

* show the live sheet moving
* orbit the camera slightly
* show rainbow patches shifting across the foil
* include folds, wrinkles, and specular highlights
* keep the GIF short so the README loads quickly

## License

See [LICENSE](https://chatgpt.com/c/LICENSE).
