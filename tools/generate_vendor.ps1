param(
    [Parameter(Mandatory=$true)][string]$InputCsv,
    [Parameter(Mandatory=$true)][string]$OutputBinary
)
$ErrorActionPreference = 'Stop'
# Offline build-time generator. The application never downloads this database.
$vendors = @{}
Import-Csv -LiteralPath $InputCsv -Encoding utf8 | ForEach-Object {
    if ($_.Registry -ne 'MA-L' -or $_.Assignment -notmatch '^[0-9A-Fa-f]{6}$') { return }
    $prefix = [Convert]::ToUInt32($_.Assignment, 16)
    $name = $_.'Organization Name'.Trim()
    if ($name.Length -eq 0 -or [Text.Encoding]::UTF8.GetByteCount($name) -gt 1024) { throw 'Invalid vendor name length' }
    $vendors[$prefix] = $name
}
if ($vendors.Count -eq 0) { throw 'No MA-L entries found' }
$prefixes = @($vendors.Keys | Sort-Object)
$strings = [IO.MemoryStream]::new()
$offsets = @{}
$records = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($records)
try {
    foreach ($prefix in $prefixes) {
        $name = $vendors[$prefix]
        if (-not $offsets.ContainsKey($name)) {
            $offsets[$name] = [uint32]$strings.Length
            $bytes = [Text.Encoding]::UTF8.GetBytes($name)
            $strings.Write($bytes, 0, $bytes.Length)
            $strings.WriteByte(0)
        }
        $writer.Write([uint32]$prefix)
        $writer.Write([uint32]$offsets[$name])
    }
    $target = [IO.File]::Create([IO.Path]::GetFullPath($OutputBinary))
    $output = [IO.BinaryWriter]::new($target)
    try {
        $output.Write([Text.Encoding]::ASCII.GetBytes('OUI1'))
        $output.Write([uint32]$prefixes.Count)
        $output.Write([uint32]$strings.Length)
        $records.Position = 0; $records.CopyTo($target)
        $strings.Position = 0; $strings.CopyTo($target)
    } finally { $output.Dispose() }
} finally { $writer.Dispose(); $records.Dispose(); $strings.Dispose() }
Write-Output "Generated $($prefixes.Count) MA-L prefixes, $((Get-Item -LiteralPath $OutputBinary).Length) bytes."
