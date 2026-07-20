param(
    [Parameter(Mandatory = $true)][string[]]$Rtl,
    [Parameter(Mandatory = $true)][string]$Top,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$Liberty,
    [switch]$AllowFallback
)

$ErrorActionPreference = "Stop"
$env:PATH = "D:\msys64\mingw64\bin;D:\msys64\usr\bin;" + $env:PATH
$yosysBuild = "D:\MFE Engine\third_party\build\yosys"
if (!(Test-Path (Join-Path $yosysBuild "yosys.exe"))) { throw "Yosys is not built. Run tools\\build-yosys.ps1 first." }

# ABC is launched by Yosys as a child process.  Keep those two executables in
# a path without spaces so Windows command parsing cannot split `MFE Engine`.
$yosysRuntime = "D:\mfe-runtime\yosys"
New-Item -ItemType Directory -Force -Path $yosysRuntime | Out-Null
foreach ($binary in @("yosys.exe", "yosys-abc.exe")) {
    $source = Join-Path $yosysBuild $binary
    $destination = Join-Path $yosysRuntime $binary
    if (!(Test-Path $destination) -or (Get-Item $source).Length -ne (Get-Item $destination).Length) {
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }
}
$yosys = Join-Path $yosysRuntime "yosys.exe"
foreach ($file in $Rtl) { if (!(Test-Path $file)) { throw "RTL file not found: $file" } }
if ($Liberty -and !(Test-Path $Liberty)) { throw "Liberty file not found: $Liberty" }

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$netlist = Join-Path $OutputDirectory "netlist.v"
$script = Join-Path $OutputDirectory "synthesis.ys"
$useWslYosys = $false
$wslProbe = & wsl.exe -d Ubuntu -- bash -lc "command -v yosys" 2>$null
if ($LASTEXITCODE -eq 0 -and $wslProbe) { $useWslYosys = $true }
$quoteYosysPath = {
    param([string]$Path)
    if ($useWslYosys) {
        $wslPath = (& wsl.exe -d Ubuntu -- wslpath -a $Path).Trim()
        '"' + $wslPath.Replace('"', '\"') + '"'
    } else {
        '"' + $Path.Replace('\', '/').Replace('"', '\"') + '"'
    }
}
$rtlCommands = $Rtl | ForEach-Object { "read_verilog -sv $(& $quoteYosysPath $_)" }
$mapping = if ($Liberty) {
    $library = & $quoteYosysPath $Liberty
    if ($useWslYosys) {
        "dfflibmap -liberty $library`nabc -liberty $library"
    } else {
        "dfflibmap -liberty $library`nabc -exe D:/mfe-runtime/yosys/yosys-abc.exe -liberty $library"
    }
} else { "techmap" }
@(
    $rtlCommands
    "hierarchy -check -top $Top"
    "proc; opt; memory; opt; flatten; techmap; opt"
    $mapping
    "clean"
    "write_verilog -noattr $(& $quoteYosysPath $netlist)"
) | Set-Content -Path $script

if ($useWslYosys) {
    $wslScript = (& wsl.exe -d Ubuntu -- wslpath -a $script).Trim()
    & wsl.exe -d Ubuntu -- yosys -s $wslScript
} else {
    & $yosys -s $script
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$manifest = [ordered]@{
    schema_version = 1
    adapter = "yosys"
    top = $Top
    rtl = $Rtl
    liberty = $Liberty
    netlist = $netlist
    netlist_sha256 = (Get-FileHash -Algorithm SHA256 $netlist).Hash
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $OutputDirectory "synthesis-manifest.json")
