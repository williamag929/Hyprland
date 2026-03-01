# Docker Testing Guide — Spatial Hyprland

> How to build and test Spatial Hyprland in an Arch Linux container  
> Updated: February 26, 2026

---

## Quick Start

### Option 1: Docker Compose (Recommended)

```bash
# Build the container
docker-compose -f docker-compose.spatial.yml build

# Start the container
docker-compose -f docker-compose.spatial.yml up -d

# Enter the container
docker-compose -f docker-compose.spatial.yml exec spatial-dev bash

# Inside container: build Hyprland
cd Hyprland
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)

# If build dir is not writable (bind mount permission issue)
# use a writable location outside the mounted repo:
# cmake -B /tmp/hypr-build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
# cmake --build /tmp/hypr-build -j$(nproc)
```

### Option 2: Docker CLI (Linux/macOS)

```bash
# Build the image
docker build -f Dockerfile.spatial-dev -t spatial-hypr:dev .

# Run the container with source mounted
docker run -it --rm \
  --name spatial-dev \
  -v "$(pwd):/home/spatial/Hyprland" \
  -e XDG_RUNTIME_DIR=/run/user/1000 \
  -e WLR_BACKENDS=headless \
  spatial-hypr:dev bash
```

### Option 2B: Docker CLI on Windows (PowerShell)

```powershell
# Build the image
docker build -f Dockerfile.spatial-dev -t spatial-hypr:dev .

# Run the container with source mounted
docker run -it --rm `
  --name spatial-dev `
  -v "C:\projects\Hyprland:/home/spatial/Hyprland" `
  -e XDG_RUNTIME_DIR=/run/user/1000 `
  -e WLR_BACKENDS=headless `
  spatial-hypr:dev bash
```

**Note for Windows:** Use backticks (`) for line continuation, not backslashes (\).

---

## Building Spatial Hyprland in Container

### 0. First-Time Container Setup

**Important:** The Docker image may not have all Hyprland dependencies pre-installed. Complete this setup on first run:

```bash
# Inside container, install AUR helper (one-time)
git clone https://aur.archlinux.org/yay-bin.git /tmp/yay
cd /tmp/yay
makepkg -si --noconfirm

# Install all Hyprland AUR dependencies
yay -S --noconfirm \
  aquamarine-git \
  hyprlang-git \
  libhyprutils-git \
  hyprcursor-git \
  hyprwayland-scanner-git \
  hyprgraphics-git \
  hyprwire-git

# Install required Arch official packages
sudo pacman -S --noconfirm re2 muparser

# Verify installations
echo "✅ Checking installed packages..."
pkg-config --modversion aquamarine
pkg-config --modversion hyprlang
pkg-config --modversion libhyprutils
```

If any package is missing, CMake will fail with `Package 'xxx' not found` error.

### 1. Configure the Build

```bash
# Inside container
cd /home/spatial/Hyprland

# Debug build (for development)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# OR: Release build (for performance testing)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++
```

### 2. Compile

```bash
# Build with all CPU cores
cmake --build build -j$(nproc)

# OR: Build verbose to see compilation commands
cmake --build build -j$(nproc) --verbose
```

### 3. Validate Shaders

Before building, validate GLSL shaders:

```bash
# Inside container — Linux/macOS
bash scripts/validate-shaders.sh

# OR on Windows PowerShell (from host)
docker exec spatial-dev bash -c "bash scripts/validate-shaders.sh"

# Expected output
# ✅ depth_spatial.frag
# ✅ depth_dof.frag
# ✅ passthrough_ar.frag
```

If any shader fails validation, the build will continue but rendering **will fail at runtime**. Fix shader syntax before building.

### 4. Build the Project

```bash
# Build with all CPU cores
cmake --build build -j$(nproc)

# OR: Build verbose to see compilation commands
cmake --build build -j$(nproc) --verbose
```

### 5. Check Build Warnings

After successful build, check for any compiler warnings:

```bash
# Capture build output to file for analysis
cmake --build build -j$(nproc) 2>&1 | tee build.log

# Count warnings
grep -i "warning" build.log | wc -l

# Should output: 0 (no warnings expected)
```

---

## Running Tests

### Unit Tests

```bash
# Run all Hyprland tests
ctest --test-dir build --output-on-failure

# Run only spatial tests
ctest --test-dir build -R "spatial" --output-on-failure

# Run tests with verbose output
ctest --test-dir build -VV
```

### Memory Leak Detection

```bash
# Run tests under valgrind
ctest --test-dir build -T memcheck

# OR: Run specific binary with valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./build/Hyprland --version
```

### Static Analysis

```bash
# Run clang-tidy on spatial files
clang-tidy \
  src/spatial/*.cpp \
  -p build/compile_commands.json \
  -- -std=c++20

# Check for issues (should be clean)
```

---

## Testing Spatial Features (Headless Mode)

Since Docker containers typically don't have display access, test in **headless mode**:

### 1. Verify Initialization

```bash
# Run Hyprland with headless backend (no actual display)
WLR_BACKENDS=headless \
WLR_RENDERER=pixman \
./build/Hyprland --config example/hyprland.conf

# Check logs for spatial initialization:
# "Creating the ZSpaceManager!"
# "ZSpaceManager initialized with dimensions"
```

### 2. Test Configuration Parsing

```bash
# Create test config with spatial section
cat > /tmp/test-spatial.conf << 'EOF'
# Minimal Hyprland config
general {
    border_size = 2
}

# [SPATIAL] Test configuration
$spatial {
    z_layers = 4
    z_layer_step = 800.0
    spring_stiffness = 200.0
    spring_damping = 20.0
}
EOF

# Validate config loads without errors
./build/Hyprland --config /tmp/test-spatial.conf --check
```

### 3. Test hyprctl Integration

```bash
# Start Hyprland in background (headless)
WLR_BACKENDS=headless ./build/Hyprland &
HYPR_PID=$!

# Wait for initialization
sleep 2

# Test hyprctl commands (if you implement spatial commands)
./build/hyprctl version
# ./build/hyprctl spatial status  # Future command

# Cleanup
kill $HYPR_PID
```

---

## Testing with Nested Wayland (Advanced)

For visual testing, you need X11 forwarding from host:

### On Windows Host (WSLg/WSL2)

If you're running Docker Desktop with WSL2:

```bash
# 1. Install VcXsrv or X410 on Windows

# 2. Run with X11 forwarding
docker run -it --rm \
  --name spatial-dev \
  -v "$(pwd):/home/spatial/Hyprland" \
  -e DISPLAY=host.docker.internal:0 \
  -e XDG_RUNTIME_DIR=/run/user/1000 \
  spatial-hypr:dev bash

# 3. Inside container: install weston for nested compositor
sudo pacman -S weston

# 4. Run weston as nested compositor
weston --backend=x11-backend.so &

# 5. Run Hyprland nested inside weston
WAYLAND_DISPLAY=wayland-1 ./build/Hyprland
```

### On Linux Host

```bash
# Share host's Wayland socket
docker run -it --rm \
  --name spatial-dev \
  -v "$(pwd):/home/spatial/Hyprland" \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY:/tmp/wayland-host \
  -e DISPLAY=$DISPLAY \
  -e WAYLAND_DISPLAY=/tmp/wayland-host \
  -e XDG_RUNTIME_DIR=/run/user/1000 \
  spatial-hypr:dev bash
```

---

## Debugging in Container

### GDB Debugging

```bash
# Run Hyprland under GDB
gdb --args ./build/Hyprland --config example/hyprland.conf

# Inside GDB:
(gdb) break ZSpaceManager::init
(gdb) run
(gdb) backtrace
(gdb) print *g_pZSpaceManager
```

### Core Dumps

```bash
# Enable core dumps
ulimit -c unlimited

# Run and capture crash
./build/Hyprland

# Analyze core dump
gdb ./build/Hyprland core
```

### Logging

```bash
# Run with verbose logging
HYPRLAND_LOG_WLR=1 \
HYPRLAND_NO_RT=1 \
./build/Hyprland --config example/hyprland.conf 2>&1 | tee hyprland.log

# Check for spatial messages
grep -i "spatial\|zspace" hyprland.log
```

---

## CI/CD Simulation

Test the GitHub Actions workflow locally:

```bash
# Inside container, simulate CI steps

# 1. Clean build
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++

# 2. Build
cmake --build build -j$(nproc) 2>&1 | tee build.log

# Check for warnings
grep -i "warning" build.log && echo "⚠️ Warnings found" || echo "✅ Clean build"

# 3. Validate shaders
glslangValidator src/render/shaders/depth_spatial.frag || exit 1
glslangValidator src/render/shaders/depth_dof.frag || exit 1

# 4. Run tests
ctest --test-dir build -R "spatial" --output-on-failure || exit 1

# 5. Static analysis
# clang-tidy src/spatial/*.cpp -p build/compile_commands.json

# 6. Memory check (if tests exist)
# ctest --test-dir build -T memcheck

echo "✅ All CI checks passed"
```

---

## Performance Profiling

### CPU Profiling with perf (requires privileged container)

```bash
# Run container with --privileged flag
docker run -it --rm --privileged \
  -v "$(pwd):/home/spatial/Hyprland" \
  spatial-hypr:dev bash

# Inside container
sudo pacman -S perf

# Profile Hyprland
perf record -g ./build/Hyprland
perf report
```

### Tracy Profiler (included in submodules)

```bash
# Build with Tracy support
cmake -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DUSE_TRACY=ON

cmake --build build -j$(nproc)

# Tracy server runs on host, profiled app in container
# Export Tracy port: docker run -p 8086:8086 ...
```

---

## Troubleshooting

### Windows-Specific Issues

#### "The system cannot find the path specified"

**Error:**
```powershell
cd : Cannot find path 'C:\home\spatial\Hyprland' because it does not exist.
```

**Cause:** PowerShell $() syntax not supported in docker run  
**Solution:** Use backticks for line continuation
```powershell
# ❌ WRONG (Linux bash syntax)
docker run -it --rm \
  -v "$(pwd):/home/spatial/Hyprland"

# ✅ CORRECT (Windows PowerShell syntax)  
docker run -it --rm `
  -v "C:\projects\Hyprland:/home/spatial/Hyprland"
```

#### "Docker Desktop API 500 Error"

**Error:**
```
request returned 500 Internal Server Error for API route and version
```

**Solution:** Restart Docker Desktop completely
```powershell
# PowerShell as Administrator
Get-Process docker* | Stop-Process -Force
Start-Sleep -Seconds 5
Start-Process "C:\Program Files\Docker\Docker\Docker Desktop.exe"
Start-Sleep -Seconds 30
docker ps  # Test connection
```

#### Docker Container Exits Immediately

**Error:** Started container with `docker run ... bash` but it exits  
**Solution:** Add `-i` (interactive) flag
```powershell
# ❌ May exit immediately
docker run -t --rm -v "..." spatial-hypr:dev bash

# ✅ Keep running
docker run -it --rm -v "..." spatial-hypr:dev bash
```

---

### Linux-Specific Issues "Permission denied" on /dev/dri

```bash
# Run with device access
docker run -it --rm \
  --device /dev/dri:/dev/dri \
  -v "$(pwd):/home/spatial/Hyprland" \
  spatial-hypr:dev bash
```

### "Cannot open display"

```bash
# Use headless backend
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
```

### "wlroots version mismatch"

```bash
# Update Arch packages
sudo pacman -Syu

# Check wlroots version
pacman -Q wlroots
```

### "Package 'aquamarine', 'hyprlang', 'hyprcursor', etc. not found"

Hyprland has several dependencies distributed via the AUR instead of official Arch repos. The solution is the same for all of them:

**Inside the container, install from AUR (one-time setup):**

```bash
# First, install yay (AUR helper) — only needed once
git clone https://aur.archlinux.org/yay-bin.git /tmp/yay
cd /tmp/yay
makepkg -si --noconfirm
```

**Then install all known Hyprland AUR dependencies at once:**

```bash
yay -S --noconfirm \
  aquamarine-git \
  hyprlang-git \
  libhyprutils-git \
  hyprcursor-git \
  hyprwayland-scanner-git \
  hyprgraphics-git \
  hyprwire-git

# Verify all are installed
pkg-config --modversion aquamarine
pkg-config --modversion hyprlang
pkg-config --modversion libhyprutils
pkg-config --modversion hyprcursor
pkg-config --modversion hyprwayland-scanner
pkg-config --modversion hyprgraphics
pkg-config --modversion hyprwire
```

sudo pacman -S --noconfirm re2 muparser

# Then retry CMake
cmake -B /tmp/hypr-build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++

**If you get "conflicting packages" error:**

The Dockerfile may have installed stable versions from official repos. Remove the conflicting package and use `-git` AUR versions:

```bash
# Remove stable hyprlang if present
pacman -R hyprlang

# Then install AUR versions
yay -S --noconfirm \
  aquamarine-git \
  hyprlang-git \
  libhyprutils-git \
  hyprcursor-git \
  hyprwayland-scanner-git \
  hyprgraphics-git \
  hyprwire-git
```

**If hyprcursor-git fails to build:**

Install its build dependencies first:

```bash
sudo pacman -S --noconfirm libxcursor librsvg

# Retry
yay -S --noconfirm hyprcursor-git
```

**If pkg-config still can't find them, set the path explicitly:**

```bash
export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig
```

**Then retry CMake:**

```bash
cmake -B /tmp/hypr-build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build /tmp/hypr-build -j$(nproc)
```

**Complete list of Hyprland AUR packages:**
- `aquamarine-git` — rendering backend (required)
- `hyprlang-git` — config language parser (required)
- `libhyprutils-git` — utility library (required)
- `hyprcursor-git` — cursor management (required)
- `hyprgraphics-git` — graphics utilities (required)
- `hyprwire-git` — wire protocol library for hyprctl (required)
- `hyprwayland-scanner-git` — Wayland protocol scanner (required)
- `hyprpaper-git` — wallpaper manager (optional)

### "Unable to (re)create the private pkgRedirects directory"

This means the build directory is not writable (common with Windows bind mounts).
Use a writable build dir outside the mounted repo:

```bash
# Use a writable build directory
cmake -B /tmp/hypr-build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build /tmp/hypr-build -j$(nproc)
```

If you want to keep build artifacts in the repo, ensure permissions:

```bash
mkdir -p build
chmod -R u+rwX build
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
```

### Build fails with "spatial/ZSpaceManager.hpp not found"

```bash
# Ensure you're in the right directory
ls src/spatial/ZSpaceManager.hpp

# Check CMakeLists.txt includes spatial directory
grep -r "spatial" CMakeLists.txt
```

---

## Cleanup

```bash
# Stop and remove container
docker-compose -f docker-compose.spatial.yml down

# Remove volumes (build artifacts)
docker-compose -f docker-compose.spatial.yml down -v

# Remove image
docker rmi spatial-hypr:dev

# Clean build artifacts on host
rm -rf build/
```

---

## Automated Test Script

Save as `test-in-docker.sh`:

```bash
#!/bin/bash
set -e

echo "🐳 Building Docker image..."
docker build -f Dockerfile.spatial-dev -t spatial-hypr:dev .

echo "🔨 Building Spatial Hyprland..."
docker run --rm \
  -v "$(pwd):/home/spatial/Hyprland" \
  spatial-hypr:dev \
  bash -c "cd Hyprland && \
           cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ && \
           cmake --build build -j\$(nproc)"

echo "🧪 Running tests..."
docker run --rm \
  -v "$(pwd):/home/spatial/Hyprland" \
  spatial-hypr:dev \
  bash -c "cd Hyprland && ctest --test-dir build --output-on-failure"

echo "✨ Validating shaders..."
docker run --rm \
  -v "$(pwd):/home/spatial/Hyprland" \
  spatial-hypr:dev \
  bash -c "cd Hyprland && \
           glslangValidator src/render/shaders/depth_spatial.frag && \
           glslangValidator src/render/shaders/depth_dof.frag"

echo "✅ All tests passed!"
```

Run with:
```bash
chmod +x test-in-docker.sh
./test-in-docker.sh
```

---

## Next Steps

Once container testing passes:

1. **Build on native Arch Linux** for performance testing
2. **Test with real GPU** (remove WLR_BACKENDS=headless)
3. **Test nested in existing Hyprland/Sway session**
4. **Profile with Tracy** for performance optimization
5. **Test with Meta Quest 3** via ALVR (requires X11/Wayland forwarding)

---

*Spatial OS — Docker Testing Guide*  
*February 2026*
