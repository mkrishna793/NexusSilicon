param(
    [Parameter(Mandatory = $true)][string[]]$Rtl,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [switch]$AllowFallback
)

$ErrorActionPreference = "Continue"

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$lintFile = Join-Path $OutputDirectory "lint.json"
$astFile = Join-Path $OutputDirectory "ast.json"

$veribleLint = Get-Command "verible-verilog-lint" -ErrorAction SilentlyContinue
$veribleSyntax = Get-Command "verible-verilog-syntax" -ErrorAction SilentlyContinue

if ($veribleLint -and $veribleSyntax) {
    Write-Host "[*] Running Verible lint and syntax analysis..."
    
    # Run Verible lint
    $lintOutputs = @()
    foreach ($file in $Rtl) {
        $rawLint = & "verible-verilog-lint" $file --ruleset all 2>&1
        $lintOutputs += [ordered]@{
            file = $file
            issues = $rawLint
        }
    }
    $lintOutputs | ConvertTo-Json -Depth 4 | Set-Content $lintFile

    # Run Verible syntax (AST)
    $astOutputs = @()
    foreach ($file in $Rtl) {
        $rawAst = & "verible-verilog-syntax" --export_json $file 2>&1
        $astOutputs += [ordered]@{
            file = $file
            ast = $rawAst
        }
    }
    $astOutputs | ConvertTo-Json -Depth 10 | Set-Content $astFile
    Write-Host "[+] Verible analysis completed."
} else {
    if (!$AllowFallback) {
        Write-Error "Verible is unavailable. Install it or explicitly use -AllowFallback for a non-production smoke run."
        exit 1
    }
    Write-Host "[!] Verible tools not found in PATH. Using fallback mock diagnostics."
    
    # Generate mock clean lint
    $mockLint = @(
        [ordered]@{
            status = "passed"
            tool = "verible-fallback"
            findings = @()
        }
    )
    $mockLint | ConvertTo-Json -Depth 4 | Set-Content $lintFile

    # Generate mock AST
    $mockAst = [ordered]@{
        status = "parsed"
        tool = "verible-fallback"
        modules = @(
            [ordered]@{
                name = "gcd"
                ports = @("clk", "rst_n", "start", "a_in", "b_in", "val_out", "done")
            }
        )
    }
    $mockAst | ConvertTo-Json -Depth 4 | Set-Content $astFile
    Write-Host "[+] Fallback mock lint and AST generated."
}
