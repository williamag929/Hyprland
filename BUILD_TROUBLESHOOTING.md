# Spatial Hyprland Build Troubleshooting Guide

## Overview

Compiling Spatial Hyprland requires several Arch User Repository (AUR) packages that are not available in the official Arch repositories. This document provides solutions for various build scenarios.

## Problem: AUR Dependencies Missing

### Symptoms
```
CMake Error at /usr/share/cmake/Modules/FindPkgConfig.cmake:1093 (message):
  The following required packages were not found:
   - aquamarine>=0.9.3
   - hyprlang>=0.6.7
   - hyprcursor>=0.1.7
   - hyprutils>=0.11.0
   - hyprgraphics>=0.1.6
```

### Root Cause
Hyprland has strict dependency requirements on AUR packages that must be built and installed before CMake configuration can complete.

### Solutions

#### **Solution 1: Use a Native Arch Linux Machine (RECOMMENDED)**

This is the easiest approach since you'll have yay or paru available:

```bash
# Install AUR helper (if not already installed)
sudo pacman -S yay

# Install all required Hyprland dependencies
yay -S aquamarine-git hyprlang-git libhyprutils-git hyprcursor-git hyprgraphics-git hyprwire-git hyprwayland-scanner-git --noconfirm

# Clone Spatial Hyprland fork
git clone https://github.com/YOUR_ORG/spatial-hypr.git
cd spatial-hypr

# Configure with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++

# Build
cmake --build build -j$(nproc)

# Optional: Run tests
ctest --test-dir build -R "spatial" --output-on-failure
```

**Estimated total time:** 30-45 minutes  
**System recommendations:** 4GB RAM minimum, 10GB free disk space

---

#### **Solution 2: Docker with Pre-Built Base Image**

If you want containerized builds, use a pre-built image that includes AUR packages:

```bash
# Option A: Use a community image (if available)
docker pull spatial-hyprland:latest

# Option B: Build a complete image yourself (takes 30-60 minutes first time)
docker build -f Dockerfile.spatial-dev -t spatial-hypr:dev .

# Run interactive development container
docker run -it --rm \
  -v "$(pwd):/home/spatial/Hyprland" \
  -e WAYLAND_DISPLAY=wayland-99 \
  spatial-hypr:dev bash

# Inside container:
cd /home/spatial/Hyprland
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j4
```

**Note:** The Dockerfile must complete building all AUR packages. This occurs during the Docker image build step, not the runtime.

---

#### **Solution 3: Manual AUR Build Chain (Advanced)**

If you need to build in Docker or another minimal environment:

```bash
# Install base dev tools
sudo pacman -Sy base-devel git cmake meson ninja pkg-config

# Build dependencies in correct order
## 1. hyprutils (foundation)
git clone https://aur.archlinux.org/hyprutils-git.git
cd hyprutils-git && makepkg -si --noconfirm && cd ..

## 2. hyprlang  
git clone https://aur.archlinux.org/hyprlang-git.git
cd hyprlang-git && makepkg -si --noconfirm && cd ..

## 3. aquamarine (longest build, ~10-15 min)
git clone https://aur.archlinux.org/aquamarine-git.git
cd aquamarine-git && makepkg -si --noconfirm && cd ..

## 4. hyprcursor
git clone https://aur.archlinux.org/hyprcursor-git.git
cd hyprcursor-git && makepkg -si --noconfirm && cd ..

## 5. hyprgraphics
git clone https://aur.archlinux.org/hyprgraphics-git.git
cd hyprgraphics-git && makepkg -si --noconfirm && cd ..

## 6. hyprwire  
git clone https://aur.archlinux.org/hyprwire-git.git
cd hyprwire-git && makepkg -si --noconfirm && cd ..

## 7. hyprwayland-scanner
git clone https://aur.archlinux.org/hyprwayland-scanner-git.git
cd hyprwayland-scanner-git && makepkg -si --noconfirm && cd ..

# Now configure Hyprland
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j4
```

**Estimated time per package:** 2-15 minutes depending on system  
**Total estimated:** 40-90 minutes for all dependencies

---

## Build System Overview

| Stage | Duration | Key Commands |
|-------|----------|--------------|
| Configure | 1-2 min | `cmake -B build` |
| Compile | 15-25 min | `cmake --build build -j4` |
| Test | 5-10 min | `ctest --test-dir build -R "spatial"` |
| **Total** | **25-40 min** | - |

---

## Compiler Selection

The CMake configuration prefers Clang but falls back to GCC:

```bash
# Explicitly use Clang (recommended)
cmake -B build -DCMAKE_CXX_COMPILER=clang++

# Or use GCC
cmake -B build -DCMAKE_CXX_COMPILER=g++

# Check compiler version
clang++ --version  # Should be 16+
g++ --version      # Should be 13+
```

---

## Environment Setup

### For Arch Linux

```bash
# Complete system dependencies (everything needed for Spatial Hyprland)
sudo pacman -S --needed \
    base-devel git go clang cmake meson openssh \
    wayland wayland-protocols glm spdlog nlohmann-json glslang \
    libxkbcommon libxcursor re2 muparser pkgconf \
    xcb-util-wm xcb-util-errors \
    gtest valgrind

# Then install AUR dependencies (requires yay or manual builds)
yay -S aquamarine-git hyprlang-git libhyprutils-git hyprcursor-git \
       hyprgraphics-git hyprwayland-scanner-git hyprwire-git
```

**Package breakdown:**
- **Build essentials:** base-devel, git, go, clang, cmake, meson
- **Wayland core:** wayland, wayland-protocols
- **Graphics/Math:** glm, spdlog, nlohmann-json, glslang
- **X11/XCB support:** libxkbcommon, libxcursor, xcb-util-wm, xcb-util-errors
- **Utilities:** re2, muparser, pkgconf, openssh
- **Testing:** gtest, valgrind

### For Docker

```bash
# Use the provided Dockerfile, but be aware:
docker build -f Dockerfile.spatial-dev -t spatial-hypr:dev .
# This downloads ~5GB and builds ~2GB of Docker layers
# Plan 45-90 minutes for first build

# Check if image built successfully
docker images | grep spatial-hypr
```

---

## Common Build Errors and Fixes

### Error: "Package 'xkbcommon' not found" or similar CMake package errors

**Symptoms:**
```
CMake Error: The following required packages were not found:
  - xkbcommon>=1.11.0
  - xcursor
  - re2
  - muparser
```

**Root Cause:** Missing system dependencies needed by Hyprland

**Fix:** Install complete system dependencies:
```bash
sudo pacman -S --needed \
    base-devel git go clang cmake meson openssh \
    wayland wayland-protocols glm spdlog nlohmann-json glslang \
    libxkbcommon libxcursor re2 muparser pkgconf \
    xcb-util-wm xcb-util-errors
```

---

### Error: "Package 'xcb-errors' not found"

**Fix:** The package is named differently:
```bash
sudo pacman -S xcb-util-errors
```

---

### Error: "Package 'hyprwire' not found"

**Fix:** This is an AUR package:
```bash
yay -S hyprwire-git
# Or manually:
git clone https://aur.archlinux.org/hyprwire-git.git
cd hyprwire-git && makepkg -si
```

---

### Error: "udis86 not found" or "subproject/udis86 missing"

**Fix:** Initialize git submodules:
```bash
git submodule update --init --recursive
```

---

### Error: "Package 'aquamarine' not found"
**Fix:** Build and install aquamarine-git from AUR (see Solution 3)

### Error: "cmake: command not found"
**Fix:** Install CMake:
```bash
sudo pacman -S cmake
```

### Error: "Could not find Clang"  
**Fix:** Install Clang:
```bash
sudo pacman -S clang llvm
```

### Linker Error: "undefined reference to ..."
**Cause:** Missing or incompatible dependency version
**Fix:** Verify dependency versions:
```bash
pkg-config --modversion aquamarine
pkg-config --modversion hyprlang
pkg-config --modversion hyprutils
```

---

## Performance Tips

- **Use -j with appropriate core count:**
  ```bash
  cmake --build build -j$(nproc)  # Desktop: all cores
  cmake --build build -j4          # Docker/VMs: 4 cores
  cmake --build build -j2          # Low-memory: 2 cores
  ```

- **Enable CPU cache optimizations:**
  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Release  # Faster final binary
  ```

- **Parallel shader compilation:**
  Shader includes are generated during CMake configuration, not build, so enabling parallel make doesn't affect shader speed.

---

## Memory Requirements

| Build Type | Min RAM | Recommended |
|-----------|---------|-------------|
| Debug | 2GB | 4GB |
| Release | 2GB | 4GB |
| With tests | 3GB | 6GB+ |
| Docker | 4GB | 8GB |

If you have < 2GB RAM:
- Use `-j1` (single-threaded build)
- Consider cross-compilation on another machine
- Use `-DCMAKE_BUILD_TYPE=MinSizeRel` for smaller binary

---

## Testing After Build

Once `cmake --build build` completes successfully:

```bash
# Verify executable exists
ls -lh build/Hyprland

# Run basic version check
./build/Hyprland --version

# Run unit tests (Spatial module)
ctest --test-dir build -R "spatial" --output-on-failure

# Check for memory leaks (on native Linux only)
valgrind --leak-check=full ./build/Hyprland --version
```

---

## Recommended Development Workflow

```bash
# 1. Clone repo
git clone https://github.com/YOUR_ORG/spatial-hypr.git
cd spatial-hypr

# 2. Ensure dependencies (native Arch only)
yay -S aquamarine-git hyprlang-git libhyprutils-git hyprcursor-git hyprgraphics-git

# 3. Configure (fresh build)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++

# 4. Build
cmake --build build -j$(nproc)

# 5. Test
ctest --test-dir build --output-on-failure

# 6. Subsequent changes - just rebuild
cmake --build build -j$(nproc)

# 7. Clean rebuild if needed
rm -rf build && cmake -B build ... && cmake --build build ...
```

---

## Getting Help

If you encounter issues not covered here:

1. **Check build log:** `cat /tmp/cmake-config.log` (if available)
2. **Verify pkg-config paths:**
   ```bash
   export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig"
   pkg-config --list-all | grep aquamarine
   ```
3. **Report issue with system info:**
   ```bash
   uname -a
   cmake --version
   gcc --version
   pacman -Q aquamarine-git hyprlang-git  # Or appropriate package versions
   ```

---

**Last Updated:** February 2026  
**For:** Spatial Hyprland v0.45.x fork on Space-Z branch
