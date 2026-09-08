$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root='C:/Paladin'
$colors=@(Import-Csv "$root/handoffs/art-direction/sunlight-and-shadow/palette-64.csv" | ForEach-Object {[System.Drawing.ColorTranslator]::FromHtml($_.Hex)})
function Export-Crop($source,$name,$w,$h,$left=0,$top=0,$width=1,$height=1,$greens=$false){
 $b=[System.Drawing.Bitmap]::new($w,$h)
 $choices=if($greens){@('337A58','49975B','A6CD59','D0E58A','235747')|ForEach-Object{[System.Drawing.ColorTranslator]::FromHtml('#'+$_)}}else{$colors}
 for($y=0;$y -lt $h;$y++){for($x=0;$x -lt $w;$x++){
  $sx=[int][Math]::Floor(($left+($x+.5)/$w*$width)*$source.Width);$sy=[int][Math]::Floor(($top+($y+.5)/$h*$height)*$source.Height)
  $c=$source.GetPixel($sx,$sy);if($c.A -lt 128){continue};$distance=[double]::PositiveInfinity;$best=$choices[0]
  foreach($p in $choices){$d=([int]$c.R-$p.R)*([int]$c.R-$p.R)+([int]$c.G-$p.G)*([int]$c.G-$p.G)+([int]$c.B-$p.B)*([int]$c.B-$p.B);if($d -lt $distance){$best=$p;$distance=$d}}
  $b.SetPixel($x,$y,$best)
 }}
 $b.Save("$root/assets/sprites/environment-v5/$name.png",[System.Drawing.Imaging.ImageFormat]::Png);$b.Dispose()
}
$source=[System.Drawing.Bitmap]::new("$root/handoffs/art-direction/environment-v5/mud-facade-source.png")
Export-Crop $source 'mud-facade' 96 28
Export-Crop $source 'mud-wall' 32 32 .70 .12 .22 .74
Export-Crop $source 'mud-rear-panel' 48 28 .015 .015 .35 .96
Export-Crop $source 'mud-cap' 96 4 .1 .05 .8 .08
Export-Crop $source 'mud-side-cap' 4 96 .025 .12 .035 .72
$source.Dispose()
$panel=[System.Drawing.Bitmap]::new("$root/assets/sprites/environment-v5/mud-rear-panel.png")
$rear=[System.Drawing.Bitmap]::new(96,28)
for($y=0;$y -lt 28;$y++){for($x=0;$x -lt 96;$x++){$sx=if($x -lt 48){$x}else{95-$x};$c=$panel.GetPixel($sx,$y);if($c.A -lt 128){$c=[System.Drawing.ColorTranslator]::FromHtml('#A99478')};$rear.SetPixel($x,$y,$c)}}
$rear.Save("$root/assets/sprites/environment-v5/mud-rear.png",[System.Drawing.Imaging.ImageFormat]::Png);$rear.Dispose();$panel.Dispose()
$cap=[System.Drawing.Bitmap]::new("$root/assets/sprites/environment-v5/mud-cap.png")
for($y=0;$y -lt 4;$y++){for($x=32;$x -lt 64;$x++){$cap.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(0,0,0,0))}}
$cap.Save("$root/assets/sprites/environment-v5/mud-front-cap.png",[System.Drawing.Imaging.ImageFormat]::Png);$cap.Dispose()
$source=[System.Drawing.Bitmap]::new("$root/handoffs/art-direction/environment-v5/grass-tuft-source.png")
Export-Crop $source 'grass-tuft' 12 12 .04 .025 .92 .95 $true
$source.Dispose()
$ground=[System.Drawing.Bitmap]::new("$root/assets/sprites/environment-v4/nature/grass.png")
$map=@{'49975B'='337A58';'79B56D'='49975B';'337A58'='235747'}
for($y=0;$y -lt $ground.Height;$y++){for($x=0;$x -lt $ground.Width;$x++){$c=$ground.GetPixel($x,$y);$hex='{0:X2}{1:X2}{2:X2}' -f $c.R,$c.G,$c.B;if(-not $map.ContainsKey($hex)){throw "Unmapped ground color $hex"};$ground.SetPixel($x,$y,[System.Drawing.ColorTranslator]::FromHtml('#'+$map[$hex]))}}
$ground.Save("$root/assets/sprites/environment-v5/grass-ground.png",[System.Drawing.Imaging.ImageFormat]::Png);$ground.Dispose()
