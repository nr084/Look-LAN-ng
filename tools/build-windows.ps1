param([ValidateSet('x86','x64','arm64')][string]$Architecture='x64', [switch]$BuildOnly)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot=Join-Path $projectRoot "build/windows-$Architecture"
$platform=@{x86='Win32';x64='x64';arm64='ARM64'}[$Architecture]
cmake -S $projectRoot -B $buildRoot -G 'Visual Studio 17 2022' -A $platform
if ($LASTEXITCODE) { throw 'CMake configure failed' }
cmake --build $buildRoot --config Release --parallel
if ($LASTEXITCODE) { throw 'Release build failed' }
if (!$BuildOnly) {
    ctest --test-dir $buildRoot -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'CTest failed' }
}
