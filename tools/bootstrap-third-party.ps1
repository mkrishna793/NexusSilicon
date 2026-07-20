param(
    [string]$Destination = (Join-Path $PSScriptRoot "..\third_party\src")
)

$ErrorActionPreference = "Stop"
$manifestPath = Join-Path $PSScriptRoot "..\third_party\toolchain.json"
$manifest = Get-Content -Raw $manifestPath | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $Destination | Out-Null

foreach ($tool in $manifest.tools) {
    $path = Join-Path $Destination $tool.name
    if (Test-Path $path) {
        $revisionFile = Join-Path $path ".mfe-source-revision"
        if (Test-Path $revisionFile) {
            Write-Host "[skip] $($tool.name) already exists at $path"
            continue
        }
        Write-Host "[retry] removing incomplete clone for $($tool.name)"
        Remove-Item -LiteralPath $path -Recurse -Force
    }

    $arguments = @("clone", "--depth", "1", "--filter=blob:none", "--branch", $tool.ref)
    if ($tool.submodules) { $arguments += @("--recurse-submodules", "--shallow-submodules") }
    $arguments += @($tool.repository, $path)
    Write-Host "[clone] $($tool.name)"
    & git @arguments
    if ($LASTEXITCODE -ne 0) { throw "Unable to clone $($tool.name)" }

    $revision = (& git -C $path rev-parse HEAD).Trim()
    Set-Content -NoNewline -Path (Join-Path $path ".mfe-source-revision") -Value $revision
}
