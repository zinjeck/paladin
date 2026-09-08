$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root='C:/Paladin'
function Export-Tile($source,$name,$row,$frames,$size,$hexes){
 $palette=@($hexes | ForEach-Object {[Drawing.ColorTranslator]::FromHtml('#'+$_)})
 $output=[Drawing.Bitmap]::new($frames*$size,$size)
 for($frame=0;$frame -lt $frames;$frame++){for($y=0;$y -lt $size;$y++){for($x=0;$x -lt $size;$x++){
  $sx=[int][Math]::Floor(($frame+($x+.5)/$size)*$source.Width/$frames)
  $sy=if($frames -eq 3){[int][Math]::Floor(($row+($y+.5)/$size)*$source.Height/2)}else{[int][Math]::Floor(($y+.5)*$source.Height/$size)}
  $c=$source.GetPixel($sx,$sy);$best=$palette[0];$distance=[double]::PositiveInfinity
  foreach($p in $palette){$d=([int]$c.R-$p.R)*([int]$c.R-$p.R)+([int]$c.G-$p.G)*([int]$c.G-$p.G)+([int]$c.B-$p.B)*([int]$c.B-$p.B);if($d -lt $distance){$best=$p;$distance=$d}}
  $output.SetPixel($frame*$size+$x,$y,$best)
 }}}
 $output.Save("$root/assets/sprites/environment-v7/$name.png",[Drawing.Imaging.ImageFormat]::Png);$output.Dispose()
}
$water=[Drawing.Bitmap]::new("$root/handoffs/art-direction/environment-v7/sources/water-calm.png")
Export-Tile $water 'water-deep' 0 3 32 @('3F5F9A','548AC4')
Export-Tile $water 'water-shallow' 1 3 32 @('548AC4','63BFC3','AFC9D6')
$water.Dispose()
$beach=[Drawing.Bitmap]::new("$root/handoffs/art-direction/environment-v7/sources/beach.png")
Export-Tile $beach 'beach' 0 1 16 @('D9C79F','F4E7C7')
$beach.Dispose()
