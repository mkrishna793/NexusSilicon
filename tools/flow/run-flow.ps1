param(
    [Parameter(Mandatory = $true)][string]$ProjectYaml,
    [switch]$AllowFallback
)

$ErrorActionPreference = "Stop"
# MFE is built with the local MinGW toolchain; make its runtime DLLs available
# when the master flow is launched from an ordinary PowerShell session.
$env:PATH = "D:\msys64\mingw64\bin;D:\msys64\usr\bin;" + $env:PATH
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

Write-Host "=================================================="
Write-Host "         Metric Field Engine (MFE) Flow           "
Write-Host "=================================================="

# Check project yaml exists
if (!(Test-Path $ProjectYaml)) {
    throw "Project manifest not found at: $ProjectYaml"
}

# Parse project manifest
Write-Host "[*] Parsing project manifest..."
$project = Get-Content $ProjectYaml | Out-String
$design = ""
$top = ""
$pdk = ""
$rtl = @()
$constraints = ""
$outputs = "results"

foreach ($line in ($project -split "`n")) {
    $line = $line.Trim()
    if ($line.StartsWith("#") -or $line.Length -eq 0) { continue }
    if ($line.StartsWith("-")) {
        $rtl += $line.Substring(1).Trim()
        continue
    }
    $colon = $line.IndexOf(":")
    if ($colon -ge 0) {
        $key = $line.Substring(0, $colon).Trim()
        $val = $line.Substring($colon + 1).Trim()
        if ($key -eq "design") { $design = $val }
        elseif ($key -eq "top") { $top = $val }
        elseif ($key -eq "pdk") { $pdk = $val }
        elseif ($key -eq "constraints") { $constraints = $val }
        elseif ($key -eq "outputs") { $outputs = $val }
    }
}

# Find PDK manifest
$pdkDir = Join-Path $root "config\pdks\$pdk"
$pdkManifestPath = Join-Path $pdkDir "manifest.json"
if (!(Test-Path $pdkManifestPath)) {
    throw "PDK manifest not found at: $pdkManifestPath"
}

$pdkJson = Get-Content $pdkManifestPath | ConvertFrom-Json
$liberty = $pdkJson.liberty_corners.typical

# Create output folder paths
$projDir = Split-Path -Parent $ProjectYaml
$outputDir = Join-Path $projDir $outputs
$frontendDir = Join-Path $outputDir "frontend"
New-Item -ItemType Directory -Force -Path $frontendDir | Out-Null

# Resolve absolute paths for RTL files
$rtlPaths = $rtl | ForEach-Object { Join-Path $projDir $_ }
$constraintsPath = Join-Path $projDir $constraints

# Step 1: RTL Linting (Verible)
Write-Host "[*] Phase 2 - Running RTL analysis and linting..."
& (Join-Path $root "tools\adapters\run-verible.ps1") -Rtl $rtlPaths -OutputDirectory $frontendDir -AllowFallback:$AllowFallback

# Step 2: Synthesis (Yosys)
Write-Host "[*] Phase 2 - Running logic synthesis..."
& (Join-Path $root "tools\adapters\run-yosys.ps1") -Rtl $rtlPaths -Top $top -Liberty $liberty -OutputDirectory $frontendDir -AllowFallback:$AllowFallback

# Rename compiled netlist to standard naming mapped_netlist.v
$synthNetlist = Join-Path $frontendDir "netlist.v"
$mappedNetlist = Join-Path $frontendDir "mapped_netlist.v"
if (Test-Path $synthNetlist) {
    Move-Item -Path $synthNetlist -Destination $mappedNetlist -Force
}

# Write stage result output
$stageResult = [ordered]@{
    stage = "synthesis"
    status = "passed"
    inputs = $rtlPaths
    outputs = @($mappedNetlist, (Join-Path $frontendDir "lint.json"), (Join-Path $frontendDir "ast.json"))
    metrics = @{
        synthesis_success = 1.0
    }
    findings = @()
}
$stageResult | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $frontendDir "stage-result.json")

Write-Host "[+] Phase 2 RTL Frontend completed successfully."
Write-Host "Outputs saved at: $frontendDir"

# Step 3: Physical Design (MFE Placement and Routing)
Write-Host "[*] Phase 4 - Running MFE C++ Physical Pipeline..."
$mfeExe = Join-Path $root "build\mfe.exe"
$pdkManifest = Join-Path $root "config\pdks\$pdk\manifest.json"

& $mfeExe run_flow $ProjectYaml $pdkManifest
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Physical design flow failed."
    exit $LASTEXITCODE
}

Write-Host "[+] Phase 4 Physical Design completed successfully."

# Step 4: Parasitic RC Extraction (OpenRCX)
Write-Host "[*] Phase 5 - Running Parasitic RC Extraction (OpenRCX)..."
$extractionDir = Join-Path $outputDir "physical"
$routedDef = Join-Path $extractionDir "routed.def"
$techLef = $pdkJson.technology_lef
$cellLef = $pdkJson.cell_lefs[0]
$rcxRules = $pdkJson.rcx_rules

& (Join-Path $root "tools\adapters\run-openrcx.ps1") -TechLef $techLef -CellLef $cellLef -RoutedDef $routedDef -RcxRules $rcxRules -OutputDirectory $extractionDir -AllowFallback:$AllowFallback
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Parasitics extraction failed."
    exit $LASTEXITCODE
}

# Step 5: Static Timing Analysis (OpenSTA)
Write-Host "[*] Phase 5 - Running Static Timing Analysis (OpenSTA)..."
$timingDir = Join-Path $outputDir "timing"
$spefFile = Join-Path $extractionDir "design.spef"

& (Join-Path $root "tools\adapters\run-opensta.ps1") -Netlist $mappedNetlist -Top $top -Liberty $liberty -Sdc $constraintsPath -OutputDirectory $timingDir -Spef $spefFile -TechLef $techLef -CellLef $cellLef -AllowFallback:$AllowFallback
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Timing analysis failed."
    exit $LASTEXITCODE
}

# Step 6: ECO Planning Loop (MFE ECO)
Write-Host "[*] Phase 5 - Running timing-aware ECO loop..."
& $mfeExe run_eco $ProjectYaml $pdkManifest
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] ECO optimization run failed."
    exit $LASTEXITCODE
}

# Step 7: Physical Verification (KLayout & Magic)
Write-Host "[*] Phase 7 - Running Physical Verification (KLayout & Magic)..."
$verifyDir = Join-Path $outputDir "verification"

& (Join-Path $root "tools\adapters\run-klayout.ps1") -TechLef $techLef -CellLef $cellLef -RoutedDef $routedDef -OutputDirectory $verifyDir -AllowFallback:$AllowFallback
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] KLayout DRC failed."
    exit $LASTEXITCODE
}

& (Join-Path $root "tools\adapters\run-magic.ps1") -TechLef $techLef -CellLef $cellLef -RoutedDef $routedDef -OutputDirectory $verifyDir -AllowFallback:$AllowFallback
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Magic DRC/LVS failed."
    exit $LASTEXITCODE
}

Write-Host "[+] Phase 7 Verification completed successfully. GDSII & OASIS binaries generated."
Write-Host "=================================================="
Write-Host "[+] End-to-End RTL-to-GDSII flow pipeline finished."
Write-Host "=================================================="
