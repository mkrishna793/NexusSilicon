param(
    [string]$Source = "D:\mfe-build\openroad",
    [string]$Build = "D:\mfe-build\openroad\build",
    [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"
$env:PATH = "D:\msys64\mingw64\bin;D:\msys64\usr\bin;" + $env:PATH
$cmake = "D:\MFE Engine\cmake\bin\cmake.exe"

if (!(Test-Path (Join-Path $Source "src\rcx\CMakeLists.txt"))) {
    throw "OpenROAD source with the OpenRCX module is required at $Source."
}

$configureArgs = @(
    "-S", $Source,
    "-B", $Build,
    "-G", "MinGW Makefiles",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBUILD_GUI=OFF",
    "-DBUILD_PYTHON=OFF",
    "-DBUILD_TESTS=OFF",
    "-DENABLE_TESTS=OFF",
    "-DALLOW_WARNINGS=ON",
    "-DCMAKE_PREFIX_PATH=D:/msys64/mingw64;D:/mfe-build/install/or-tools",
    "-DTCL_LIBRARY=D:/msys64/mingw64/lib/libtcl86.dll.a",
    "-DTCL_HEADER=D:/msys64/mingw64/include/tcl.h",
    "-DTCL_INCLUDE_PATH=D:/msys64/mingw64/include",
    "-DFLEX_INCLUDE_DIR=D:/msys64/usr/include",
    "-DZLIB_LIBRARY=D:/msys64/mingw64/lib/libz.dll.a",
    "-DZLIB_INCLUDE_DIR=D:/msys64/mingw64/include",
    "-DPython3_ROOT_DIR=D:/msys64/mingw64",
    "-DPython3_EXECUTABLE=D:/msys64/mingw64/bin/python.exe",
    "-DPython3_LIBRARY=D:/msys64/mingw64/lib/libpython3.14.dll.a",
    "-DPython3_INCLUDE_DIR=D:/msys64/mingw64/include/python3.14",
    "-DCUDD_DIR=D:/mfe-build/install/cudd",
    "-DLEMON_DIR=D:/mfe-build/install/lemon/cmake",
    "-Dortools_DIR=D:/mfe-build/install/or-tools/lib/cmake/ortools",
    "-DProtobuf_DIR=D:/mfe-build/install/or-tools/lib/cmake/protobuf",
    "-Dutf8_range_DIR=D:/mfe-build/install/or-tools/lib/cmake/utf8_range",
    "-Dre2_DIR=D:/mfe-build/install/or-tools/lib/cmake/re2"
)

& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (!$ConfigureOnly) {
    & $cmake --build $Build --parallel 4
    exit $LASTEXITCODE
}
