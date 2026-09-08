param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$palette=[Collections.Generic.HashSet[int]]::new()
Get-Content -LiteralPath "$Project/config/art-palette.hex" | Where-Object {$_ -match '^#[0-9A-Fa-f]{6}$'} | ForEach-Object {
    [void]$palette.Add([Drawing.ColorTranslator]::FromHtml($_).ToArgb())
}
$files=@(Get-Content -LiteralPath "$Project/assets/sprites/sprites.catalog" | Where-Object {$_ -and -not $_.StartsWith('#')} | ForEach-Object {($_ -split '\s+')[1]} | Sort-Object -Unique)
$pixels=0
foreach($file in $files) {
    $bitmap=[Drawing.Bitmap]::new("$Project/assets/sprites/$file")
    try {
        for($y=0;$y -lt $bitmap.Height;$y++) {for($x=0;$x -lt $bitmap.Width;$x++) {
            $c=$bitmap.GetPixel($x,$y)
            if($c.A -ne 0 -and ($c.A -ne 255 -or -not $palette.Contains($c.ToArgb()))) {throw "Invalid palette/alpha: $file at $x,$y"}
            $pixels++
        }}
    } finally {$bitmap.Dispose()}
}
"Verified $($files.Count) referenced PNGs, $pixels pixels: exact 64-color palette; alpha is 0 or 255."
