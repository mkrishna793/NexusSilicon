param(
    [string]$Source = "D:\MFE Engine\third_party\src\opensta",
    [string]$Build = "D:\MFE Engine\third_party\build\opensta",
    [string]$Cudd = "D:\MFE Engine\third_party\install\cudd"
)

$ErrorActionPreference = "Stop"
$env:PATH = "D:\msys64\mingw64\bin;D:\msys64\usr\bin;" + $env:PATH
$cmake = "D:\MFE Engine\cmake\bin\cmake.exe"
$tclLibrary = "D:/msys64/mingw64/lib/libtcl86.dll.a"
$tclHeader = "D:/msys64/mingw64/include/tcl.h"
$swig = "D:/msys64/mingw64/bin/swig.exe"
$flexInclude = "D:/msys64/usr/include"
$zlibLibrary = "D:/msys64/mingw64/lib/libz.dll.a"
$zlibInclude = "D:/msys64/mingw64/include"

if (!(Test-Path $Cudd)) { throw "CUDD is not installed at $Cudd. Run tools\\build-cudd.ps1 first." }
if (!(Test-Path $tclLibrary) -or !(Test-Path $tclHeader) -or !(Test-Path $swig) -or !(Test-Path (Join-Path $flexInclude "FlexLexer.h")) -or !(Test-Path $zlibLibrary)) {
    throw "OpenSTA build dependencies are missing: Tcl or SWIG."
}

& $cmake -S $Source -B $Build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
    "-DCUDD_DIR=$Cudd" "-DTCL_LIBRARY=$tclLibrary" "-DTCL_HEADER=$tclHeader" `
    "-DSWIG_EXECUTABLE=$swig" "-DFLEX_INCLUDE_DIR=$flexInclude" "-DFLEX_INCLUDE_DIRS=$flexInclude" `
    "-DZLIB_LIBRARY=$zlibLibrary" "-DZLIB_INCLUDE_DIR=$zlibInclude" `
    -DUSE_TCL_READLINE=OFF -DBUILD_TESTS=OFF
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $Build --parallel 4
exit $LASTEXITCODE
