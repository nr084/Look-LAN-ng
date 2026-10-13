param([ValidateSet('x86','x64','arm64')][string]$Architecture='x64', [switch]$BuildOnly)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot=Join-Path $projectRoot "build/windows-$Architecture"
$platform=@{x86='Win32';x64='x64';arm64='ARM64'}[$Architecture]
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$instances=@(& $vswhere -latest -products '*' -format json | ConvertFrom-Json)
if (!$instances.Count) { throw 'Visual Studio C++ build tools are not installed' }
$major=([version]$instances[0].installationVersion).Major
$generator=switch($major) { 17 {'Visual Studio 17 2022'} 18 {'Visual Studio 18 2026'} default { throw "Unsupported Visual Studio major $major" } }
Write-Host "Using $generator at $($instances[0].installationPath) for $Architecture"
cmake -S $projectRoot -B $buildRoot -G $generator -A $platform "-DCMAKE_GENERATOR_INSTANCE=$($instances[0].installationPath)"
if ($LASTEXITCODE) { throw 'CMake configure failed' }
if ($BuildOnly) {
    cmake --build $buildRoot --config Release --target LookAtLANng --parallel 8
} else {
    cmake --build $buildRoot --config Release --parallel
}
if ($LASTEXITCODE) { throw 'Release build failed' }
if (!$BuildOnly) {
    ctest --test-dir $buildRoot -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'CTest failed' }
}
