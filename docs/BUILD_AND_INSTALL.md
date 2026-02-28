# Spatial Hyprland — Build, Install & First Launch Guide

> Fork: spatial-hypr (Hyprland v0.45.x)  
> Last updated: February 2026

---

## Prerequisites

### Recommended Hardware
- **GPU:** Any modern Intel, AMD, or NVIDIA with proper Linux drivers  
  - Intel Arc / Iris Xe — best out of the box (i915, full EGL/GBM)  
  - AMD (amdgpu) — excellent open-source support  
  - NVIDIA — works but requires extra env vars (see Troubleshooting)  
- **RAM:** 4 GB minimum, 8 GB recommended  
- **Storage:** 10 GB free for build artifacts  

> **VirtualBox warning:** VirtualBox's vmwgfx driver does not support EGL properly without 3D Acceleration enabled. Use a real GPU or QEMU/KVM with virtio-gpu for development.

### Recommended OS
- **Arch Linux** (rolling — all deps available in official repos + AUR)  
- **Fedora 41/42** (stable alternative)  
- Avoid Ubuntu LTS / Debian stable — packages too old for aquamarine + wlroots

---

## 1. Install Dependencies (Arch Linux)

```bash
sudo pacman -S \
    base-devel git cmake meson ninja clang \
    wayland wayland-protocols libdrm mesa \
    libinput libxkbcommon pixman \
    glm spdlog nlohmann-json \
    glslang spirv-tools \
    xorg-server xorg-xinit xterm \
    seatd libseat \
    gtest \
    hyprutils hyprlang aquamarine hyprwayland-scanner \
    pkgconf

# Optional: for seatd-based seat management
sudo systemctl enable --now seatd
sudo usermod -aG seat $USER
```

---

## 2. Clone the Repository

```bash
git clone https://github.com/hyprwm/Hyprland.git -b Space-Z spatial-hypr
cd spatial-hypr


git submodule update --init --recursive

```

---

## 3. Build

sudo pacman -S cmake
sudo pacman -S base-devel
```bash
# Ensure clang is installed first
sudo pacman -S clang

cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=$(which clang++) \
    -DCMAKE_C_COMPILER=$(which clang) \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build -j$(nproc)
```

Build artifacts:
- `build/Hyprland` — main compositor binary
- `build/hyprctl/hyprctl` — IPC control tool

### run

export XDG_RUNTIME_DIR=/run/user/$(id -u)
export WAYLAND_DISPLAY=wayland-0   # GNOME's socket — usually wayland-0 or wayland-1
export WLR_BACKENDS=wayland
export WLR_NO_HARDWARE_CURSORS=1

mkdir -p ~/.config/hypr
cp example/hyprland.conf ~/.config/hypr/hyprland.conf
echo "monitor=,1920x1080@60,0x0,1" >> ~/.config/hypr/hyprland.conf

./build/Hyprland --config ~/.config/hypr/hyprland.conf

./build/Hyprland --config ~/.config/hypr/hyprland.conf

Hyprland opens as a window inside your GNOME session. If wayland-0 doesn't work, check the correct socket:

echo $WAYLAND_DISPLAY

### Build Notes
- Build takes 5–15 minutes depending on hardware
- Subprojects (aquamarine, hyprlang, etc.) are fetched automatically via CMake FetchContent if not found system-wide
- `glaze` is fetched automatically if not installed

---

## 4. Validate Shaders

```bash
glslangValidator -V src/render/shaders/depth_spatial.frag
glslangValidator -V src/render/shaders/depth_dof.frag
glslangValidator -V src/render/shaders/passthrough_ar.frag
```

All three must pass with no errors before running.

---

## 5. Install (optional)

```bash
sudo cmake --install build
```

Or run directly from the build directory without installing (recommended for development).

---

## 6. Setup XDG Runtime Directory

Hyprland requires `XDG_RUNTIME_DIR` to exist. The cleanest solution is enabling systemd-logind which creates it automatically on login:

```bash
sudo systemctl enable --now systemd-logind
```

Log out and back in — `/run/user/1000` will be created automatically.

**Manual fallback** (if logind is not available):

```bash
sudo mkdir -p /run/user/$(id -u)
sudo chown $(whoami):$(whoami) /run/user/$(id -u)
sudo chmod 700 /run/user/$(id -u)
export XDG_RUNTIME_DIR=/run/user/$(id -u)
```

To make the export permanent, add to `~/.bash_profile`:

```bash
export XDG_RUNTIME_DIR=/run/user/$(id -u)
```

---

## 7. Configuration

Copy the example config:

```bash
mkdir -p ~/.config/hypr
cp example/hyprland.conf ~/.config/hypr/hyprland.conf
```

Set your monitor in the config (required to avoid corrupted refresh rates):

```bash
echo "monitor=,1920x1080@60,0x0,1" >> ~/.config/hypr/hyprland.conf
```

For VirtualBox specifically:
```bash
echo "monitor=Virtual-1,1280x720@60,0x0,1" >> ~/.config/hypr/hyprland.conf
```

---

## 8. First Launch

### Option A — Direct from TTY (recommended, real GPU)

Log in at a TTY (not inside another compositor), then:

```bash
export XDG_RUNTIME_DIR=/run/user/$(id -u)
./build/Hyprland --config ~/.config/hypr/hyprland.conf
```

### Option B — Nested inside Xorg (for testing without seat control)

```bash
# Install Xorg if missing
sudo pacman -S xorg-server xorg-xinit

# Create .xinitrc
cat > ~/.xinitrc << 'EOF'
export XDG_RUNTIME_DIR=/run/user/$(id -u)
export WLR_BACKENDS=x11
export WLR_NO_HARDWARE_CURSORS=1
exec /home/$USER/spatial-hypr/build/Hyprland --config ~/.config/hypr/hyprland.conf
EOF

startx
```

Hyprland opens as a **window inside Xorg** — no DRM or seat required.

### Option C — Nested inside another Wayland compositor

```bash
export XDG_RUNTIME_DIR=/run/user/$(id -u)
export WAYLAND_DISPLAY=wayland-0   # your current compositor's socket
export WLR_BACKENDS=wayland
./build/Hyprland --config ~/.config/hypr/hyprland.conf
```

---

## 9. Verify Spatial Features

Once Hyprland is running, confirm the spatial layer system initialized:

```bash
cat ~/.cache/hyprland/hyprland.log | grep -i "ZSpace\|SPATIAL\|spatial"
```

Expected output:
```
DEBUG ]: Creating the ZSpaceManager!
```

### Z-Layer Navigation

| Action | Effect |
|--------|--------|
| Scroll wheel up | Move to next Z layer (deeper) |
| Scroll wheel down | Move to previous Z layer (closer) |

### Layer Depths

| Layer | Name | Z Position | Opacity | Blur |
|-------|------|-----------|---------|------|
| 0 | Foreground | 0.0 | 1.00 | 0px |
| 1 | Near | -800.0 | 0.75 | 1.5px |
| 2 | Mid | -1600.0 | 0.40 | 5px |
| 3 | Far | -2800.0 | 0.15 | 12px |

---

## 10. Launch Apps Inside the Compositor

```bash
# Set WAYLAND_DISPLAY to Hyprland's socket (check ~/.cache/hyprland/hyprland.log)
export WAYLAND_DISPLAY=wayland-1

# Launch a terminal
foot &

# Launch a browser
firefox &
```

---

## Troubleshooting

### `XDG_RUNTIME_DIR not set` / `Permission denied /run/user/1000/hypr`
```bash
sudo mkdir -p /run/user/$(id -u)
sudo chown $(whoami):$(whoami) /run/user/$(id -u)
sudo chmod 700 /run/user/$(id -u)
export XDG_RUNTIME_DIR=/run/user/$(id -u)
```

### `libseat: failed to open a seat` / `Only owner of session may take control`
You are running over SSH — Hyprland must be launched **from the physical TTY**, not an SSH session. Or use nested mode (`WLR_BACKENDS=x11`).

### `EGL_BAD_MATCH: dri2_create_context` (VirtualBox)
```bash
MESA_LOADER_DRIVER_OVERRIDE=kms_swrast \
WLR_NO_HARDWARE_CURSORS=1 \
./build/Hyprland --config ~/.config/hypr/hyprland.conf
```

### `Cannot commit when a page-flip is awaiting` (VirtualBox)
```bash
AQ_DRM_NO_ATOMIC=1 \
./build/Hyprland --config ~/.config/hypr/hyprland.conf
```

### NVIDIA GPU
```bash
GBM_BACKEND=nvidia-drm \
__GLX_VENDOR_LIBRARY_NAME=nvidia \
WLR_NO_HARDWARE_CURSORS=1 \
./build/Hyprland --config ~/.config/hypr/hyprland.conf
```

### `clang is not a full path and was not found`
CMake cannot locate clang in your `PATH`. Fix:
```bash
# Install clang
sudo pacman -S clang        # Arch
# sudo dnf install clang    # Fedora

# Verify it is on PATH
which clang clang++

# Re-run cmake using the full path
cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=$(which clang++) \
    -DCMAKE_C_COMPILER=$(which clang)
```

### Check crash report
```bash
cat ~/.cache/hyprland/hyprlandCrashReport*.txt | tail -50
```

### Check full log
```bash
cat ~/.cache/hyprland/hyprland.log | tail -100
```

---

## Running Tests

```bash
# Run spatial-specific tests only
ctest --test-dir build -R "spatial" --output-on-failure

# Run all tests
ctest --test-dir build --output-on-failure
```

---

## Kernel Module Requirements

For DRM to work, the correct GPU kernel module must be loaded:

| GPU | Module |
|-----|--------|
| Intel | `i915` or `xe` |
| AMD | `amdgpu` |
| NVIDIA | `nvidia-drm` |
| VMware/VirtualBox | `vmwgfx` |

Verify:
```bash
ls /dev/dri/
# Expected: card0  renderD128
```

If `/dev/dri/` is empty:
```bash
sudo modprobe <module>
# e.g.: sudo modprobe vmwgfx
```

If modules directory is missing entirely:
```bash
sudo pacman -S linux linux-headers
sudo reboot
```

---

*Document: BUILD_AND_INSTALL.md*  
*Fork: spatial-hypr (Hyprland v0.45.x + Spatial OS Z-layer system)*
