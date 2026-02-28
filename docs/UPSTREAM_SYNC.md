# Upstream Sync Guide — spatial-hypr

This document explains how to synchronize the `spatial-hypr` fork with the upstream [hyprwm/Hyprland](https://github.com/hyprwm/Hyprland) repository without losing spatial OS changes.

---

## Overview

The fork maintains two distinct sets of commits on the `Space-Z` branch:

| Type | Prefix | Description |
|------|--------|-------------|
| Upstream | *(none / `[FORK]`)* | Unmodified Hyprland code |
| Spatial | `[SPATIAL]` | New files and modifications for Spatial OS |

The sync strategy is **cherry-pick rebase**: after pulling upstream changes onto a new branch, all `[SPATIAL]`-prefixed commits are cherry-picked on top, then a PR is opened for review.

---

## Quick Start

```bash
# Make sure you are on Space-Z and the tree is clean
git checkout Space-Z
git status   # must show nothing to commit

# Run the sync script
./scripts/sync-upstream.sh

# Or preview without making any changes
./scripts/sync-upstream.sh --dry-run

# Sync without opening a PR (e.g., manual review)
./scripts/sync-upstream.sh --no-pr
```

The script will:
1. Add the `upstream` remote if absent
2. Fetch `upstream/main`
3. Create branch `upstream-sync-YYYYMMDD` from upstream HEAD
4. Cherry-pick all `[SPATIAL]` commits
5. Validate the build + shaders + spatial tests
6. Push the branch and open a GitHub PR (requires `gh` CLI)

---

## Step-by-Step Manual Procedure

When the automated script cannot resolve conflicts, follow these steps manually.

### 1 — Set up the upstream remote (one-time)

```bash
git remote add upstream https://github.com/hyprwm/Hyprland.git
git fetch upstream main
```

### 2 — Identify the merge base

```bash
# Find the last common commit between Space-Z and upstream/main
git merge-base HEAD upstream/main
```

### 3 — Collect all [SPATIAL] commits

```bash
# List every commit with [SPATIAL] prefix, oldest first
git log --oneline --reverse "$(git merge-base HEAD upstream/main)..HEAD" \
    | grep '\[SPATIAL\]'
```

Save this list — these are the commits to replay.

### 4 — Create the sync branch

```bash
git checkout -b upstream-sync-$(date +%Y%m%d) upstream/main
```

### 5 — Cherry-pick spatial commits

```bash
# Example — replace with actual SHAs from step 3
git cherry-pick abc1234 def5678 ...
```

If a conflict occurs:
```bash
# Edit the conflicting files, then:
git add <resolved-files>
git cherry-pick --continue
```

For conflicts in upstream-owned files (`Renderer.cpp`, `InputManager.cpp`, etc.), keep **both** the upstream change and the `[SPATIAL]` addition. Never discard upstream logic.

### 6 — Validate the build

```bash
cmake -B build-sync -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build-sync -j$(nproc)

# Shaders
glslangValidator -V src/render/shaders/depth_spatial.frag
glslangValidator -V src/render/shaders/depth_dof.frag
glslangValidator -V src/render/shaders/passthrough_ar.frag

# Tests
ctest --test-dir build-sync -R "spatial" --output-on-failure
```

### 7 — Open the PR

```bash
git push origin upstream-sync-$(date +%Y%m%d)
# Then open a PR: upstream-sync-YYYYMMDD → main
```

---

## Commit Prefix Rules

| Prefix | When to use |
|--------|-------------|
| `[SPATIAL]` | **Required** on every commit that adds or modifies Spatial OS code |
| `[FORK]` | Commits that adjust build infrastructure for the fork |
| `[CI]` | CI/CD workflow changes |
| `[DOCS]` | Documentation-only changes |

Commits **without** a `[SPATIAL]` or `[FORK]` prefix are treated as upstream changes and will **not** be carried forward during a sync. If you forget the prefix, use:

```bash
git commit --amend -m "[SPATIAL] Your original message"
# or for older commits:
git rebase -i <merge-base>  # and reword the relevant entries
```

---

## Conflict Resolution Reference

### `src/render/Renderer.cpp`

The spatial layer adds:
- Z-space manager initialization in `renderMonitor()`
- `spatialProjection` / `spatialView` matrix upload
- Depth sorting in `renderWorkspaceWindows()`
- `lastFloatingWindow` deferral in the floating pass
- `applySpatialShader()` call in `renderWindow()`

When upstream modifies these same functions, preserve **all** upstream logic and append or integrate the `[SPATIAL]` additions. Look for `// [SPATIAL]` comments to find insertion points.

### `src/managers/input/InputManager.cpp`

The spatial layer routes vertical scroll events through `g_pSpatialInputHandler->processScrollEvent()` and adds a modifier guard. Keep this routing; upstream changes typically affect other input paths.

### `src/Compositor.cpp`

The spatial layer instantiates `g_pZSpaceManager` and `g_pSpatialInputHandler` and registers the layer-change callback. These additions are append-only and rarely conflict.

### `src/desktop/view/Window.hpp`

The spatial layer adds `SSpatialProps m_sSpatialProps` as an aggregate member. This is append-only and should never conflict unless upstream adds a member with the same name.

---

## Files Modified by the Fork

The following upstream files carry `[SPATIAL]` modifications. Pay extra attention during sync:

```
src/Compositor.hpp            — g_pZSpaceManager / g_pSpatialInputHandler globals
src/Compositor.cpp            — instantiation + layer-change callback registration
src/render/Renderer.cpp       — depth sort, projection uniforms, shader dispatch
src/managers/input/InputManager.cpp  — scroll → Z nav routing
src/desktop/view/Window.hpp   — SSpatialProps struct + m_sSpatialProps member
CMakeLists.txt                — spatial/ sources, glm detection, test glob
```

New files (no upstream conflict):
```
src/spatial/ZSpaceManager.hpp/.cpp
src/spatial/SpatialConfig.hpp/.cpp
src/spatial/SpatialInputHandler.hpp/.cpp
src/render/shaders/depth_spatial.frag
src/render/shaders/depth_dof.frag
src/render/shaders/passthrough_ar.frag
tests/spatial/ZSpaceManagerTest.cpp
tests/spatial/SpatialConfigTest.cpp
tests/spatial/SpatialInputHandlerTest.cpp
docs/SPATIAL_HYPR_FORK_SPEC.md
docs/SPATIAL_OS_SPEC.md
docs/SPATIAL_CHANGES.md
.github/workflows/spatial-build.yml
scripts/sync-upstream.sh
```

---

## Checking Which Upstream Files Were Modified

```bash
# All commits on Space-Z that touch upstream-owned files
git log --oneline upstream/main..HEAD -- \
    src/Compositor.cpp \
    src/Compositor.hpp \
    src/render/Renderer.cpp \
    src/managers/input/InputManager.cpp \
    src/desktop/view/Window.hpp \
    CMakeLists.txt
```

---

## Troubleshooting

**`cherry-pick` stops with conflicts on every sync**

This usually means a `[SPATIAL]` commit modified a large context block that changes frequently upstream. Consider splitting the commit into smaller changes that touch fewer surrounding lines, making future cherry-picks more surgical.

**Build fails after sync**

Check `SPATIAL_CHANGES.md` for any API changes. Common issues:
- Hyprland renames a type (e.g., `PHLWINDOW` definition changes)
- `g_pCompositor->m_windows` changes container type
- Wayland protocol API bump in `aquamarine`

**Shader validation fails**

`glslangValidator` version mismatch is common. Ensure `glslang` is updated:
```bash
sudo pacman -S --noconfirm glslang  # Arch
```

**`gh` CLI not found**

Install with `sudo pacman -S github-cli` or create the PR manually on GitHub after pushing the branch.
