param(
    [Parameter(Mandatory = $true)][string]$TechLef,
    [Parameter(Mandatory = $true)][string]$CellLef,
    [Parameter(Mandatory = $true)][string]$RoutedDef,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [switch]$AllowFallback
)

$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$reportFile = Join-Path $OutputDirectory "klayout_drc.rpt"
$manifestFile = Join-Path $OutputDirectory "klayout-manifest.json"
$gdsFile = Join-Path (Split-Path -Parent $RoutedDef) "design.gds"
$deck = "D:\MFE Engine\third_party\src\openroad-flow-scripts\flow\platforms\sky130hd\drc\sky130hd.lydrc"

$hasWslKlayout = (& wsl.exe -d Ubuntu -- bash -lc "command -v klayout" 2>$null)
if ($hasWslKlayout -and (Test-Path $gdsFile) -and (Test-Path $deck)) {
    $wslGds = (& wsl.exe -d Ubuntu -- wslpath -a $gdsFile).Trim()
    $wslDeck = (& wsl.exe -d Ubuntu -- wslpath -a $deck).Trim()
    $wslReport = (& wsl.exe -d Ubuntu -- wslpath -a $reportFile).Trim()
    Write-Host "[*] Running Sky130 KLayout DRC in WSL..."
    $command = "klayout -b -r '$wslDeck' -rd in_gds='$wslGds' -rd report_file='$wslReport'"
    & wsl.exe -d Ubuntu -- bash -lc $command 2>&1 | Tee-Object -FilePath (Join-Path $OutputDirectory "klayout.log")
    $real = Test-Path $reportFile
    if (!$real -and !$AllowFallback) { exit 1 }
} else {
    if (!$AllowFallback) { Write-Error "KLayout, the Sky130 deck, or the GDS output is unavailable."; exit 1 }
    "KLayout fallback: no physical verification executed." | Set-Content -LiteralPath $reportFile
    $real = $false
}

[ordered]@{
    schema_version = 1
    adapter = "klayout-drc"
    mode = if ($real) { "real" } else { "fallback" }
    gds = $gdsFile
    deck = $deck
    report = $reportFile
} | ConvertTo-Json | Set-Content $manifestFile
if (!$real) { exit 1 }
exit 0
