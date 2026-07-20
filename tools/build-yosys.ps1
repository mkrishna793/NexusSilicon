param(
    [string]$Source = "D:\MFE Engine\third_party\src\yosys",
    [string]$Build = "D:\MFE Engine\third_party\build\yosys"
)

$ErrorActionPreference = "Stop"
$env:PATH = "D:\msys64\mingw64\bin;D:\msys64\usr\bin;" + $env:PATH
$cmake = "D:\MFE Engine\cmake\bin\cmake.exe"

& $cmake -S $Source -B $Build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $Build --parallel 4
exit $LASTEXITCODE
