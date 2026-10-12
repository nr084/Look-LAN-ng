param([string]$InputDirectory='dist', [string]$OutputDirectory='dist')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$inputRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot $InputDirectory))
$outputRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
New-Item -ItemType Directory -Force $outputRoot | Out-Null
$version=$null; $checksums=@()
foreach($architecture in @('x86','x64','arm64')) {
    $source=Join-Path $inputRoot "windows-$architecture"
    $info=Get-Content (Join-Path $source 'build-info.json') -Raw | ConvertFrom-Json
    if (!$version) { $version=$info.version }
    if ($version -notmatch '^\d+\.\d+\.\d+(-rc\.\d+)?$' -or $info.version -ne $version -or $info.architecture -ne $architecture -or $info.integrity -ne 'passed') { throw 'Architecture/version/integrity mismatch' }
    $exe=Join-Path $source 'Look@LAN-ng.exe'
    if ((Get-FileHash $exe).Hash -ne $info.exe_sha256) { throw 'Executable checksum mismatch' }
    $zip=Join-Path $outputRoot "look@lan-ng-$architecture-$version.zip"
    Compress-Archive -LiteralPath $exe -DestinationPath $zip -CompressionLevel Optimal -Force
    $checksums+=('- `'+[IO.Path]::GetFileName($zip)+'`: `'+(Get-FileHash $zip).Hash+'`')
    Write-Output $zip
}
$notes=Get-Content (Join-Path $projectRoot 'docs/RELEASE_NOTES.md') -Raw
$notes+="`n`n## SHA-256`n`n"+($checksums -join "`n")+"`n"
Set-Content (Join-Path $outputRoot 'release-notes.md') $notes -Encoding utf8
