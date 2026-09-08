Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System.Drawing;
public static class BuildingAlphaBounds {
 public static int[] Get(Bitmap b) { int x0=b.Width,y0=b.Height,x1=0,y1=0;
  for(int y=0;y<b.Height;y++)for(int x=0;x<b.Width;x++)if(b.GetPixel(x,y).A>=128){x0=System.Math.Min(x0,x);y0=System.Math.Min(y0,y);x1=System.Math.Max(x1,x);y1=System.Math.Max(y1,y);}
  return new int[]{x0,y0,x1,y1};
 }
}
'@ -ReferencedAssemblies System.Drawing.Common,System.Drawing.Primitives
$project = 'C:/Paladin'
$palette = @(Import-Csv "$project/handoffs/art-direction/sunlight-and-shadow/palette-64.csv" | ForEach-Object { [System.Drawing.ColorTranslator]::FromHtml($_.Hex) })
$destRoot = "$project/assets/sprites/environment-v4/buildings"
function Export-Part($source, $rect, $width, $height, $name) {
    $output = [System.Drawing.Bitmap]::new($width,$height)
    for ($y=0;$y -lt $height;$y++) { for ($x=0;$x -lt $width;$x++) {
        # Slightly broader pixel clusters without changing the runtime dimensions.
        $u=([Math]::Floor($x*.75)+.5)/[Math]::Ceiling($width*.75)
        $v=([Math]::Floor($y*.75)+.5)/[Math]::Ceiling($height*.75)
        $sx=[Math]::Min($source.Width-1,[int][Math]::Floor($rect[0]+$u*$rect[2]))
        $sy=[Math]::Min($source.Height-1,[int][Math]::Floor($rect[1]+$v*$rect[3]))
        $c=$source.GetPixel($sx,$sy); if($c.A -lt 128){continue}
        $best=$palette[0];$distance=[double]::PositiveInfinity
        foreach($p in $palette){$d=([int]$c.R-$p.R)*([int]$c.R-$p.R)+([int]$c.G-$p.G)*([int]$c.G-$p.G)+([int]$c.B-$p.B)*([int]$c.B-$p.B);if($d -lt $distance){$best=$p;$distance=$d}}
        $output.SetPixel($x,$y,$best)
    }}
    $output.Save("$destRoot/$name.png",[System.Drawing.Imaging.ImageFormat]::Png)
    $output.Dispose()
}
foreach($job in @(@('house',990,100,28),@('keep',1530,220,36),@('bakery',830,100,32))){
    $id=$job[0];$source=[System.Drawing.Bitmap]::new("$project/handoffs/art-direction/environment-v4/sources/$id.png")
    $bounds=[BuildingAlphaBounds]::Get($source)
    $minX=$bounds[0];$minY=$bounds[1];$maxX=$bounds[2];$maxY=$bounds[3]
    $w=$maxX-$minX+1;$h=$maxY-$minY+1;$split=$job[1]
    if($id -eq 'house') { Export-Part $source @($minX,$minY,$w,($split-$minY)) 112 $job[2] "$id-roof" }
    Export-Part $source @(($minX+$w/14),$split,($w*6/7),($maxY-$split+1)) 96 $job[3] "$id-facade"
    Export-Part $source @($minX,$minY,$w,$h) 32 40 "$id-marker"
    $source.Dispose()
}
$map=@{DF947F='D9C79F';F3B69A='F4E7C7';CB7C70='A99478';B76858='886044';'874D50'='74513F';'633E4B'='4E3B39';'392B3C'='49352F';'95655F'='A99478';'080F1B'='080F1B'}
foreach($part in @(@('tribal-interior-north-v2.png','interior-wall'),@('tribal-sidecap-v2.png','side-cap'),@('tribal-wallcap-open-v2.png','front-cap'),@('tribal-mud-wall-v1.png','wall-material'))){
    $b=[System.Drawing.Bitmap]::new("$project/assets/sprites/structures/$($part[0])")
    for($y=0;$y -lt $b.Height;$y++){for($x=0;$x -lt $b.Width;$x++){$c=$b.GetPixel($x,$y);if($c.A -eq 0){continue};$hex='{0:X2}{1:X2}{2:X2}' -f $c.R,$c.G,$c.B;if(-not $map.ContainsKey($hex)){throw "Unmapped $hex"};$b.SetPixel($x,$y,[System.Drawing.ColorTranslator]::FromHtml('#'+$map[$hex]))}}
    $b.Save("$destRoot/$($part[1]).png",[System.Drawing.Imaging.ImageFormat]::Png);$b.Dispose()
}
