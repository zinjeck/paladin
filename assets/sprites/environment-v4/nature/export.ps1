Add-Type -AssemblyName System.Drawing
$destRoot = $PSScriptRoot
$specs = Get-Content -LiteralPath (Join-Path $destRoot 'source-manifest.json') -Raw | ConvertFrom-Json
$allowed = @{}
Import-Csv 'C:/Paladin/handoffs/art-direction/sunlight-and-shadow/palette-64.csv' | ForEach-Object { $allowed[$_.Hex.ToUpper()] = $true }
$report = @()
foreach ($spec in $specs) {
    $source = [System.Drawing.Bitmap]::new($spec.source)
    $reduced = [System.Drawing.Bitmap]::new([int]$spec.width, [int]$spec.height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($reduced)
    $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.DrawImage($source, [System.Drawing.Rectangle]::new(0,0,$spec.width,$spec.height),0,0,$source.Width,$source.Height,[System.Drawing.GraphicsUnit]::Pixel)
    $graphics.Dispose()
    $palette = @($spec.palette | ForEach-Object { [System.Drawing.ColorTranslator]::FromHtml('#'+$_) })
    $result = [System.Drawing.Bitmap]::new([int]$spec.width, [int]$spec.height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $used = @{}
    $opaque = 0
    $offPalette = 0
    for ($y=0; $y -lt $spec.height; $y++) {
      for ($x=0; $x -lt $spec.width; $x++) {
        $pixel = $reduced.GetPixel($x,$y)
        if ($spec.transparent -and $pixel.A -lt 128) { $result.SetPixel($x,$y,[System.Drawing.Color]::Transparent); continue }
        $distance = [double]::PositiveInfinity
        $closest = $palette[0]
        foreach ($color in $palette) {
          $dr = [double]$pixel.R-$color.R; $dg = [double]$pixel.G-$color.G; $db = [double]$pixel.B-$color.B
          $candidate = $dr*$dr + $dg*$dg + $db*$db
          if ($candidate -lt $distance) { $distance=$candidate; $closest=$color }
        }
        $result.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(255,$closest.R,$closest.G,$closest.B))
        $hex = '#{0:X2}{1:X2}{2:X2}' -f $closest.R,$closest.G,$closest.B
        $used[$hex]=$true; $opaque++
        if (-not $allowed.ContainsKey($hex)) {$offPalette++}
      }
    }
    $path = Join-Path $destRoot ($spec.key+'.png')
    $result.Save($path,[System.Drawing.Imaging.ImageFormat]::Png)
    $report += [pscustomobject]@{file=$spec.key+'.png';width=$spec.width;height=$spec.height;opaque=$opaque;transparent=($spec.width*$spec.height-$opaque);colors=$used.Count;offPalette=$offPalette;partialAlpha=0;hex=@($used.Keys|Sort-Object)}
    $result.Dispose();$reduced.Dispose();$source.Dispose()
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $destRoot 'verification.json')
$report | Select-Object file,width,height,opaque,transparent,colors,offPalette,partialAlpha | Format-Table -AutoSize
