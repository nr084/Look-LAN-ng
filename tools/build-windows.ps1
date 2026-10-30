param([ValidateSet('x86','x64','arm64')][string]$Architecture='x64', [switch]$BuildOnly)
$ErrorActionPreference='Stop'
$looklanPython=$env:LOOKLAN_PYTHON
if(!$looklanPython){$looklanPython=(Get-Command python -ErrorAction Stop).Source}
# All entry points run the stable-toolchain/IEEE preflight. BuildOnly is retained
# as a compatible argument; release validation is deliberately still performed.
& $looklanPython (Join-Path $PSScriptRoot 'build-current.py') --arch $Architecture
if($LASTEXITCODE){throw 'Local Windows build/validation failed'}
