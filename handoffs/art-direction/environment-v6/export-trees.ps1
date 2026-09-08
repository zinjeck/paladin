$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root='C:/Paladin'
$source=[Drawing.Bitmap]::new("$root/handoffs/art-direction/environment-v6/sources/tree-parts.png")
$palette=@(Import-Csv "$root/handoffs/art-direction/sunlight-and-shadow/palette-64.csv" | ForEach-Object {[Drawing.ColorTranslator]::FromHtml($_.Hex)})
foreach($row in 0..2){foreach($column in 0..2){
 $left=[int][Math]::Floor($column*$source.Width/3);$top=[int][Math]::Floor($row*$source.Height/3)
 $right=[int][Math]::Floor(($column+1)*$source.Width/3);$bottom=[int][Math]::Floor(($row+1)*$source.Height/3)
 $x0=$right;$y0=$bottom;$x1=$left;$y1=$top
 for($y=$top;$y -lt $bottom;$y++){for($x=$left;$x -lt $right;$x++){if($source.GetPixel($x,$y).A -ge 128){$x0=[Math]::Min($x0,$x);$y0=[Math]::Min($y0,$y);$x1=[Math]::Max($x1,$x);$y1=[Math]::Max($y1,$y)}}}
 $kind=@('trunk','branch','crown')[$row];$w=@(12,18,24)[$row];$h=@(16,14,22)[$row]
 $image=[Drawing.Bitmap]::new($w,$h)
 for($y=0;$y -lt $h;$y++){for($x=0;$x -lt $w;$x++){
  $sx=$x0+[int][Math]::Floor(($x+.5)*($x1-$x0+1)/$w);$sy=$y0+[int][Math]::Floor(($y+.5)*($y1-$y0+1)/$h)
  $c=$source.GetPixel($sx,$sy);if($c.A -lt 128){continue}
  $best=$palette[0];$distance=[double]::PositiveInfinity
  foreach($p in $palette){$d=([int]$c.R-$p.R)*([int]$c.R-$p.R)+([int]$c.G-$p.G)*([int]$c.G-$p.G)+([int]$c.B-$p.B)*([int]$c.B-$p.B);if($d -lt $distance){$best=$p;$distance=$d}}
  $image.SetPixel($x,$y,$best)
 }}
 $image.Save("$root/assets/sprites/environment-v6/tree-$kind-$($column+1).png",[Drawing.Imaging.ImageFormat]::Png);$image.Dispose()
}}
$source.Dispose()
