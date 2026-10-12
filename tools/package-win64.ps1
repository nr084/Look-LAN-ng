param([string]$BuildDirectory = 'build')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
if (-not $buildRoot.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Build path escapes project' }
$exe = Join-Path $buildRoot 'Look@LAN-ng.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Build Release first' }
$version = (Get-Item -LiteralPath $exe).VersionInfo.FileVersion
$productVersion = (Get-Item -LiteralPath $exe).VersionInfo.ProductVersion
if ($productVersion) { $version = $productVersion }
if ($version -notmatch '^\d+\.\d+\.\d+(-rc\.\d+)?$') { throw 'Invalid EXE version' }
$distRoot = Join-Path $projectRoot 'dist'
$unpacked = Join-Path $buildRoot 'unpacked'
New-Item -ItemType Directory -Force $distRoot,$unpacked | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $unpacked 'Look@LAN-ng.exe') -Force
$stage = Join-Path $buildRoot ('pack-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    $release = Invoke-RestMethod -Uri 'https://api.github.com/repos/upx/upx/releases/latest' -Headers @{ 'User-Agent'='LookAtLANng-release-builder' }
    if ($release.draft -or $release.prerelease -or $release.tag_name -notmatch '^v\d+\.\d+\.\d+$') { throw 'UPX release is not stable' }
    $upxVersion = $release.tag_name.Substring(1)
    $asset = @($release.assets | Where-Object { $_.name -eq "upx-$upxVersion-win64.zip" })
    if ($asset.Count -ne 1 -or -not $asset[0].browser_download_url.StartsWith('https://github.com/upx/upx/releases/download/')) { throw 'UPX asset not found in official repository' }
    $archive = Join-Path $stage 'upx.zip'
    Invoke-WebRequest -Uri $asset[0].browser_download_url -OutFile $archive -UseBasicParsing
    $upxSha = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($asset[0].digest -and $asset[0].digest -ne "sha256:$upxSha") { throw 'UPX archive digest mismatch' }
    Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $stage 'upx')
    $upx = Join-Path $stage "upx\upx-$upxVersion-win64\upx.exe"
    if (-not (Test-Path -LiteralPath $upx)) { throw 'UPX executable missing' }
    $packed = Join-Path $stage 'Look@LAN-ng.exe'
    $packingOutput = & $upx --ultra-brute --lzma -o $packed $exe 2>&1
    $packingCode = $LASTEXITCODE
    $packingOutput | ForEach-Object { Write-Host $_ }
    $compressionApplied = $packingCode -eq 0
    $compressionNote = 'passed'
    if (-not $compressionApplied) { throw 'UPX compression failed; no packed release was published' }
    else {
        & $upx -t $packed
        if ($LASTEXITCODE -ne 0) { throw 'UPX integrity test failed' }
    }
    $smokeResults = @{}
    foreach ($argument in @('--smoke-test','--smoke-dpi','--smoke-close-scan','--smoke-close-tools')) {
        $smoke = Start-Process -FilePath $packed -ArgumentList $argument -PassThru -WindowStyle Hidden
        if (-not $smoke.WaitForExit(30000)) { $smoke.Kill(); $smoke.WaitForExit(); throw "Packed EXE $argument timed out" }
        $smoke.Refresh()
        if ($smoke.ExitCode -eq 77 -and $argument -in @('--smoke-close-scan','--smoke-close-tools')) { $smokeResults[$argument] = 'skipped_no_ipv4' }
        elseif ($smoke.ExitCode -ne 0) { throw "Packed EXE $argument failed: $($smoke.ExitCode)" }
        else { $smokeResults[$argument] = 'passed' }
    }
    $versionRoot = Join-Path $distRoot $version
    New-Item -ItemType Directory -Force -Path $versionRoot | Out-Null
    $destination = Join-Path $versionRoot 'Look@LAN-ng.exe'
    Copy-Item -LiteralPath $packed -Destination $destination -Force
    $latestExeUpdated = $true
    try { Copy-Item -LiteralPath $packed -Destination (Join-Path $distRoot 'Look@LAN-ng.exe') -Force -ErrorAction Stop }
    catch { $latestExeUpdated = $false; Write-Warning "Flat EXE could not be updated; use $destination or the ZIP. $($_.Exception.Message)" }
    $zip = Join-Path $distRoot "Look@LAN-ng-$version-win64.zip"
    Compress-Archive -LiteralPath $destination -DestinationPath $zip -CompressionLevel Optimal -Force
    $report = [ordered]@{
        version=$version; architecture='win64'; copyright=(Get-Item -LiteralPath $exe).VersionInfo.LegalCopyright
        exe_path="$version/Look@LAN-ng.exe"; latest_exe_updated=$latestExeUpdated
        upx_version=$upxVersion; upx_release=$release.html_url; upx_archive_sha256=$upxSha
        options='--ultra-brute --lzma'; compression_applied=$compressionApplied; compression_note=$compressionNote
        integrity_test=$(if ($compressionApplied) {'passed'} else {'not_applicable'}); ui_smoke='passed'
        ui_dpi=$smokeResults['--smoke-dpi']; ui_close_scan=$smokeResults['--smoke-close-scan']
        ui_close_tools=$smokeResults['--smoke-close-tools']
        unpacked_bytes=(Get-Item -LiteralPath $exe).Length; packed_bytes=(Get-Item -LiteralPath $destination).Length
        exe_sha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        zip_sha256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
    }
    $report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $distRoot 'release-info.json') -Encoding utf8
    $report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $distRoot "Look@LAN-ng-$version-win64.json") -Encoding utf8
    $report | ConvertTo-Json
} finally {
    $resolvedStage = [IO.Path]::GetFullPath($stage)
    if (-not $resolvedStage.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Cleanup path escapes build directory' }
    if (Test-Path -LiteralPath $resolvedStage) { Remove-Item -LiteralPath $resolvedStage -Recurse -Force }
}
