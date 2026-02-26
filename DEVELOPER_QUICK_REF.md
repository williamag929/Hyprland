# Spatial Hyprland - Quick Developer Reference

## Build Paths

### 🏆 Recommended: Native Arch Linux
```bash
# Step 1: Install base build tools and system dependencies
sudo pacman -S --needed base-devel git go clang cmake meson openssh \
    wayland wayland-protocols glm spdlog nlohmann-json glslang \
    libxkbcommon libxcursor re2 muparser pkgconf \
    xcb-util-wm xcb-util-errors

# Step 2: Install yay (AUR helper)
cd ~
git clone https://aur.archlinux.org/yay.git
cd yay
makepkg -si
cd ..

# Step 3: Install AUR dependencies
yay -S aquamarine-git hyprlang-git libhyprutils-git hyprcursor-git \
       hyprgraphics-git hyprwayland-scanner-git hyprwire-git

# Step 4: Clone and build Spatial Hyprland
git clone https://github.com/YOUR_ORG/spatial-hypr.git
cd spatial-hypr
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)
```
**Time:** 40-50 minutes  
**Status:** ✅ Tested & works

---

### 📦 Docker Container Build
```bash
# Build base Docker image (one-time, ~60-90 min)
docker build -f Dockerfile.spatial-dev -t spatial-hypr:dev .

# Run development container
docker run -it --rm -v "$(pwd):/home/spatial/Hyprland" spatial-hypr:dev bash

# Inside container: build as normal
cd /home/spatial/Hyprland
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4
```
**Time:** 60-90 min (image) + 25 min (per build)  
**Status:** ⚠️ Image build complex, but runtime works

---

### 🪟 Windows/macOS (Via WSL2 or VM)
```bash
# On WSL2 with Arch, follow native path above:
wsl -d Arch
pacman -Syu
yay -S aquamarine-git hyprlang-git ...  # Same as native
```
**Time:** 40-50 minutes  
**Status:** ✅ Recommended alternative to Docker

---

## Complete Dependencies List

### System Packages (pacman)
```bash
sudo pacman -S --needed \
    base-devel git go clang cmake meson openssh \
    wayland wayland-protocols glm spdlog nlohmann-json glslang \
    libxkbcommon libxcursor re2 muparser pkgconf \
    xcb-util-wm xcb-util-errors
```

| Package | Purpose |
|---------|---------|
| `base-devel` | Build tools (gcc, make, fakeroot, etc.) |
| `git` | Version control |
| `go` | Required for yay and some dependencies |
| `clang` | C++ compiler (recommended) |
| `cmake` + `meson` | Build systems |
| `openssh` | SSH server for remote access |
| `wayland` + `wayland-protocols` | Wayland compositor base |
| `glm` | OpenGL Mathematics library |
| `spdlog` | Logging library |
| `nlohmann-json` | JSON parsing |
| `glslang` | Shader validator |
| `libxkbcommon` | Keyboard handling |
| `libxcursor` | Cursor support |
| `re2` | Regular expressions |
| `muparser` | Math expression parser |
| `pkgconf` | Package config tool |
| `xcb-util-wm` | X11/XCB window manager utilities |
| `xcb-util-errors` | XCB error handling |

### AUR Dependencies (yay)
```bash
yay -S aquamarine-git hyprlang-git libhyprutils-git hyprcursor-git \
       hyprgraphics-git hyprwayland-scanner-git hyprwire-git
```

| Package | Purpose |
|---------|---------|
| `aquamarine-git` | Hyprland rendering backend |
| `hyprlang-git` | Hyprland configuration parser |
| `libhyprutils-git` | Hyprland utility library |
| `hyprcursor-git` | Hyprland cursor library |
| `hyprgraphics-git` | Hyprland graphics utilities |
| `hyprwayland-scanner-git` | Wayland protocol scanner |
| `hyprwire-git` | Hyprland wire protocol |

### Important: Initialize Submodules
```bash
# After cloning the repo, always run:
git submodule update --init --recursive
```

This fetches required submodules like `udis86`, `tracy`, and `hyprland-protocols`.

---

## Compilation Workflow

### Fresh Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)
```

### Incremental Build (after changes)
```bash
cmake --build build -j$(nproc)
```

### Clean Rebuild
```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)
```

### Release Build (optimized)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

---

## Testing

### Run All Unit Tests
```bash
ctest --test-dir build --output-on-failure
```

### Run Only Spatial Module Tests  
```bash
ctest --test-dir build -R "spatial" --output-on-failure
```

### Run Specific Test
```bash
ctest --test-dir build -R "ZSpaceManager" -V
```

### Memory Check (valgrind)
```bash
valgrind --leak-check=full ./build/Hyprland --version
```

---

## Shader Validation

### PreBuild Check
```bash
bash scripts/validate-shaders.sh
```

### Compile Individual Shader
```bash
glslangValidator -V src/render/shaders/depth_spatial.frag
glslangValidator -V src/render/shaders/depth_dof.frag
```

---

## Code Quality

### Static Analysis
```bash
clang-tidy src/spatial/*.cpp -- -I. -Isrc -std=c++26
```

### Format Check
```bash
clang-format -i src/spatial/ZSpaceManager.cpp
```

### Compile with ASan (address sanitizer)
```bash
cmake -B build -DWITH_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

---

## Debugging

### Run in GDB
```bash
gdb ./build/Hyprland
(gdb) run --nested
(gdb) bt  # Backtrace
(gdb) q   # Quit
```

### Get Build Warnings
```bash
cmake --build build 2>&1 | grep -i "warning"
```

### Verbose Build Output
```bash
cmake --build build -j4 --verbose 2>&1 | less
```

### Check What Gets Compiled
```bash
cmake --build build -j1 -- VERBOSE=1 2>&1 | head -50
```

---

## Project Structure

```
spatial-hypr/
├── src/spatial/                    ← NEW: Z-depth system
│   ├── ZSpaceManager.{hpp,cpp}     ← Core Z-layer management
│   ├── SpatialConfig.{hpp,cpp}     ← Configuration parsing  
│   └── SpatialInputHandler.{hpp,cpp} ← Scroll→Z conversion
├── src/render/shaders/
│   ├── depth_spatial.frag          ← Adaptive blur + fade
│   ├── depth_dof.frag              ← Depth-of-field effect
│   └── passthrough_ar.frag         ← AR passthrough (future)
├── scripts/
│   ├── generateShaderIncludes.sh    ← Embed shaders at build time
│   ├── validate-shaders.sh          ← Pre-build validation
│   └── validate-shaders.ps1         ← Windows PowerShell version
├── docs/
│   ├── SPATIAL_OS_SPEC.md           ← Architecture specification
│   ├── SPATIAL_HYPR_FORK_SPEC.md   ← Hyprland fork details
│   ├── CODE_REVIEW_SPATIAL.md       ← Detailed code review
│   ├── CMAKE_ANALYSIS.md            ← Build system analysis
│   └── SHADER_REFERENCE.md          ← Graphics pipeline docs
├── README-SPATIAL.md                ← Full setup guide
├── BUILD_TROUBLESHOOTING.md         ← Solutions for common issues
├── CMakeLists.txt                   ← Build configuration
└── ...
```

---

## Key Design Details

### Z-Layer System
- **Layers:** 0 (front, active) through 3 (back)
- **Layer spacing:** 800.0f units apart in Z space
- **Animation:** Spring physics with configurable stiffness/damping
- **Activation:** Scroll wheel or `spatial_*` keybinds

### Spatial Config Section (hyprland.conf)
```conf
# New spatial{} configuration block
spatial {
  z_layers = 4              # Number of depth layers
  z_layer_step = 800.0      # Z distance between layers
  spring_stiffness = 200.0  # Velocity response
  spring_damping = 20.0     # Oscillation dampening
}
```

### Shader Effects
- **depth_spatial.frag:** 9-tap Gaussian blur, opacity fade
- **depth_dof.frag:** Depth-of-field with focus plane
- **blur radius:** Scales with Z distance (0px at focus, 12px at far)

### Thread Safety
- `ZSpaceManager` uses internal `std::mutex`
- Window Z-position updates are atomic
- Animation updates are frame-synchronized

---

## Common Tasks

### Adding a New Shader
1. Create `src/render/shaders/my_effect.frag`
2. Add to `scripts/generateShaderIncludes.sh`
3. Reference in `src/render/Renderer.cpp` with `SHADER_INCLUDE(my_effect_frag)`
4. Run `bash scripts/validate-shaders.sh` to verify
5. Rebuild: `cmake --build build -j$(nproc)`

### Adding a Z-Space Feature
1. Add method to `src/spatial/ZSpaceManager.hpp`
2. Implement in `src/spatial/ZSpaceManager.cpp`  
3. Add test in `tests/spatial/ZSpaceManager_test.cpp`
4. Rebuild and test: `cmake --build build && ctest --test-dir build -R "spatial"`

### Modifying Hyprland Core (Careful!)
- Files in `src/managers/`, `src/Desktop.cpp`, etc. are upstream
- Prefix commits with `[SPATIAL]` for future rebase tracking
- Test that vanilla Hyprland features still work

---

## Performance Optimization

### For Development (Debug Builds)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -j$(nproc)
# Slower compile (~25 min), fast iteration, debug symbols
```

### For Testing/Benchmarking (Release)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -j$(nproc)
# Slower compile (~30 min), ultra-fast runtime (60 FPS+ stable)
```

### Parallel Build Optimization
```bash
# Use all cores
cmake --build build -j$(nproc)

# Or specify number
cmake --build build -j8

# For low-memory systems
cmake --build build -j2
```

### Compiler Optimization
```bash
# Use LTO if time permits
cmake -B build -DCMAKE_CXX_FLAGS="-flto=thin" ...
```

---

## Documentation

| Document | Purpose | Audience |
|----------|---------|----------|
| [README-SPATIAL.md](README-SPATIAL.md) | Full setup & testing guide | Everyone |
| [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) | Error solutions | Build issues |
| [SPATIAL_OS_SPEC.md](docs/SPATIAL_OS_SPEC.md) | System architecture | Architects |
| [SPATIAL_HYPR_FORK_SPEC.md](docs/SPATIAL_HYPR_FORK_SPEC.md) | Technical fork changes | Developers |
| [CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md) | Code quality analysis | Code reviewers |
| [SHADER_REFERENCE.md](docs/SHADER_REFERENCE.md) | Graphics pipeline | Graphics developers |

---

## Issue Reporting

When reporting build/test issues, include:
```bash
# System info
uname -a
cmake --version
clang++ --version

# Dependency versions
pkg-config --modversion aquamarine
pacman -Q aquamarine-git hyprlang-git libhyprutils-git

# Build error (last 50 lines)
cmake --build build 2>&1 | tail -50
```

---

## Branch Strategy

- **`main`** - Stable Hyprland upstream
- **`Space-Z`** - Spatial OS development (current work)
- **`feature/*`** - Feature branches

```bash
# Switch to development branch
git checkout Space-Z

# Create feature branch
git checkout -b feature/my-feature Space-Z

# After work
git push origin feature/my-feature
# Create PR against Space-Z branch
```

---

## Contact & Support

- **Issues:** GitHub Issues (include `[BUILD]` tag for build problems)
- **Documentation:** See docs/ folder
- **Code Review:** Spatial module specific in [CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md)

---

**Last Updated:** February 26, 2026  
**For:** Spatial Hyprland v0.45.x (Space-Z branch)
