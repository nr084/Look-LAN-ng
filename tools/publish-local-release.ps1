param([Parameter(Mandatory)][string]$Version,[Parameter(Mandatory)][string]$NotesFile,[Parameter(Mandatory)][string[]]$Assets,[switch]$Prerelease,[switch]$AddAssetsToPublishedRelease)
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+\.\d+([-.][A-Za-z0-9.]+)?$'){throw 'Invalid release version'}
$repository='nr084/Look-LAN-ng';$tag='v'+$Version
$files=@($Assets | ForEach-Object {Get-Item -LiteralPath $_});if(!$files.Count){throw 'No release assets'}
$developmentPath=Join-Path $PSScriptRoot '../dist/development-artifacts.json'
if(Test-Path -LiteralPath $developmentPath){
    $development=Get-Content -LiteralPath $developmentPath -Raw -Encoding utf8 | ConvertFrom-Json
    foreach($file in $files){
        if($development.assets.($file.Name) -eq (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()){
            throw 'Local development artifact was built without validation; it must not be published as a ready release.'
        }
    }
}
foreach($platform in @('linux','macos')){
    $portFiles=@($files | Where-Object { $_.Name -like ('look@lan-ng-'+$platform+'-*') })
    if(!$portFiles.Count){continue}
    if($Prerelease -or $Version -match '[-]'){throw 'Port preview publication is disabled by the user; publish only completed, validated ports.'}
    $proofPath=Join-Path $PSScriptRoot ('../dist/'+$platform+'-release-validation.json')
    if(!(Test-Path -LiteralPath $proofPath)){throw ($platform+' release validation report is missing.')}
    $proof=Get-Content -LiteralPath $proofPath -Raw -Encoding utf8 | ConvertFrom-Json
    if($proof.version -ne $Version -or $proof.native_tests -ne 'passed' -or $proof.feature_complete -ne $true){throw ($platform+' release is not complete and natively validated.')}
    foreach($file in $portFiles){if($proof.assets.($file.Name) -ne (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()){throw ($platform+' validation report does not match the release artifact.')}}
}
if(@($files.Name | Select-Object -Unique).Count -ne $files.Count){throw 'Duplicate asset names'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach($file in $files){
    if($file.Name -match '^look@lan-ng-(x86|x64|arm64)-'){
        $architecture=$Matches[1];$infoPath=Join-Path $PSScriptRoot "../dist/windows-$architecture/build-info.json"
        $info=Get-Content -LiteralPath $infoPath -Raw|ConvertFrom-Json
        if($info.version -ne $Version -or $info.integrity -ne 'passed'){throw 'Windows package version/integrity mismatch'}
        foreach($smoke in $info.packed_smokes.PSObject.Properties){if($smoke.Value -notin @('passed','skipped_no_ipv4')){throw 'Pending/failed Windows packed smoke'}}
        if($architecture -eq 'arm64' -and ($info.native_validation.architecture -ne 'ARM64' -or $info.native_validation.exe_sha256 -ne $info.exe_sha256)){throw 'Matching native ARM64 evidence required'}
        $archive=[IO.Compression.ZipFile]::OpenRead($file.FullName)
        try{if($archive.Entries.Count-ne1 -or $archive.Entries[0].FullName-ne'Look@LAN-ng.exe'){throw 'Windows ZIP must contain one EXE'};$stream=$archive.Entries[0].Open();try{$hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)).ToLowerInvariant()}finally{$stream.Dispose()};if($hash-ne$info.exe_sha256){throw 'Windows ZIP does not match tested packed EXE'}}finally{$archive.Dispose()}
    }
}
$notes=Get-Content -LiteralPath $NotesFile -Raw -Encoding utf8
$commit=(git rev-parse HEAD).Trim();if($LASTEXITCODE){throw 'Not in source repository'}
$env:GCM_INTERACTIVE='never';$env:GIT_TERMINAL_PROMPT='0'
$lines="protocol=https`nhost=github.com`n`n" | git credential fill 2>$null
$credential=@{};foreach($line in $lines){$pair=$line -split '=',2;if($pair.Count -eq 2){$credential[$pair[0]]=$pair[1]}}
if(!$credential['password']){throw 'GitHub credentials unavailable'}
$headers=@{Authorization=('Bearer '+$credential['password']);'User-Agent'='LookLAN-ng-local-release';Accept='application/vnd.github+json'}
$releases=Invoke-RestMethod "https://api.github.com/repos/$repository/releases?per_page=100" -Headers $headers
$release=@($releases | Where-Object tag_name -eq $tag)
if($release.Count -gt 1){throw 'Ambiguous release tag'}
if(!$release.Count){
    $body=@{tag_name=$tag;target_commitish=$commit;name="Look@LAN-ng $Version";body=$notes;draft=$true;prerelease=[bool]$Prerelease;make_latest='false'} | ConvertTo-Json
    $release=Invoke-RestMethod -Method Post "https://api.github.com/repos/$repository/releases" -Headers $headers -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($body))
}else{$release=$release[0];if(!$release.draft -and !$AddAssetsToPublishedRelease){throw 'Release already published; use explicit add-only mode to add newly validated platform files'}}
foreach($file in $files){
    $hash=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant();$existing=@($release.assets | Where-Object name -eq $file.Name)
    if($existing.Count){if($existing[0].digest -ne ('sha256:'+$hash)){throw 'Existing draft asset differs'};continue}
    $url=$release.upload_url -replace '\{.*\}$','';$url+='?name='+[Uri]::EscapeDataString($file.Name)
    $asset=Invoke-RestMethod -Method Post $url -Headers $headers -ContentType 'application/octet-stream' -InFile $file.FullName
    if($asset.digest -ne ('sha256:'+$hash) -or $asset.size -ne $file.Length){throw 'Uploaded asset digest/size mismatch'}
}
$body=@{draft=$false;prerelease=[bool]$Prerelease;body=$notes;make_latest=$(if($Prerelease){'false'}else{'true'})} | ConvertTo-Json
$published=Invoke-RestMethod -Method Patch "https://api.github.com/repos/$repository/releases/$($release.id)" -Headers $headers -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($body))
Write-Output $published.html_url
$published.assets | ForEach-Object {Write-Output ($_.name+' '+$_.size+' '+$_.digest)}
# Verify public downloads before removing any older release. Only validated
# current-version deliveries may replace the user's previous download page.
$checkRoot=Join-Path $PSScriptRoot ('../build/publication-check-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $checkRoot >$null
try{
    foreach($file in $files){$remote=@($published.assets|Where-Object name -eq $file.Name);if($remote.Count-ne1){throw 'Published delivery missing'};$download=Join-Path $checkRoot $file.Name;Invoke-WebRequest $remote[0].browser_download_url -OutFile $download;if((Get-FileHash -LiteralPath $download).Hash-ne(Get-FileHash -LiteralPath $file.FullName).Hash){throw 'Published download SHA-256 mismatch'}}
    $all=Invoke-RestMethod "https://api.github.com/repos/$repository/releases?per_page=100" -Headers $headers
    foreach($older in $all){if($older.id-eq$published.id){continue};if($older.tag_name-match'^v(\d+\.\d+\.\d+)'){$oldVersion=[version]$Matches[1];if($oldVersion-lt[version](($Version-split'-')[0])){Invoke-RestMethod -Method Delete "https://api.github.com/repos/$repository/releases/$($older.id)" -Headers $headers >$null;Write-Output ('Removed older release '+$older.tag_name)}}}
}finally{
    $resolved=[IO.Path]::GetFullPath($checkRoot);$build=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build'))+[IO.Path]::DirectorySeparatorChar
    if(!$resolved.StartsWith($build,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup outside project build directory'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
