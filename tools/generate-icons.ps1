$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$assetRoot = Join-Path (Split-Path $PSScriptRoot -Parent) 'src\resources\assets'
New-Item -ItemType Directory -Force -Path $assetRoot | Out-Null
$svg = @'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
 <rect x="8" y="8" width="240" height="240" rx="48" fill="#142d3a"/>
 <circle cx="128" cy="128" r="84" fill="none" stroke="#3c746b" stroke-width="8"/>
 <circle cx="128" cy="128" r="47" fill="none" stroke="#3c746b" stroke-width="6"/>
 <path d="M65 188L128 128L188 66" fill="none" stroke="#72e4a4" stroke-width="10" stroke-linecap="round"/>
 <circle cx="65" cy="188" r="13" fill="#d4f3ed"/>
 <circle cx="128" cy="128" r="16" fill="#72e4a4"/>
 <circle cx="188" cy="66" r="13" fill="#70d6ed"/>
</svg>
'@
Set-Content -LiteralPath (Join-Path $assetRoot 'app.svg') -Value $svg -Encoding UTF8
$frames = @()
foreach ($size in @(16,24,32,48,64,128,256)) {
 $canvas = [Drawing.Bitmap]::new(1024,1024); $graphics = [Drawing.Graphics]::FromImage($canvas)
 $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
 $graphics.ScaleTransform(4,4); $graphics.Clear([Drawing.Color]::Transparent)
 $path = [Drawing.Drawing2D.GraphicsPath]::new()
 foreach ($arc in @(@(8,8,180),@(152,8,270),@(152,152,0),@(8,152,90))) { $path.AddArc($arc[0],$arc[1],96,96,$arc[2],90) }; $path.CloseFigure()
 $background = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#142d3a')); $graphics.FillPath($background,$path)
 $ring = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#3c746b'),8); $graphics.DrawEllipse($ring,44,44,168,168); $ring.Width = 6; $graphics.DrawEllipse($ring,81,81,94,94)
 $line = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#72e4a4'),10); $line.StartCap = $line.EndCap = [Drawing.Drawing2D.LineCap]::Round
 $graphics.DrawLine($line,65,188,128,128); $graphics.DrawLine($line,128,128,188,66)
 foreach ($node in @(@(65,188,13,'#d4f3ed'),@(128,128,16,'#72e4a4'),@(188,66,13,'#70d6ed'))) { $brush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($node[3])); $graphics.FillEllipse($brush,$node[0]-$node[2],$node[1]-$node[2],$node[2]*2,$node[2]*2); $brush.Dispose() }
 $small = [Drawing.Bitmap]::new($size,$size); $scaled = [Drawing.Graphics]::FromImage($small); $scaled.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
 $scaled.DrawImage($canvas,0,0,$size,$size); $stream = [IO.MemoryStream]::new(); $small.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
 $frames += ,@{Size=$size; Bytes=$stream.ToArray()}
 if ($size -eq 256) { $small.Save((Join-Path $assetRoot 'app.png'),[Drawing.Imaging.ImageFormat]::Png) }
 $stream.Dispose(); $scaled.Dispose(); $small.Dispose(); $line.Dispose(); $ring.Dispose(); $background.Dispose(); $path.Dispose(); $graphics.Dispose(); $canvas.Dispose()
}
$file = [IO.File]::Create((Join-Path $assetRoot 'app.ico')); $writer = [IO.BinaryWriter]::new($file)
try {
 $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count); $offset = 6 + 16*$frames.Count
 foreach ($frame in $frames) { $dimension = if ($frame.Size -eq 256) { 0 } else { $frame.Size }; $writer.Write([byte]$dimension); $writer.Write([byte]$dimension); $writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$frame.Bytes.Length); $writer.Write([uint32]$offset); $offset += $frame.Bytes.Length }
 foreach ($frame in $frames) { $writer.Write([byte[]]$frame.Bytes) }
} finally { $writer.Dispose(); $file.Dispose() }
Write-Output 'Original Look@LAN-ng SVG/PNG/multi-resolution ICO generated.'
