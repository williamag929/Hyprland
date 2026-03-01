# ══════════════════════════════════════════════════════════════════════════════
# Spatial Hyprland — Shader Validation Script (Windows PowerShell)
# Validates all GLSL shaders before compilation
# ══════════════════════════════════════════════════════════════════════════════

param(
    [string]$ShaderDir = "src\render\shaders",
    [switch]$ShowDetails = $false
)

$ErrorActionPreference = "Stop"

# ────────────────────────────────────────────────────────────────────────────
# Check if validator is installed
# ────────────────────────────────────────────────────────────────────────────

$validator = $null
try {
    $validator = Get-Command glslangValidator -ErrorAction Stop
} catch {
    Write-Host "❌ glslangValidator not found" -ForegroundColor Red
    Write-Host ""
    Write-Host "Installation instructions:"
    Write-Host "  • Windows (pre-built): Download from https://github.com/KhronosGroup/glslang/releases"
    Write-Host "  • Or in Docker container (recommended):"
    Write-Host "      docker exec spatial-dev bash -c 'glslangValidator -G src/render/shaders/*.frag'"
    exit 1
}

Write-Host "📋 GLSL Shader Validator (Windows)" -ForegroundColor Blue
Write-Host "Validator: $($validator.Source)"
Write-Host "Checking shaders in: $ShaderDir"
Write-Host ""

# ────────────────────────────────────────────────────────────────────────────
# Find and validate all GLSL shaders
# ────────────────────────────────────────────────────────────────────────────

$shaderExtensions = @("*.frag", "*.vert", "*.geom", "*.comp", "*.tesc", "*.tese")
$shaders = @()

foreach ($ext in $shaderExtensions) {
    $shaders += @(Get-ChildItem -Path $ShaderDir -Filter $ext -ErrorAction SilentlyContinue)
}

if ($shaders.Count -eq 0) {
    Write-Host "⚠️  No shaders found in $ShaderDir" -ForegroundColor Yellow
    exit 0
}

$shadersValid = 0
$shadersInvalid = 0
$validationErrors = @()

foreach ($shader in $shaders | Sort-Object Name) {
    $shaderName = $shader.Name
    Write-Host -NoNewline "  $($shaderName.PadRight(35)) ... "
    
    try {
        $output = & glslangValidator -G $shader.FullName 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅" -ForegroundColor Green
            $shadersValid++
        } else {
            Write-Host "❌ FAILED" -ForegroundColor Red
            $shadersInvalid++
            $validationErrors += @{
                File = $shaderName
                Error = $output
            }
        }
    } catch {
        Write-Host "❌ ERROR" -ForegroundColor Red
        $shadersInvalid++
        $validationErrors += @{
            File = $shaderName
            Error = $_.Exception.Message
        }
    }
}

# ────────────────────────────────────────────────────────────────────────────
# Show detailed errors if requested
# ────────────────────────────────────────────────────────────────────────────

if ($validationErrors.Count -gt 0 -and $ShowDetails) {
    Write-Host ""
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Red
    Write-Host "VALIDATION ERROR DETAILS" -ForegroundColor Red
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Red
    
    foreach ($error in $validationErrors) {
        Write-Host ""
        Write-Host "File: $($error.File)" -ForegroundColor Yellow
        Write-Host "Error:" -ForegroundColor Red
        $error.Error | ForEach-Object { Write-Host "  $_" }
    }
}

# ────────────────────────────────────────────────────────────────────────────
# Summary
# ────────────────────────────────────────────────────────────────────────────

Write-Host ""
Write-Host "─────────────────────────────────────────────────────────────────────" -ForegroundColor Cyan
Write-Host "📊 Validation Summary:" -ForegroundColor Cyan
Write-Host "   Total shaders found:   $($shaders.Count)"
Write-Host "   Valid shaders:         $('{0}' -f $shadersValid)" -ForegroundColor Green
if ($shadersInvalid -gt 0) {
    Write-Host "   Invalid shaders:       $($shadersInvalid)" -ForegroundColor Red
} else {
    Write-Host "   Invalid shaders:       $($shadersInvalid)" -ForegroundColor Green
}
Write-Host "─────────────────────────────────────────────────────────────────────" -ForegroundColor Cyan

# ────────────────────────────────────────────────────────────────────────────
# Exit with appropriate code
# ────────────────────────────────────────────────────────────────────────────

if ($shadersInvalid -eq 0) {
    Write-Host "✅ All shaders validated successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "❌ Shader validation failed!" -ForegroundColor Red
    exit 1
}
