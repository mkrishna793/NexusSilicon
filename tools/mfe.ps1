param(
    [Parameter(Mandatory = $true)][ValidateSet("flow", "ask")][string]$Command,
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$Question,
    [switch]$Offline
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$projectPath = (Resolve-Path $Project).Path

if ($Command -eq "flow") {
    & (Join-Path $root "tools\flow\run-flow.ps1") $projectPath
    exit $LASTEXITCODE
}

if ([string]::IsNullOrWhiteSpace($Question)) { throw "-Question is required for ask." }
$agent = Join-Path $root "tools\agent\mfe_agent.py"
$arguments = @($agent, $Question, "--project", $projectPath)
if ($Offline) { $arguments += "--offline" }
& python @arguments
exit $LASTEXITCODE
