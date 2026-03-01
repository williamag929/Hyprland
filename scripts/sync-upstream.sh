#!/usr/bin/env bash
# =============================================================================
# [SPATIAL] sync-upstream.sh
# Synchronizes the spatial-hypr fork with the upstream Hyprland repository.
#
# Usage:
#   ./scripts/sync-upstream.sh [--dry-run] [--no-pr]
#
# What it does:
#   1. Adds upstream remote if not present
#   2. Fetches upstream/main
#   3. Creates a dated branch: upstream-sync-YYYYMMDD
#   4. Rebases [SPATIAL] commits onto the new upstream HEAD
#   5. Runs a quick cmake build validation
#   6. Opens a GitHub PR (requires gh CLI) unless --no-pr is passed
#
# Requirements:
#   git, cmake, clang++, gh (optional, for auto-PR)
# =============================================================================

set -euo pipefail

# ─── options ──────────────────────────────────────────────────────────────────
DRY_RUN=0
NO_PR=0

for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=1 ;;
        --no-pr)   NO_PR=1   ;;
    esac
done

# ─── config ───────────────────────────────────────────────────────────────────
UPSTREAM_REMOTE="upstream"
UPSTREAM_URL="https://github.com/hyprwm/Hyprland.git"
UPSTREAM_BRANCH="main"
SPATIAL_BRANCH_PREFIX="upstream-sync"
DATE_STAMP="$(date +%Y%m%d)"
SYNC_BRANCH="${SPATIAL_BRANCH_PREFIX}-${DATE_STAMP}"
REPO_ROOT="$(git rev-parse --show-toplevel)"
BUILD_DIR="${REPO_ROOT}/build-sync-validation"

# ─── helpers ──────────────────────────────────────────────────────────────────
info()  { echo -e "\033[0;34m[SPATIAL SYNC]\033[0m $*"; }
ok()    { echo -e "\033[0;32m[✓]\033[0m $*"; }
warn()  { echo -e "\033[0;33m[!]\033[0m $*"; }
fail()  { echo -e "\033[0;31m[✗]\033[0m $*" >&2; exit 1; }

run() {
    if [[ $DRY_RUN -eq 1 ]]; then
        echo -e "\033[0;90m[dry-run]\033[0m $*"
    else
        eval "$@"
    fi
}

# ─── 1. upstream remote ───────────────────────────────────────────────────────
info "Step 1/6 — Ensuring upstream remote is configured"
cd "$REPO_ROOT"

if ! git remote get-url "$UPSTREAM_REMOTE" &>/dev/null; then
    info "Adding remote '${UPSTREAM_REMOTE}' → ${UPSTREAM_URL}"
    run git remote add "$UPSTREAM_REMOTE" "$UPSTREAM_URL"
else
    ok "Remote '${UPSTREAM_REMOTE}' already exists"
fi

# ─── 2. fetch upstream ────────────────────────────────────────────────────────
info "Step 2/6 — Fetching upstream/${UPSTREAM_BRANCH}"
run git fetch "$UPSTREAM_REMOTE" "$UPSTREAM_BRANCH" --no-tags
ok "Fetched upstream/${UPSTREAM_BRANCH}"

UPSTREAM_SHA="$(git rev-parse "${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH}")"
info "Upstream HEAD: ${UPSTREAM_SHA:0:12}"

# ─── 3. collect [SPATIAL] commits ────────────────────────────────────────────
info "Step 3/6 — Collecting [SPATIAL] commits on current branch"
CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

# Find the merge-base between current HEAD and upstream
MERGE_BASE="$(git merge-base HEAD "${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH}")"
info "Merge base: ${MERGE_BASE:0:12} (current branch: ${CURRENT_BRANCH})"

# Spatial commits = commits on top of merge-base that have [SPATIAL] prefix
SPATIAL_COMMITS="$(git log --oneline --reverse "${MERGE_BASE}..HEAD" \
    | grep '^\S\+ \[SPATIAL\]' | awk '{print $1}')"

if [[ -z "$SPATIAL_COMMITS" ]]; then
    warn "No [SPATIAL]-prefixed commits found between merge-base and HEAD."
    warn "Nothing to cherry-pick. The fork may already be up to date, or"
    warn "commits may be missing the [SPATIAL] prefix."
    SPATIAL_COUNT=0
else
    SPATIAL_COUNT="$(echo "$SPATIAL_COMMITS" | wc -l | tr -d ' ')"
    ok "Found ${SPATIAL_COUNT} [SPATIAL] commit(s) to carry forward:"
    git log --oneline "${MERGE_BASE}..HEAD" | grep '\[SPATIAL\]' | sed 's/^/  /'
fi

# ─── 4. create sync branch ────────────────────────────────────────────────────
info "Step 4/6 — Creating sync branch '${SYNC_BRANCH}' from upstream HEAD"

if git show-ref --verify --quiet "refs/heads/${SYNC_BRANCH}"; then
    warn "Branch '${SYNC_BRANCH}' already exists."
    warn "Delete it first with:  git branch -D ${SYNC_BRANCH}"
    fail "Aborting to avoid clobbering existing branch."
fi

run git checkout -b "$SYNC_BRANCH" "${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH}"
ok "Created ${SYNC_BRANCH}"

# Cherry-pick each [SPATIAL] commit
if [[ $SPATIAL_COUNT -gt 0 ]]; then
    info "Cherry-picking ${SPATIAL_COUNT} [SPATIAL] commit(s)..."
    FAILED_PICKS=()
    while IFS= read -r sha; do
        [[ -z "$sha" ]] && continue
        MSG="$(git log --oneline -1 "$sha" | cut -d' ' -f2-)"
        if ! run git cherry-pick "$sha" --allow-empty; then
            warn "Cherry-pick of ${sha:0:8} FAILED: ${MSG}"
            warn "Resolve conflicts, then run: git cherry-pick --continue"
            FAILED_PICKS+=("$sha")
        else
            ok "  Picked ${sha:0:8}: ${MSG}"
        fi
    done <<< "$SPATIAL_COMMITS"

    if [[ ${#FAILED_PICKS[@]} -gt 0 ]]; then
        fail "${#FAILED_PICKS[@]} cherry-pick(s) failed. Fix conflicts and re-run."
    fi
fi

# ─── 5. build validation ─────────────────────────────────────────────────────
info "Step 5/6 — Running cmake build validation"

if [[ $DRY_RUN -eq 1 ]]; then
    warn "[dry-run] Skipping build validation"
else
    mkdir -p "$BUILD_DIR"
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_C_COMPILER=clang \
        -DENABLE_SPATIAL=ON \
        -DBUILD_TESTING=ON \
        -S "$REPO_ROOT" \
        > /tmp/spatial-sync-cmake.log 2>&1 \
        || fail "cmake configure failed. See /tmp/spatial-sync-cmake.log"

    cmake --build "$BUILD_DIR" -j"$(nproc)" \
        > /tmp/spatial-sync-build.log 2>&1 \
        || fail "Build failed. See /tmp/spatial-sync-build.log"

    ok "Build succeeded"

    # Validate shaders
    info "Validating GLSL shaders..."
    SHADERS=(
        "src/render/shaders/depth_spatial.frag"
        "src/render/shaders/depth_dof.frag"
        "src/render/shaders/passthrough_ar.frag"
    )
    for shader in "${SHADERS[@]}"; do
        if [[ -f "${REPO_ROOT}/${shader}" ]]; then
            glslangValidator "${REPO_ROOT}/${shader}" > /dev/null \
                && ok "  ${shader}" \
                || warn "  SHADER VALIDATION FAILED: ${shader}"
        else
            warn "  Shader not found (skipping): ${shader}"
        fi
    done

    # Run spatial tests
    info "Running spatial test suite..."
    ctest --test-dir "$BUILD_DIR" -R "spatial" --output-on-failure -Q 2>&1 \
        && ok "All spatial tests passed" \
        || warn "Some spatial tests failed — check output above"

    # Cleanup temp build dir
    rm -rf "$BUILD_DIR"
fi

# ─── 6. open PR ───────────────────────────────────────────────────────────────
info "Step 6/6 — Pull request"

if [[ $NO_PR -eq 1 ]]; then
    warn "Skipping PR creation (--no-pr)"
elif [[ $DRY_RUN -eq 1 ]]; then
    warn "[dry-run] Would push ${SYNC_BRANCH} and open PR"
elif ! command -v gh &>/dev/null; then
    warn "gh CLI not found — skipping automatic PR creation."
    info "Push manually and open a PR:"
    info "  git push origin ${SYNC_BRANCH}"
    info "  # Then open PR: ${SYNC_BRANCH} → main"
else
    run git push origin "$SYNC_BRANCH"

    UPSTREAM_SHORT="${UPSTREAM_SHA:0:8}"
    PR_TITLE="[SPATIAL] Upstream sync — Hyprland@${UPSTREAM_SHORT} (${DATE_STAMP})"
    PR_BODY="## Upstream Sync\n\nAutomatic synchronization with [hyprwm/Hyprland](https://github.com/hyprwm/Hyprland) HEAD \`${UPSTREAM_SHA}\`.\n\n### Spatial commits carried forward\n\`\`\`\n$(git log --oneline "${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH}..HEAD" | grep '\[SPATIAL\]')\n\`\`\`\n\n### Checklist\n- [ ] Build passes\n- [ ] All spatial tests pass\n- [ ] No regressions in existing Wayland behaviour\n- [ ] SPATIAL_CHANGES.md updated if API changed"

    run gh pr create \
        --title "$PR_TITLE" \
        --body "$PR_BODY" \
        --base main \
        --head "$SYNC_BRANCH" \
        --label "upstream-sync"

    ok "PR created"
fi

# ─── done ─────────────────────────────────────────────────────────────────────
echo ""
ok "Upstream sync complete."
info "Branch:   ${SYNC_BRANCH}"
info "Commits:  ${SPATIAL_COUNT} [SPATIAL] commit(s) cherry-picked"
if [[ $DRY_RUN -eq 0 && $NO_PR -eq 0 ]]; then
    info "Next:     Review the PR, resolve any conflicts, then merge."
else
    info "Next:     git push origin ${SYNC_BRANCH}  (then open PR manually)"
fi
