# Spatial Hyprland - Complete Documentation Index

> All resources created during this development session (Feb 26, 2026)  
> **Branch:** Space-Z | **Project:** Spatial OS integration into Hyprland v0.45.x

---

## 📚 Documentation Roadmap

### Start Here
| Document | Purpose | Read time |
|----------|---------|-----------|
| 📄 [THIS FILE](#) | Navigation guide | 3 min |
| 🚀 [ACTIVITY_SUMMARY.md](ACTIVITY_SUMMARY.md) | What we accomplished & current status | 10 min |
| ⚡ [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md) | Copy-paste build commands | 5 min |

### Setup & Installation
| Document | Best For | Read time |
|-----------|----------|-----------|
| 📖 [README-SPATIAL.md](README-SPATIAL.md) | Complete setup guide (8 sections) | 30 min |
| 🔧 [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) | Fixing build errors | 20 min |
| 🐳 [docs/DOCKER_TESTING.md](docs/DOCKER_TESTING.md) | Docker-specific instructions | 15 min |

### Technical Deep-Dives
| Document | Topic | Read time |
|----------|-------|-----------|
| 🏗️ [docs/CMAKE_ANALYSIS.md](docs/CMAKE_ANALYSIS.md) | Build system internals | 25 min |
| 🎨 [docs/SHADER_REFERENCE.md](docs/SHADER_REFERENCE.md) | Graphics pipeline & shaders | 30 min |
| 💻 [docs/CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md) | Code quality & architecture | 45 min |
| 📋 [docs/SPATIAL_HYPR_FORK_SPEC.md](docs/SPATIAL_HYPR_FORK_SPEC.md) | Fork modifications spec | 20 min |
| 🌍 [docs/SPATIAL_OS_SPEC.md](docs/SPATIAL_OS_SPEC.md) | System-wide architecture | 40 min |

### Validation & Testing
| Document | Purpose | Read time |
|----------|---------|-----------|
| ✅ [scripts/validate-shaders.sh](scripts/validate-shaders.sh) | Bash shader validator (pre-build) | 2 min |
| ✅ [scripts/validate-shaders.ps1](scripts/validate-shaders.ps1) | PowerShell shader validator | 2 min |

---

## 🎯 Quick Navigation by Task

### "I just want to build Spatial Hyprland"
1. Read: [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md) (5 min)
2. Follow build path that matches your system:
   - **Linux Arch:** Section "Recommended: Native Arch Linux"
   - **Windows/WSL2:** Section "Windows/macOS (Via WSL2 or VM)"
   - **Docker:** Section "Docker Container Build"
3. Run commands, wait 40-90 minutes depending on path
4. Done!

**Expected result:** `./build/Hyprland --version` works

---

### "I need to understand the architecture"
1. Start: [docs/SPATIAL_OS_SPEC.md](docs/SPATIAL_OS_SPEC.md) (40 min)
   - What is Spatial OS?
   - How does Z-depth work?
   - System-wide design
2. Then: [docs/SPATIAL_HYPR_FORK_SPEC.md](docs/SPATIAL_HYPR_FORK_SPEC.md) (20 min)
   - What changed in Hyprland?
   - Module integration points
   - Configuration format
3. Optional: [docs/CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md) (45 min)
   - Code quality assessment
   - Design patterns used
   - Known improvements needed

---

### "The build is failing, help!"
1. Check error type:
   - **CMake error?** → [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) section "Common Build Errors"
   - **Missing packages?** → [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) section "Problem: AUR Dependencies Missing"
   - **Linker error?** → [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) section "Linker Error"
   - **Docker issue?** → [docs/DOCKER_TESTING.md](docs/DOCKER_TESTING.md) section "Troubleshooting"
2. Find matching error description
3. Follow solution step-by-step

---

### "I want to modify the Spatial module"
1. Architecture overview: [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md) section "Project Structure"
2. Build commands: [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md) section "Compilation Workflow"
3. For Z-space changes: [docs/CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md) (find ZSpaceManager section)
4. For shaders: [docs/SHADER_REFERENCE.md](docs/SHADER_REFERENCE.md)
5. For CMake: [docs/CMAKE_ANALYSIS.md](docs/CMAKE_ANALYSIS.md) section "Adding New Shaders"

---

### "I need to validate what we built"
1. Pre-build validation: `bash scripts/validate-shaders.sh`
2. Post-build testing: [README-SPATIAL.md](README-SPATIAL.md) section "Phase 8: Testing & Validation"
3. Memory checking: [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md) section "Debugging"
4. Code review: [docs/CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md)

---

### "I'm setting up for the team/CI"
1. Review: [ACTIVITY_SUMMARY.md](ACTIVITY_SUMMARY.md) section "Docker Pre-Built Image"
2. Build Docker image once: `docker build -f Dockerfile.spatial-dev -t spatial-hypr:dev .` (90 min)
3. Document for team: [README-SPATIAL.md](README-SPATIAL.md) section "Installation Steps"
4. CI/CD: Docker image is reusable, only compile step varies per build

---

## 📊 Document Statistics

### By Type
| Type | Count | Total Lines | Purpose |
|------|-------|-------------|---------|
| **Setup/Guide** | 4 | ~4500 | Installation & troubleshooting |
| **Technical** | 5 | ~2000 | Architecture & deep dives |
| **Scripts** | 2 | ~200 | Validation & build helpers |
| **Index** | 1 | ~400 | This navigation guide |
| **Total** | 12 | ~7100 | Complete documentation |

### By Size
| Document | Lines | Read Time | Importance |
|----------|-------|-----------|------------|
| [README-SPATIAL.md](README-SPATIAL.md) | 917 | 30 min | ⭐⭐⭐ Essential |
| [CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md) | 850 | 45 min | ⭐⭐⭐ For reviewers/archi |
| [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) | 400+ | 20 min | ⭐⭐⭐ For errors |
| [SHADER_REFERENCE.md](docs/SHADER_REFERENCE.md) | 500+ | 30 min | ⭐⭐ Graphics devs |
| [CMAKE_ANALYSIS.md](docs/CMAKE_ANALYSIS.md) | 450+ | 25 min | ⭐⭐ Build system |
| [SPATIAL_OS_SPEC.md](docs/SPATIAL_OS_SPEC.md) | 400+ | 40 min | ⭐⭐⭐ Architecture |
| [SPATIAL_HYPR_FORK_SPEC.md](docs/SPATIAL_HYPR_FORK_SPEC.md) | 350+ | 20 min | ⭐⭐⭐ Technical |
| [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md) | 900+ | 5 min | ⭐⭐⭐ Quick lookup |
| [ACTIVITY_SUMMARY.md](ACTIVITY_SUMMARY.md) | 500+ | 10 min | ⭐⭐⭐ Status report |
| [DOCKER_TESTING.md](docs/DOCKER_TESTING.md) | 300+ | 15 min | ⭐⭐ Docker users |
| validate-shaders.{sh,ps1} | 200 | 2 min | ⭐⭐ Pre-build checks |

---

## 🗂️ File Organization

```
spatial-hypr/
├── 📄 README-SPATIAL.md                          ← START HERE (setup)
├── 📄 ACTIVITY_SUMMARY.md                        ← Status report
├── 📄 DEVELOPER_QUICK_REF.md                     ← Copy-paste commands
├── 📄 BUILD_TROUBLESHOOTING.md                   ← Error solutions
├── 📄 INDEX.md                                   ← THIS FILE
│
├── docs/
│   ├── 📄 SPATIAL_OS_SPEC.md                    ← Architecture (read first)
│   ├── 📄 SPATIAL_HYPR_FORK_SPEC.md             ← Fork details
│   ├── 📄 CODE_REVIEW_SPATIAL.md                ← Code analysis (85/100)
│   ├── 📄 CMAKE_ANALYSIS.md                     ← Build system docs
│   ├── 📄 SHADER_REFERENCE.md                   ← Graphics pipeline
│   ├── 📄 DOCKER_TESTING.md                     ← Docker guide (updated)
│   ├── SPATIAL_STATUS.md                        ← Project status
│   ├── SPATIAL_CHANGES.md                       ← Git commit log
│   └── other upstream docs...
│
├── scripts/
│   ├── 🔧 validate-shaders.sh                   ← Bash validator (NEW)
│   ├── 🔧 validate-shaders.ps1                  ← PowerShell validator (NEW)
│   ├── generateShaderIncludes.sh                ← Shader generator
│   └── ...
│
├── src/
│   ├── spatial/                                 ← Spatial OS module
│   │   ├── ZSpaceManager.{hpp,cpp}
│   │   ├── SpatialConfig.{hpp,cpp}
│   │   └── SpatialInputHandler.{hpp,cpp}
│   ├── render/shaders/
│   │   ├── depth_spatial.frag
│   │   ├── depth_dof.frag
│   │   └── passthrough_ar.frag
│   └── ...
│
└── CMakeLists.txt                              ← Build configuration
```

---

## 📈 Recommended Reading Order

### For First-Time Setup
```
1. ACTIVITY_SUMMARY.md (10 min) - Learn what was done
2. DEVELOPER_QUICK_REF.md (5 min) - Choose build path
3. README-SPATIAL.md (30 min) - Follow setup steps
4. Run: cmake -B build ... && cmake --build build (20-40 min)
5. Done! 🎉
```
**Total time:** 1 hour

### For Developers
```
1. DEVELOPER_QUICK_REF.md (5 min) - Quick reference
2. SPATIAL_OS_SPEC.md (40 min) - Understand system
3. SPATIAL_HYPR_FORK_SPEC.md (20 min) - Understand changes
4. CODE_REVIEW_SPATIAL.md (45 min) - Code deep-dive
5. CMAKE_ANALYSIS.md (25 min) - Build internals
```
**Total time:** 2.5 hours

### For Code Reviewers
```
1. ACTIVITY_SUMMARY.md (10 min) - Projects overview
2. CODE_REVIEW_SPATIAL.md (45 min) - Detailed analysis
3. SHADER_REFERENCE.md (30 min) - Graphics validation
4. CMAKE_ANALYSIS.md (25 min) - Build correctness
```
**Total time:** 1.75 hours

### For Architecture/Design Review
```
1. SPATIAL_OS_SPEC.md (40 min) - System design
2. SPATIAL_HYPR_FORK_SPEC.md (20 min) - Fork architecture
3. CODE_REVIEW_SPATIAL.md - if specific questions
```
**Total time:** 1 hour

---

## ✅ Pre-Build Checklist

Before running `cmake` and `cmake --build`, verify:

- [ ] You've read [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md) or [README-SPATIAL.md](README-SPATIAL.md)
- [ ] You have a supported system:
  - [ ] Native Arch Linux, OR
  - [ ] WSL2 with Arch, OR
  - [ ] Docker with image built
- [ ] You've chosen a build path (Solution 1, 2, or 3)
- [ ] Dependencies are installed (yay successful or Docker image ready)
- [ ] You have 10GB+ free disk space
- [ ] You have 1-2 hours available

---

## 🚀 Quick Start Copy-Paste

### Native Arch Linux (Fastest)
```bash
# Prerequisites (if not installed)
sudo pacman -Sy && sudo pacman -S yay

# Install all dependencies at once
yay -S aquamarine-git hyprlang-git libhyprutils-git hyprcursor-git \
       hyprgraphics-git hyprwayland-scanner-git hyprwire-git --noconfirm

# Setup build
cd spatial-hypr
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)

# Test
ctest --test-dir build -R "spatial" --output-on-failure
```
**Expected time:** 40-50 minutes

### WSL2 with Arch
```bash
# One-time setup from PowerShell
wsl --install -d Arch

# Then in WSL terminal
wsl -d Arch bash -c "
  pacman -Syu --noconfirm &&
  pacman -S yay --noconfirm &&
  yay -S aquamarine-git hyprlang-git libhyprutils-git hyprcursor-git \
         hyprgraphics-git hyprwayland-scanner-git hyprwire-git --noconfirm
"

# Build (from WSL)
cd spatial-hypr
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)
```
**Expected time:** 45-55 minutes

---

## 📞 Support Resources

### For Build Issues
→ [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md)

### For Architecture Questions
→ [SPATIAL_OS_SPEC.md](docs/SPATIAL_OS_SPEC.md)

### For Compilation Commands
→ [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md)

### For Code Details
→ [CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md)

### For Docker Specifics
→ [DOCKER_TESTING.md](docs/DOCKER_TESTING.md)

---

## 📝 Document Updates

| Document | Status | Last Updated |
|----------|--------|--------------|
| README-SPATIAL.md | ✅ Complete + Docker notes | Feb 26, 2026 |
| BUILD_TROUBLESHOOTING.md | ✅ New | Feb 26, 2026 |
| DEVELOPER_QUICK_REF.md | ✅ New | Feb 26, 2026 |
| ACTIVITY_SUMMARY.md | ✅ New | Feb 26, 2026 |
| CODE_REVIEW_SPATIAL.md | ✅ Complete | Feb 26, 2026 |
| CMAKE_ANALYSIS.md | ✅ Complete | Feb 26, 2026 |
| SHADER_REFERENCE.md | ✅ Complete | Feb 26, 2026 |
| DOCKER_TESTING.md | ✅ Updated | Feb 26, 2026 |
| validate-shaders.sh | ✅ New | Feb 26, 2026 |
| validate-shaders.ps1 | ✅ New | Feb 26, 2026 |

---

## 🎯 Next Actions

1. **Choose your build path:**
   - Arch Linux → See [README-SPATIAL.md](README-SPATIAL.md), "Installation Steps"
   - WSL2 → See [README-SPATIAL.md](README-SPATIAL.md), "WSL2 Installation"
   - Docker → See [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md), "Solution 3"

2. **Follow the setup steps** (40-50 minutes)

3. **Validate the build:**
   ```bash
   bash scripts/validate-shaders.sh
   ctest --test-dir build -R "spatial" --output-on-failure
   ```

4. **Review what we built:**
   - See [ACTIVITY_SUMMARY.md](ACTIVITY_SUMMARY.md) for overview
   - See [CODE_REVIEW_SPATIAL.md](docs/CODE_REVIEW_SPATIAL.md) for details

---

## 📋 Summary

- ✅ **12 documents** created/updated
- ✅ **~7,100 lines** of documentation
- ✅ **3 solution paths** documented with scripts
- ✅ **Complete code review** (85/100 quality score)
- ✅ **Build system analyzed** with optimization recommendations
- ✅ **Graphics pipeline documented** with performance metrics
- ⚠️ **Docker AUR complexity identified** (solution: use native Arch or WSL2)

**Ready to build?** Start with [DEVELOPER_QUICK_REF.md](DEVELOPER_QUICK_REF.md) or [README-SPATIAL.md](README-SPATIAL.md)!

---

**Last Generated:** February 26, 2026  
**For:** Spatial Hyprland v0.45.x (Space-Z branch)  
**Status:** ✅ Documentation Complete
