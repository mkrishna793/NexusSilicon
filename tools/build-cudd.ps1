param(
    [string]$Source = "D:\MFE Engine\third_party\src\cudd",
    [string]$Install = "D:\MFE Engine\third_party\install\cudd"
)

$ErrorActionPreference = "Stop"
$bash = "D:\msys64\usr\bin\bash.exe"
$sourcePath = ($Source -replace '^D:', '/d').Replace('\', '/').Replace(' ', '\ ')
$installPath = ($Install -replace '^D:', '/d').Replace('\', '/').Replace(' ', '\ ')
$command = "export PATH=/mingw64/bin:/usr/bin:`$PATH; export CC=gcc CXX=g++ MAKE=mingw32-make; cd $sourcePath && ./configure --prefix=$installPath && mingw32-make -j4 && mingw32-make install"
& $bash -lc $command
exit $LASTEXITCODE
