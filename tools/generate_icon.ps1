# Generates resources/perfpulse.ico (16..256 px, PNG-compressed entries).
# The design matches the runtime tray icon drawn by src/app/icon_factory.cpp:
# a rounded square with a heartbeat-style pulse line.
param(
    [string]$Output = (Join-Path $PSScriptRoot '..\resources\perfpulse.ico')
)

Add-Type -AssemblyName System.Drawing

function New-PulseBitmap([int]$size) {
    $bitmap = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $radius = [Math]::Max(2, $size * 0.22)
    $rect = New-Object System.Drawing.RectangleF 0, 0, ($size - 1), ($size - 1)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $radius * 2
    $path.AddArc($rect.X, $rect.Y, $d, $d, 180, 90)
    $path.AddArc($rect.Right - $d, $rect.Y, $d, $d, 270, 90)
    $path.AddArc($rect.Right - $d, $rect.Bottom - $d, $d, $d, 0, 90)
    $path.AddArc($rect.X, $rect.Bottom - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect,
        ([System.Drawing.Color]::FromArgb(255, 124, 92, 255)),
        ([System.Drawing.Color]::FromArgb(255, 34, 211, 238)), 45.0
    $g.FillPath($brush, $path)

    $s = $size / 32.0
    $points = @(
        (New-Object System.Drawing.PointF (4 * $s), (17 * $s)),
        (New-Object System.Drawing.PointF (10 * $s), (17 * $s)),
        (New-Object System.Drawing.PointF (13 * $s), (8 * $s)),
        (New-Object System.Drawing.PointF (17 * $s), (25 * $s)),
        (New-Object System.Drawing.PointF (20 * $s), (14 * $s)),
        (New-Object System.Drawing.PointF (22 * $s), (17 * $s)),
        (New-Object System.Drawing.PointF (28 * $s), (17 * $s))
    )
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::White), ([Math]::Max(1.5, 2.6 * $s))
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLines($pen, $points)
    $g.Dispose()
    return $bitmap
}

$sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256
$images = foreach ($size in $sizes) {
    $bitmap = New-PulseBitmap $size
    $stream = New-Object System.IO.MemoryStream
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    [PSCustomObject]@{ Size = $size; Bytes = $stream.ToArray() }
}

$out = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $out
$writer.Write([UInt16]0)               # reserved
$writer.Write([UInt16]1)               # type: icon
$writer.Write([UInt16]$images.Count)
$offset = 6 + 16 * $images.Count
foreach ($image in $images) {
    $dimension = if ($image.Size -ge 256) { 0 } else { $image.Size }
    $writer.Write([Byte]$dimension)
    $writer.Write([Byte]$dimension)
    $writer.Write([Byte]0)             # palette
    $writer.Write([Byte]0)             # reserved
    $writer.Write([UInt16]1)           # planes
    $writer.Write([UInt16]32)          # bits per pixel
    $writer.Write([UInt32]$image.Bytes.Length)
    $writer.Write([UInt32]$offset)
    $offset += $image.Bytes.Length
}
foreach ($image in $images) {
    $writer.Write($image.Bytes)
}
$writer.Flush()
[System.IO.File]::WriteAllBytes((Resolve-Path -LiteralPath (Split-Path $Output)).Path + '\' + (Split-Path $Output -Leaf), $out.ToArray())
Write-Host "Wrote $Output"
