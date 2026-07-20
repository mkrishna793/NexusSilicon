param(
    [Parameter(Mandatory = $true)][string]$Netlist,
    [Parameter(Mandatory = $true)][string]$Top,
    [Parameter(Mandatory = $true)][string]$Liberty,
    [Parameter(Mandatory = $true)][string]$Sdc,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$Spef,
    [string]$TechLef,
    [string]$CellLef,
    [switch]$AllowFallback
)

$ErrorActionPreference = "Continue"
$env:PATH = "D:\msys64\mingw64\bin;D:\msys64\usr\bin;" + $env:PATH
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$opensta = Join-Path $root "third_party\build\opensta\sta.exe"
$wslOpenRoad = if ($env:MFE_OPENROAD_WSL) { $env:MFE_OPENROAD_WSL } else { "/home/bhanu/openroad/build/bin/openroad" }

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$script = Join-Path $OutputDirectory "timing.tcl"
$report = Join-Path $OutputDirectory "timing.rpt"
$manifestFile = Join-Path $OutputDirectory "timing-manifest.json"

$quoteTclPath = {
    param([string]$Path)
    '{' + $Path.Replace('\', '/').Replace('}', '\}') + '}'
}
$runSuccess = $false

# OpenROAD embeds the compatible OpenSTA version used by the same host that
# produced the SPEF.  Prefer it over a separately-built Windows OpenSTA so
# that SPEF syntax and database assumptions remain consistent.
$wslCheck = & wsl test -f $wslOpenRoad 2>$null
if ($LASTEXITCODE -eq 0) {
    $wslNetlist = (& wsl wslpath $Netlist).Trim()
    $wslLiberty = (& wsl wslpath $Liberty).Trim()
    $wslSdc = (& wsl wslpath $Sdc).Trim()
    $wslScript = (& wsl wslpath $script).Trim()
    $wslSpef = if ($Spef) { (& wsl wslpath $Spef).Trim() } else { "" }
    $wslTechLef = if ($TechLef) { (& wsl wslpath $TechLef).Trim() } else { "" }
    $wslCellLef = if ($CellLef) { (& wsl wslpath $CellLef).Trim() } else { "" }
    $quoteWslTcl = { param([string]$Path) '{' + $Path.Replace('}', '\}') + '}' }
    $wslSpefCommand = if ($wslSpef) { "read_spef $(& $quoteWslTcl $wslSpef)" } else { "" }
    $wslTechLefCommand = if ($wslTechLef) { "read_lef $(& $quoteWslTcl $wslTechLef)" } else { "" }
    $wslCellLefCommand = if ($wslCellLef) { "read_lef $(& $quoteWslTcl $wslCellLef)" } else { "" }

    @(
        $wslTechLefCommand
        $wslCellLefCommand
        "read_liberty $(& $quoteWslTcl $wslLiberty)"
        "read_verilog $(& $quoteWslTcl $wslNetlist)"
        "link_design $Top"
        "read_sdc $(& $quoteWslTcl $wslSdc)"
        $wslSpefCommand
        "report_checks -path_delay max -fields {slew cap input_pins} -digits 4"
        "report_checks -path_delay min -fields {slew cap input_pins} -digits 4"
        "exit"
    ) | Set-Content -LiteralPath $script -Encoding Ascii

    Write-Host "[*] Executing OpenSTA timing checks inside the OpenROAD host..."
    $wslCommand = "cat '$wslScript' | '$wslOpenRoad' -no_init"
    # Windows PowerShell's Tee-Object writes UTF-16 by default.  STA report
    # readers expect an ASCII/UTF-8 text report, so preserve a portable
    # encoding explicitly after echoing the host output to the console.
    $hostOutput = & wsl.exe -d Ubuntu -- bash -lc $wslCommand 2>&1
    $hostExitCode = $LASTEXITCODE
    $hostOutput | ForEach-Object { Write-Host $_ }
    $hostOutput | Set-Content -LiteralPath $report -Encoding Ascii
    if ($hostExitCode -eq 0 -and (Test-Path $report)) {
        $runSuccess = $true
    }
} elseif (Test-Path $opensta) {
    $spefCommand = if ($Spef) { "read_spef $(& $quoteTclPath $Spef)" } else { "" }
    @(
        "read_liberty $(& $quoteTclPath $Liberty)"
        "read_verilog $(& $quoteTclPath $Netlist)"
        "link_design $Top"
        "read_sdc $(& $quoteTclPath $Sdc)"
        $spefCommand
        "report_checks -path_delay max -fields {slew cap input_pins} -digits 4"
        "report_checks -path_delay min -fields {slew cap input_pins} -digits 4"
        "exit"
    ) | Set-Content -LiteralPath $script -Encoding Ascii
    Write-Host "[*] Executing standalone OpenSTA timing checks..."
    try {
        # Feed Tcl through stdin: this avoids command-file argument parsing
        # issues when the workspace path contains spaces.
        $hostOutput = Get-Content -LiteralPath $script | & $opensta 2>&1
        $hostExitCode = $LASTEXITCODE
        $hostOutput | ForEach-Object { Write-Host $_ }
        $hostOutput | Set-Content -LiteralPath $report -Encoding Ascii
        if ($hostExitCode -eq 0 -and (Test-Path $report)) {
            $runSuccess = $true
        }
    } catch {
        Write-Host "[!] OpenSTA runtime failed."
    }
}

if (-not $runSuccess) {
    if (!$AllowFallback) {
        Write-Error "OpenSTA did not produce a timing report. Use -AllowFallback only for a non-production smoke run."
        exit 1
    }
    Write-Host "[!] OpenSTA execution failed or timing report not generated. Using fallback mock report with setup violation."
    
    # Generate mock timing report showing a setup violation on g0 -> g1
    $mockReport = @"
Startpoint: g0
Endpoint: g1
slack -0.1500 (VIOLATED)
"@
    $mockReport | Set-Content -LiteralPath $report -Encoding Ascii
    Write-Host "[+] Fallback mock timing report generated."
}

# Write timing manifest
[ordered]@{ 
    schema_version = 1
    adapter = if ($wslCheck -and $LASTEXITCODE -eq 0) { "openroad-opensta" } else { "opensta" }
    netlist = $Netlist
    liberty = $Liberty
    tech_lef = $TechLef
    cell_lef = $CellLef
    sdc = $Sdc
    spef = $Spef
    report = $report 
} | ConvertTo-Json | Set-Content $manifestFile
exit 0
