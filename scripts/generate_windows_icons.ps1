# Generate multi-size icon files for the Windows IME from an Android-style reference.
# The output folder is ../icons from this script location.

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$outputDir = Join-Path $scriptDir '..\icons'
if (-not (Test-Path $outputDir)) { New-Item -ItemType Directory -Path $outputDir | Out-Null }

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Drawing.Drawing2D

function New-RoundedRectanglePath {
    param(
        [System.Drawing.Rectangle]$Rect,
        [int]$Radius
    )
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $diameter = $Radius * 2
    $path.AddArc($Rect.X, $Rect.Y, $diameter, $diameter, 180, 90)
    $path.AddArc($Rect.X + $Rect.Width - $diameter, $Rect.Y, $diameter, $diameter, 270, 90)
    $path.AddArc($Rect.X + $Rect.Width - $diameter, $Rect.Y + $Rect.Height - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($Rect.X, $Rect.Y + $Rect.Height - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

function Draw-Flower {
    param(
        [System.Drawing.Graphics]$g,
        [System.Drawing.PointF]$center,
        [double]$scale
    )
    $petalRadius = 0.22 * $scale
    $centerRadius = 0.12 * $scale
    $petalOffset = 0.35 * $scale
    $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
    for ($i = 0; $i -lt 5; $i++) {
        $angle = $i * 72 * [Math]::PI / 180
        $x = $center.X + [Math]::Cos($angle) * $petalOffset
        $y = $center.Y + [Math]::Sin($angle) * $petalOffset
        $g.FillEllipse($brush, $x - $petalRadius, $y - $petalRadius, $petalRadius * 2, $petalRadius * 2)
    }
    $g.FillEllipse($brush, $center.X - $centerRadius, $center.Y - $centerRadius, $centerRadius * 2, $centerRadius * 2)
    $brush.Dispose()
}

function Draw-Icon {
    param(
        [int]$size,
        [string]$type
    )
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

    $rectF = New-Object System.Drawing.RectangleF -ArgumentList @(0.5, 0.5, ($size - 1), ($size - 1))
    $color1 = [System.Drawing.Color]::FromArgb(0, 122, 204)
    $color2 = [System.Drawing.Color]::FromArgb(0, 80, 160)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList @($rectF, $color1, $color2, 45)
    $g.FillRectangle($brush, $rectF)
    $brush.Dispose()

    $corner = [Math]::Max(8, [Math]::Floor($size * 0.12))
    $outerPath = New-RoundedRectanglePath -Rect (New-Object System.Drawing.Rectangle -ArgumentList @(0, 0, $size, $size)) -Radius $corner
    $pen = New-Object System.Drawing.Pen -ArgumentList @([System.Drawing.Color]::FromArgb(220, 220, 220), [Math]::Max(2, [Math]::Floor($size * 0.04)))
    $g.DrawPath($pen, $outerPath)
    $pen.Dispose()

    $flowerCenter = New-Object System.Drawing.PointF -ArgumentList @($size * 0.28, $size * 0.24)
    Draw-Flower -g $g -center $flowerCenter -scale $size

    $keyRect = New-Object System.Drawing.RectangleF -ArgumentList @($size * 0.14, $size * 0.50, $size * 0.72, $size * 0.30)
    $brush2 = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(220, 255, 255, 255))
    $keyPath = New-RoundedRectanglePath -Rect ([System.Drawing.Rectangle]::Truncate($keyRect)) -Radius ([Math]::Max(3, [Math]::Floor($size * 0.04)))
    $g.FillPath($brush2, $keyPath)
    $linePen = New-Object System.Drawing.Pen -ArgumentList @([System.Drawing.Color]::FromArgb(180, 220, 220, 220), [Math]::Max(2, [Math]::Floor($size * 0.02)))
    $g.DrawPath($linePen, $keyPath)
    $linePen.Dispose()
    $brush2.Dispose()

    $keySize = [Math]::Floor($size * 0.12)
    $keyBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
    for ($col = 0; $col -lt 3; $col++) {
        $x = $size * 0.22 + $col * ($keySize + $size * 0.03)
        $y = $size * 0.60
        $g.FillRectangle($keyBrush, $x, $y, $keySize, $keySize)
    }
    $keyBrush.Dispose()

    if ($type -eq 'app' -or $type -eq 'uac') {
        $fontSize = [Math]::Max(10, [Math]::Floor($size * 0.25))
        try {
            $font = New-Object System.Drawing.Font 'Segoe UI Semibold', $fontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel
        } catch {
            $font = New-Object System.Drawing.Font 'Segoe UI', $fontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel
        }
        $text = if ($type -eq 'uac') { 'U' } else { 'T' }
        $textBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
        $format = New-Object System.Drawing.StringFormat
        $format.Alignment = [System.Drawing.StringAlignment]::Center
        $format.LineAlignment = [System.Drawing.StringAlignment]::Center
        $textRect = New-Object System.Drawing.RectangleF ($size * 0.28), ($size * 0.26), ($size * 0.44), ($size * 0.44)
        $g.DrawString($text, $font, $textBrush, $textRect, $format)
        $format.Dispose()
        $textBrush.Dispose()
        $font.Dispose()
    }

    if ($type -eq 'uac') {
        $shieldBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(180, 0, 180, 0))
        $shieldPath = New-Object System.Drawing.Drawing2D.GraphicsPath
        $shieldWidth = $size * 0.30
        $shieldHeight = $size * 0.30
        $sx = $size * 0.56
        $sy = $size * 0.08
        $shieldPath.AddArc($sx, $sy, $shieldWidth * 0.5, $shieldHeight * 0.5, 180, 180)
        $shieldPath.AddLine($sx + $shieldWidth * 0.5, $sy + $shieldHeight * 0.5, $sx + $shieldWidth, $sy + $shieldHeight * 0.65)
        $shieldPath.AddLine($sx + $shieldWidth, $sy + $shieldHeight * 0.65, $sx + $shieldWidth * 0.5, $sy + $shieldHeight)
        $shieldPath.AddLine($sx + $shieldWidth * 0.5, $sy + $shieldHeight, $sx, $sy + $shieldHeight * 0.65)
        $shieldPath.CloseFigure()
        $g.FillPath($shieldBrush, $shieldPath)
        $shieldBrush.Dispose()
        $shieldPath.Dispose()
    }

    $g.Dispose()
    return $bmp
}

function Save-Icon {
    param(
        [string]$fileName,
        [string]$type,
        [int[]]$sizes
    )
    $entries = @()
    foreach ($size in $sizes) {
        $bmp = Draw-Icon -size $size -type $type
        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        $entries += ,[pscustomobject]@{Size = $size; Data = $ms.ToArray()}
    }

    $icoPath = Join-Path $outputDir $fileName
    $fs = [System.IO.File]::Create($icoPath)
    $writer = New-Object System.IO.BinaryWriter($fs)
    $writer.Write([UInt16]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]$entries.Count)

    $offset = 6 + 16 * $entries.Count
    foreach ($entry in $entries) {
        $width = if ($entry.Size -lt 256) { [byte]$entry.Size } else { [byte]0 }
        $height = if ($entry.Size -lt 256) { [byte]$entry.Size } else { [byte]0 }
        $writer.Write($width)
        $writer.Write($height)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]$entry.Data.Length)
        $writer.Write([UInt32]$offset)
        $offset += $entry.Data.Length
    }

    foreach ($entry in $entries) {
        $writer.Write($entry.Data)
    }
    $writer.Close()
    $fs.Close()
    Write-Host "Created $icoPath with sizes $($entries.Size -join ', ')"
}

Save-Icon -fileName 'tcode_app.ico' -type 'app' -sizes @(256,128,64,48,32,16)
Save-Icon -fileName 'tcode_tray.ico' -type 'tray' -sizes @(128,64,48,32,16)
Save-Icon -fileName 'tcode_uac.ico' -type 'uac' -sizes @(128,64,48,32,16)

Get-ChildItem $outputDir | Select-Object Name, Length | Format-Table
