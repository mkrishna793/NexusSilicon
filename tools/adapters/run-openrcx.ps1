param(
    [Parameter(Mandatory = $true)][string]$TechLef,
    [Parameter(Mandatory = $true)][string]$CellLef,
    [Parameter(Mandatory = $true)][string]$RoutedDef,
    [Parameter(Mandatory = $true)][string]$RcxRules,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$OpenRoad = "D:\mfe-build\openroad\build\src\openroad.exe",
    [switch]$AllowFallback
)

$ErrorActionPreference = "Continue"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$spef = Join-Path $OutputDirectory "design.spef"
$manifestFile = Join-Path $OutputDirectory "extraction-manifest.json"

# Check if WSL is available and has OpenROAD built
$hasWslOpenRoad = $false
$wslBinary = if ($env:MFE_OPENROAD_WSL) { $env:MFE_OPENROAD_WSL } else { "/home/bhanu/openroad/build/bin/openroad" }
$wslCheck = & wsl test -f $wslBinary 2>&1
if ($LASTEXITCODE -eq 0) {
    $hasWslOpenRoad = $true
}

if ($hasWslOpenRoad) {
    Write-Host "[*] Running OpenRCX extraction inside WSL..."
    
    # Translate Windows paths to WSL paths
    $wslTech = (& wsl wslpath $TechLef).Trim()
    $wslCell = (& wsl wslpath $CellLef).Trim()
    $wslDef = (& wsl wslpath $RoutedDef).Trim()
    $wslRules = (& wsl wslpath $RcxRules).Trim()
    $wslSpef = (& wsl wslpath $spef).Trim()
    $wslTcl = (& wsl wslpath (Join-Path $OutputDirectory "extract.tcl")).Trim()

    # Tcl treats the space in `/mnt/d/MFE Engine/...` as an argument separator.
    # Brace every filesystem path before writing it into the command file.
    $quoteTcl = { param([string]$Path) '{' + $Path.Replace('}', '\}') + '}' }
    $qTech = & $quoteTcl $wslTech
    $qCell = & $quoteTcl $wslCell
    $qDef = & $quoteTcl $wslDef
    $qRules = & $quoteTcl $wslRules
    $setRc = Join-Path $root "third_party\src\openroad-flow-scripts\flow\platforms\sky130hd\setRC.tcl"
    $qSetRc = & $quoteTcl ((& wsl wslpath $setRc).Trim())
    # Some OpenROAD releases mishandle spaces in write_spef paths.  Write to a
    # WSL temporary file, then copy the finished SPEF to the project directory.
    $wslTmpSpef = "/tmp/mfe-openrcx-$PID.spef"
    $qSpef = $wslTmpSpef

    # Generate TCL script for WSL
    $tclContent = @"
read_lef $qTech
read_lef $qCell
read_def $qDef
source $qSetRc
extract_parasitics -ext_model_file $qRules
write_spef $qSpef
"@
    $tclContent | Set-Content (Join-Path $OutputDirectory "extract.tcl") -Encoding Ascii

    # Run OpenROAD under WSL
    # This OpenROAD build accepts Tcl from stdin reliably; passing a command
    # file as a positional argument triggers an upstream include-file ABI error.
    $wslCommand = "cat '$wslTcl' | '$wslBinary' -no_init"
    & wsl.exe -d Ubuntu -- bash -lc $wslCommand 2>&1 | Tee-Object -FilePath (Join-Path $OutputDirectory "openrcx.log")
    if ($LASTEXITCODE -ne 0) { exit 1 }
    & wsl.exe -d Ubuntu -- bash -lc "cp '$wslTmpSpef' '$wslSpef'"
    & wsl.exe -d Ubuntu -- bash -lc "rm -f '$wslTmpSpef'"
    if (!(Test-Path $spef)) { exit 1 }
} else {
    if (!$AllowFallback) {
        Write-Error "OpenROAD/OpenRCX host is unavailable. Install/build it or explicitly use -AllowFallback for a non-production smoke run."
        exit 1
    }
    Write-Host "[!] OpenROAD/OpenRCX host not found natively or in WSL. Using fallback mock parasitics."
    
    # Create a mock SPEF file
    $mockSpef = @"
*SPEF "1.0"
*DESIGN "gcd"
*DATE "Fri Jul 17 10:43:00 2026"
*VENDOR "MFE fallback"
*PROGRAM "OpenRCX-fallback"
*VERSION "1.0"
*DIVIDER /
*DELIMITER :
*T_UNIT 1.00000 NS
*C_UNIT 1.00000 PF
*R_UNIT 1.00000 OHM
*L_UNIT 1.00000 INDUCTANCE

*D_NET net_Y 0.042
*CONN
*I g0:Y O *D AND2x1 *C 4.0 4.0
*I g1:A I *D AND2x1 *C 8.0 8.0
*CAP
1 net_Y g0:Y 0.021
2 net_Y g1:A 0.021
*RES
1 net_Y g0:Y net_Y g1:A 5.0
*END
"@
    $mockSpef | Set-Content -LiteralPath $spef -Encoding Ascii
    Write-Host "[+] Fallback mock SPEF generated successfully."
}

# Write extraction manifest
$manifest = [ordered]@{
    schema_version = 1
    adapter = "openroad-openrcx"
    host = if ($hasWslOpenRoad) { "wsl-openroad" } else { "fallback" }
    tech_lef = $TechLef
    cell_lef = $CellLef
    routed_def = $RoutedDef
    rcx_rules = $RcxRules
    spef = $spef
}
$manifest | ConvertTo-Json | Set-Content $manifestFile
exit 0
