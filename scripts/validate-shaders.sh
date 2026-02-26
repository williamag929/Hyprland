#!/bin/bash
# ══════════════════════════════════════════════════════════════════════════════
# Spatial Hyprland — Shader Validation Script
# Validates all GLSL shaders before compilation
# ══════════════════════════════════════════════════════════════════════════════

set -e

VALIDATOR="glslangValidator"
SHADER_DIR="src/render/shaders"
SHADERS_FOUND=0
SHADERS_VALID=0
SHADERS_INVALID=0

# Color codes for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ────────────────────────────────────────────────────────────────────────────
# Check if validator is installed
# ────────────────────────────────────────────────────────────────────────────
if ! command -v "$VALIDATOR" &> /dev/null; then
    echo -e "${RED}❌ glslangValidator not found${NC}"
    echo ""
    echo "Installation instructions:"
    echo "  • Arch Linux:     sudo pacman -S glslang"
    echo "  • Ubuntu/Debian:  sudo apt install glslang-tools"
    echo "  • macOS:          brew install glslang"
    exit 1
fi

VALIDATOR_VERSION=$($VALIDATOR --version 2>&1 | head -1)
echo -e "${BLUE}📋 GLSL Shader Validator${NC}"
echo "Validator: $VALIDATOR_VERSION"
echo "Checking shaders in: $SHADER_DIR"
echo ""

# ────────────────────────────────────────────────────────────────────────────
# Find and validate all GLSL shaders
# ────────────────────────────────────────────────────────────────────────────
for shader in "$SHADER_DIR"/*.{frag,vert,geom,comp,tesc,tese} 2>/dev/null; do
    # Skip if no files match pattern
    [ -f "$shader" ] || continue
    
    SHADERS_FOUND=$((SHADERS_FOUND + 1))
    SHADER_NAME=$(basename "$shader")
    
    printf "  %-35s ... " "$SHADER_NAME"
    
    # Run validation and capture output
    if VALIDATION_OUTPUT=$($VALIDATOR -V "$shader" 2>&1); then
        echo -e "${GREEN}✅${NC}"
        SHADERS_VALID=$((SHADERS_VALID + 1))
    else
        echo -e "${RED}❌ FAILED${NC}"
        SHADERS_INVALID=$((SHADERS_INVALID + 1))
        
        # Show detailed error
        echo -e "     ${RED}Error details:${NC}"
        echo "$VALIDATION_OUTPUT" | sed 's/^/     /'
        echo ""
    fi
done

# ────────────────────────────────────────────────────────────────────────────
# Summary
# ────────────────────────────────────────────────────────────────────────────
echo ""
echo "─────────────────────────────────────────────────────────────────────"
echo -e "📊 Validation Summary:"
echo "   Total shaders found:   $SHADERS_FOUND"
echo -e "   Valid shaders:       ${GREEN}$SHADERS_VALID${NC}"
if [ $SHADERS_INVALID -gt 0 ]; then
    echo -e "   Invalid shaders:     ${RED}$SHADERS_INVALID${NC}"
else
    echo -e "   Invalid shaders:     ${GREEN}$SHADERS_INVALID${NC}"
fi
echo "─────────────────────────────────────────────────────────────────────"

# ────────────────────────────────────────────────────────────────────────────
# Exit with appropriate code
# ────────────────────────────────────────────────────────────────────────────
if [ $SHADERS_INVALID -eq 0 ] && [ $SHADERS_FOUND -gt 0 ]; then
    echo -e "${GREEN}✅ All shaders validated successfully!${NC}"
    exit 0
elif [ $SHADERS_FOUND -eq 0 ]; then
    echo -e "${YELLOW}⚠️  No shaders found in $SHADER_DIR${NC}"
    exit 0
else
    echo -e "${RED}❌ Shader validation failed!${NC}"
    exit 1
fi
