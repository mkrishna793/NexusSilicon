param(
    [string]$Source = "D:\MFE Engine\third_party\src\klayout"
)

$ErrorActionPreference = "Stop"
$bash = "D:\msys64\usr\bin\bash.exe"
$qmake = "D:\msys64\mingw64\bin\qmake6.exe"
if (!(Test-Path $qmake)) { throw "Qt 6 qmake is not installed. Install mingw-w64-x86_64-qt6-base first." }

$sourcePath = ($Source -replace '^D:', '/d').Replace('\', '/').Replace(' ', '\ ')
$qmakePath = ($qmake -replace '^D:', '/d').Replace('\', '/').Replace(' ', '\ ')
& $bash -lc "export PATH=/mingw64/bin:/usr/bin:`$PATH; cd $sourcePath && ./build.sh -j4 -qmake $qmakePath"
exit $LASTEXITCODE
