$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root='C:/Paladin'
$source=[Drawing.Bitmap]::new("$root/handoffs/art-direction/environment-v6/sources/roof-alpha.png")
$palette=@(Import-Csv "$root/handoffs/art-direction/sunlight-and-shadow/palette-64.csv" | ForEach-Object {[Drawing.ColorTranslator]::FromHtml($_.Hex)})
$roof=[Drawing.Bitmap]::new(56,50)
for($y=0;$y -lt 50;$y++){for($x=0;$x -lt 56;$x++){
 $c=$source.GetPixel([int][Math]::Floor(($x+.5)*$source.Width/56),[int][Math]::Floor(($y+.5)*$source.Height/50))
 if($c.A -lt 128){continue};$best=$palette[0];$distance=[double]::PositiveInfinity
 foreach($p in $palette){$d=([int]$c.R-$p.R)*([int]$c.R-$p.R)+([int]$c.G-$p.G)*([int]$c.G-$p.G)+([int]$c.B-$p.B)*([int]$c.B-$p.B);if($d -lt $distance){$distance=$d;$best=$p}}
 $roof.SetPixel($x,$y,$best)
}}
$roof.Save("$root/assets/sprites/environment-v6/house-roof.png",[Drawing.Imaging.ImageFormat]::Png)
# Repeat the plain thatch band, retaining ridge pegs and front roof geometry.
$keep=[Drawing.Bitmap]::new(56,114)
for($y=0;$y -lt 114;$y++){$sy=if($y -lt 8){$y}elseif($y -lt 80){8+($y-8)%8}else{16+$y-80};for($x=0;$x -lt 56;$x++){$keep.SetPixel($x,$y,$roof.GetPixel($x,$sy))}}
$keep.Save("$root/assets/sprites/environment-v6/keep-roof.png",[Drawing.Imaging.ImageFormat]::Png)
$keep.Dispose();$roof.Dispose();$source.Dispose()
