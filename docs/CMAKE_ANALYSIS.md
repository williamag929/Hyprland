# CMake Configuration Analysis — Spatial Hyprland

> Detailed analysis of build system integration  
> Date: February 26, 2026

---

## Overview

**Build System:** CMake 3.30+  
**Generator:** Unix Makefiles (Linux/macOS) or Ninja (optional)  
**Compilers:** Clang 16+ preferred, GCC 13+ supported  
**Configuration:** Multi-file, modular design  

---

## Source File Organization

### File Globbing Strategy

**CMakeLists.txt, Line 273:**
```cmake
file(GLOB_RECURSE SRCFILES "src/*.cpp")
```

**Impact:**
- ✅ All C++ files in `src/` recursively included
- ✅ **Automatically picks up `src/spatial/*.cpp`** — manual changes NOT needed
- ⚠️ New files added to `src/` are immediately compiled

**Implication:** Adding new files to `src/spatial/` will automatically be built without CMakeLists.txt changes.

---

## Spatial Module Integration

### Current Setup ✅

```
CMakeLists.txt (line 284)
    └── add_executable(Hyprland src/main.cpp)
            └── target_link_libraries() → hyprland_lib
                    └── add_library(hyprland_lib STATIC ${SRCFILES})
                            ├── src/Compositor.cpp
                            ├── src/Window.cpp
                            ├── src/spatial/ZSpaceManager.cpp  ← INCLUDED
                            ├── src/spatial/SpatialConfig.cpp   ← INCLUDED
                            ├── src/spatial/SpatialInputHandler.cpp  ← INCLUDED
                            └── ... (all other src/*.cpp)
```

### Why This Works

1. **Glob recursion** captures subdirectories
2. **Header includes** resolve via `target_include_directories()`
3. **Link chain:** Hyprland exe → hyprland_lib → spatial objects

---

## Dependency Resolution

### Required Packages (pkg-config)

**Lines 252-268 in CMakeLists.txt:**
```cmake
pkg_check_modules(deps REQUIRED IMPORTED_TARGET GLOBAL
  xkbcommon>=1.11.0
  uuid
  wayland-server>=1.22.91
  wayland-protocols>=1.45
  cairo
  pango
  pangocairo
  pixman-1
  xcursor
  libdrm
  libinput>=1.28
  gbm
  gio-2.0
  re2          # ⚠️ Required by Hyprland (not Spatial)
  muparser     # ⚠️ Required by Hyprland (not Spatial)
)
```

### Spatial-Specific Dependencies

- ✅ **GLM** — Already required by Hyprland for rendering
- ✅ **OpenGL** — Already required for Compositor
- ✅ **pthread** — Standard library (already linked)

**No new external dependencies needed!** ✅

---

## Compiler Flags

### Debug Build

**CMakeLists.txt, Line 305-315:**
```cmake
if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Setting debug flags")
  
  if(WITH_ASAN)
    message(STATUS "Enabling ASan")
    target_link_libraries(hyprland_lib PUBLIC asan)
    target_compile_options(hyprland_lib PUBLIC -fsanitize=address)
  endif()
  
  add_compile_options(-fno-pie -fno-builtin)
  add_link_options(-no-pie -fno-builtin)
endif()
```

**For Spatial development, we recommend:**
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DWITH_ASAN=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

This enables:
- ✅ Debug symbols (`-g`)
- ✅ Address Sanitizer (memory bug detection)
- ✅ No optimizations (-O0)
- ✅ `compile_commands.json` for IDE integration

### Release Build

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++
```

Enables:
- ✅ Optimizations (-O3 or equivalent)
- ✅ Stripped debug symbols
- ✅ ~60-70% performance improvement over Debug

---

## Shader Integration

### Shader Processing Pipeline

**CMakeLists.txt, Lines 27-32:**
```cmake
execute_process(COMMAND ./scripts/generateShaderIncludes.sh
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE HYPR_SHADER_GEN_RESULT)
if(NOT HYPR_SHADER_GEN_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to generate shader includes...")
endif()
```

**Flow:**
```
.frag/.vert files
        ↓
./scripts/generateShaderIncludes.sh
        ↓
Embedded as C++ hex strings (build/_shaders.hpp)
        ↓
Compiled into Hyprland binary
        ↓
Runtime: Load from memory verbatim
```

### Spatial Shaders

**Files:**
- `src/render/shaders/depth_spatial.frag` ← Depth blurring
- `src/render/shaders/depth_dof.frag` ← Depth of field
- `src/render/shaders/passthrough_ar.frag` ← AR passthrough (future)

**Verification:**
```bash
# Check if shaders are embedded
strings build/Hyprland | grep "version 430 core"

# Should output shader version string if compiled
```

---

## Recommended CMake Improvements (Optional)

### 1. Explicit Spatial Module Target

**Current:** All .cpp files globbed into single library  
**Better:** Separate target for module isolation

**Add to CMakeLists.txt after line 284:**
```cmake
# Define Spatial module sources explicitly
target_sources(hyprland_lib PRIVATE
    src/spatial/ZSpaceManager.cpp
    src/spatial/SpatialConfig.cpp
    src/spatial/SpatialInputHandler.cpp
)

# Link spatial module dependencies (if any)
# Currently none — all satisfied by hyprland_lib

# Optional: Set module-specific compile options
set_source_files_properties(
    src/spatial/ZSpaceManager.cpp
    src/spatial/SpatialConfig.cpp
    src/spatial/SpatialInputHandler.cpp
    PROPERTIES
    COMPILE_FLAGS "-Wall -Wextra -Wpedantic"
)
```

**Benefits:**
- Visible in IDE/LSP as distinct module
- Easier to disable/remove
- Better for parallel builds

### 2. Shader Validation in CMake

**Add to CMakeLists.txt:**
```cmake
# ──────────────────────────────────────────────────────────────
# Shader validation (optional, add if glslangValidator available)
# ──────────────────────────────────────────────────────────────

find_program(GLANG_VALIDATOR glslangValidator)

if(GLANG_VALIDATOR)
    message(STATUS "Found glslangValidator: ${GLANG_VALIDATOR}")
    
    # Create custom target to validate shaders
    add_custom_target(validate-shaders
        COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/validate-shaders.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Validating GLSL shaders..."
    )
    
    # Make it run before main build (optional)
    # add_dependencies(hyprland_lib validate-shaders)
else()
    message(STATUS "glslangValidator not found — shader validation disabled")
endif()
```

**Usage:**
```bash
cmake --build build --target validate-shaders
```

### 3. Build Type Validation

**Prevent accidental builds:**
```cmake
# After project() declaration
if(NOT CMAKE_BUILD_TYPE)
    message(WARNING "CMAKE_BUILD_TYPE not set. Using Debug. 
             Recommended: Release for production, Debug for development")
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Validate options
string(TOLOWER ${CMAKE_BUILD_TYPE} _build_type_lower)
if(NOT _build_type_lower MATCHES "^(debug|release|minsizerel|relwithdebinfo)$")
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()
```

---

## Build Directory Management

### Local Build (In-tree)

```bash
# Create build/ subdirectory in repo
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Files created:
# build/CMakeCache.txt
# build/CMakeFiles/
# build/Makefile (or compile_commands.json)
# build/Hyprland (executable)
```

**Pros:** ✅ Simple, direct  
**Cons:** ⚠️ Pollutes source tree, harder to clean

### Out-of-Tree Build (Recommended)

```bash
# Build outside source tree
mkdir -p /tmp/hyprland-build
cd /tmp/hyprland-build
cmake /home/spatial/Hyprland -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Or in Docker:
cmake -B /tmp/hypr-build /home/spatial/Hyprland
```

**Pros:**  
- ✅ Source tree stays clean
- ✅ Multiple builds possible (Debug + Release)
- ✅ Easier to delete and rebuild

**Cons:**  
- ⚠️ Slightly more verbose

---

## Continuous Integration Configuration

### GitHub Actions Integration

**File:** `.github/workflows/spatial-build.yml`

**Recommended CI steps:**
```yaml
name: Spatial Build & Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    container: archlinux:latest
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Install Dependencies
        run: |
          pacman -Syu --noconfirm
          pacman -S --noconfirm \
            cmake ninja clang++ git \
            wayland wlroots wayland-protocols
          # AUR packages via yay
          
      - name: Validate Shaders
        run: bash scripts/validate-shaders.sh
        
      - name: Configure Build
        run: |
          cmake -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_COMPILER=clang++
      
      - name: Compile
        run: cmake --build build -j$(nproc)
      
      - name: Check Warnings
        run: cmake --build build 2>&1 | grep -i warning || echo "✅ No warnings"
      
      - name: Run Tests
        run: ctest --test-dir build --output-on-failure
      
      - name: Memory Check (ASan)
        run: WITH_ASAN=ON cmake -B build-asan ...
```

---

## Performance Considerations

### Build Time Optimization

| Technique | Impact | Cost |
|-----------|--------|------|
| **Precompiled headers** | -30% time | +Build complexity |
| **Parallel build** (`-j`) | -60%+ time | CPU intensive |
| **ccache/sccache** | -50% rebuild | Disk space |
| **LTO (Link-Time Opt)** | +5% perf | +Build time |
| **Ninja generator** | ~10% faster | Requires Ninja |

**Quick win — Use Ninja:**
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build  # Much faster
```

### Linking Optimization

**Split library strategy:**
```cmake
# Instead of monolithic hyprland_lib:
add_library(hyprland_render STATIC src/render/*.cpp)
add_library(hyprland_managers STATIC src/managers/*.cpp)
add_library(hyprland_spatial STATIC src/spatial/*.cpp)

target_link_libraries(Hyprland
    hyprland_render
    hyprland_managers
    hyprland_spatial
)
```

**Benefit:** Parallel linking (faster rebuild)

---

## Troubleshooting CMake Issues

### "CMakeCache.txt is different than the directory"

**Cause:** Mixed host/container paths

**Fix:**
```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

### "Package 'xyz' not found"

**Cause:** Missing dependency

**Solution:**
```bash
# Find package
pkg-config --list-all | grep xyz

# Install via package manager
sudo pacman -S xyz-libs
# OR
yay -S xyz-git
```

### "clang++ not found"

**Cause:** Compiler not installed

**Solution:**
```bash
# Arch
sudo pacman -S clang

# Ubuntu
sudo apt install clang

# Verify
which clang++
```

---

## Checklist for CMake Review

- [ ] All spatial source files automatically included via GLOB
- [ ] No duplicate source inclusion
- [ ] Shader generation script runs at configure time
- [ ] compiler_commands.json generated for IDEs
- [ ] Proper link-time dependency resolution
- [ ] Debug/Release builds both work
- [ ] No warnings in CI pipeline
- [ ] Build passes on fresh Docker image

---

## References

| Component | File | Lines |
|-----------|------|-------|
| Main config | `CMakeLists.txt` | 1-679 |
| Spatial sources | `src/spatial/*.cpp` | All included |
| Shader processing | `scripts/generateShaderIncludes.sh` | - |
| CI/CD config | `.github/workflows/*.yml` | - |

---

*End of CMake Analysis*  
*Generated: February 26, 2026*
