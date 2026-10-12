param([Parameter(Mandatory)][ValidateSet('x86','x64','arm64')][string]$Architecture)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$exe=Join-Path $projectRoot "build/windows-$Architecture/Release/Look@LAN-ng.exe"
$version=(Get-Item -LiteralPath $exe).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+(-rc\.\d+)?$') { throw 'Invalid executable version' }
if ($env:GITHUB_REF_TYPE -eq 'tag' -and $env:GITHUB_REF_NAME -ne "v$version") { throw 'Tag and executable version differ' }
$output=Join-Path $projectRoot "dist/windows-$Architecture"
New-Item -ItemType Directory -Force $output | Out-Null
$unpacked=Join-Path $output 'unpacked'
New-Item -ItemType Directory -Force $unpacked | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $unpacked 'Look@LAN-ng.exe') -Force
$stage=Join-Path $projectRoot ('build/upx-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $stage | Out-Null
try {
    $headers=@{'User-Agent'='LookAtLANng-release'}
    if ($env:GH_TOKEN) { $headers.Authorization='Bearer '+$env:GH_TOKEN }
    $release=Invoke-RestMethod 'https://api.github.com/repos/upx/upx/releases/latest' -Headers $headers
    if ($release.draft -or $release.prerelease -or $release.tag_name -notmatch '^v\d+\.\d+\.\d+$') { throw 'UPX must be stable' }
    $upxVersion=$release.tag_name.Substring(1)
    $asset=@($release.assets | Where-Object name -eq "upx-$upxVersion-win64.zip")
    if ($asset.Count -ne 1 -or !$asset[0].browser_download_url.StartsWith('https://github.com/upx/upx/releases/download/')) { throw 'Official UPX asset missing' }
    $archive=Join-Path $stage 'upx.zip'
    Invoke-WebRequest $asset[0].browser_download_url -OutFile $archive
    $digest=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()
    if (!$asset[0].digest -or $asset[0].digest -ne "sha256:$digest") { throw 'UPX digest missing or mismatched' }
    Expand-Archive -LiteralPath $archive -DestinationPath $stage
    $upx=Join-Path $stage "upx-$upxVersion-win64/upx.exe"
    $packed=Join-Path $stage 'Look@LAN-ng.exe'
    & $upx --ultra-brute --lzma -o $packed $exe
    if ($LASTEXITCODE) { throw 'UPX compression failed' }
    & $upx -t $packed
    if ($LASTEXITCODE) { throw 'UPX integrity failed' }
    $smokes=[ordered]@{}
    foreach ($argument in @('--smoke-test','--smoke-dpi','--smoke-close-scan','--smoke-close-tools')) {
        $process=Start-Process $packed -ArgumentList $argument -PassThru -WindowStyle Hidden
        if (!$process.WaitForExit(30000)) { $process.Kill(); $process.WaitForExit(); throw "Packed $argument timeout" }
        if ($process.ExitCode -eq 77 -and $argument -in @('--smoke-close-scan','--smoke-close-tools')) { $smokes[$argument]='skipped_no_ipv4' }
        elseif ($process.ExitCode -ne 0) { throw "Packed $argument failed: $($process.ExitCode)" }
        else { $smokes[$argument]='passed' }
    }
    $destination=Join-Path $output 'Look@LAN-ng.exe'
    Copy-Item -LiteralPath $packed -Destination $destination -Force
    [ordered]@{
        version=$version;architecture=$Architecture;copyright=(Get-Item $exe).VersionInfo.LegalCopyright
        upx_version=$upxVersion;upx_release=$release.html_url;upx_archive_sha256=$digest
        options='--ultra-brute --lzma';integrity='passed';packed_smokes=$smokes
        runner_arch=$env:PROCESSOR_ARCHITECTURE;unpacked_bytes=(Get-Item $exe).Length
        packed_bytes=(Get-Item $destination).Length;exe_sha256=(Get-FileHash $destination).Hash
        unpacked_sha256=(Get-FileHash $exe).Hash
    } | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $output 'build-info.json') -Encoding utf8
} finally {
    $resolved=[IO.Path]::GetFullPath($stage)
    if (!$resolved.StartsWith((Join-Path $projectRoot 'build')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Cleanup outside build' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
