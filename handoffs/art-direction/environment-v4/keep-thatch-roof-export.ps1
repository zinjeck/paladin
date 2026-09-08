$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing.Common,System.Drawing.Primitives,System.Collections -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
public static class RoofCutout {
  public static Rectangle ClearOutside(Bitmap bitmap) {
    int w=bitmap.Width,h=bitmap.Height;
    var queue = new Queue<int>();var seen=new bool[w*h];
    Action<int,int> add=(x,y)=>{int i=y*w+x;if(seen[i])return;seen[i]=true;var c=bitmap.GetPixel(x,y);int max=Math.Max(c.R,Math.Max(c.G,c.B));int min=Math.Min(c.R,Math.Min(c.G,c.B));if(c.A<128 || (max-min<35 && min>175)){queue.Enqueue(i);bitmap.SetPixel(x,y,Color.Transparent);}};
    for(int x=0;x<w;x++){add(x,0);add(x,h-1);}for(int y=0;y<h;y++){add(0,y);add(w-1,y);}
    while(queue.Count>0){int i=queue.Dequeue(),x=i%w,y=i/w;if(x>0)add(x-1,y);if(x<w-1)add(x+1,y);if(y>0)add(x,y-1);if(y<h-1)add(x,y+1);}
    int left=w,top=h,right=-1,bottom=-1;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(bitmap.GetPixel(x,y).A>=128){left=Math.Min(left,x);top=Math.Min(top,y);right=Math.Max(right,x);bottom=Math.Max(bottom,y);}
    return Rectangle.FromLTRB(left,top,right+1,bottom+1);
  }
}
'@
$sourcePath='C:/Paladin/handoffs/art-direction/environment-v4/sources/keep-thatch-roof-source.png'
$targetPath='C:/Paladin/assets/sprites/environment-v4/buildings/keep-thatch-roof.png'
$original=[System.Drawing.Bitmap]::new($sourcePath)
$source=[System.Drawing.Bitmap]::new($original.Width,$original.Height,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$sourceGraphics=[System.Drawing.Graphics]::FromImage($source)
$sourceGraphics.DrawImageUnscaled($original,0,0)
$sourceGraphics.Dispose();$original.Dispose()
$bounds=[RoofCutout]::ClearOutside($source)
$reduced=[System.Drawing.Bitmap]::new(84,171,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics=[System.Drawing.Graphics]::FromImage($reduced)
$graphics.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
$graphics.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$graphics.DrawImage($source,[System.Drawing.Rectangle]::new(0,0,84,171),$bounds,[System.Drawing.GraphicsUnit]::Pixel)
$graphics.Dispose();$source.Dispose()
$house=[System.Drawing.Bitmap]::new('C:/Paladin/assets/sprites/environment-v4/buildings/house-roof.png')
$houseColors=@{}
for($y=0;$y -lt $house.Height;$y++){for($x=0;$x -lt $house.Width;$x++){$c=$house.GetPixel($x,$y);if($c.A -eq 255){$houseColors[$c.ToArgb()]=$c}}}
$house.Dispose();$palette=@($houseColors.Values)
$quantized=[System.Drawing.Bitmap]::new(84,171,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
for($y=0;$y -lt 171;$y++){for($x=0;$x -lt 84;$x++){
 $c=$reduced.GetPixel($x,$y)
 if($c.A -lt 128){$quantized.SetPixel($x,$y,[System.Drawing.Color]::Transparent);continue}
 $best=[double]::PositiveInfinity;$match=$palette[0]
 foreach($p in $palette){$dr=[double]$c.R-$p.R;$dg=[double]$c.G-$p.G;$db=[double]$c.B-$p.B;$d=$dr*$dr+$dg*$dg+$db*$db;if($d -lt $best){$best=$d;$match=$p}}
 $quantized.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(255,$match.R,$match.G,$match.B))
}}
$reduced.Dispose()
$output=[System.Drawing.Bitmap]::new(112,228,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
for($y=0;$y -lt 228;$y++){for($x=0;$x -lt 112;$x++){$output.SetPixel($x,$y,$quantized.GetPixel([Math]::Min(83,[int][Math]::Floor($x*0.75)),[Math]::Min(170,[int][Math]::Floor($y*0.75))))}}
$quantized.Dispose()
$allowed=@{};Import-Csv 'C:/Paladin/handoffs/art-direction/sunlight-and-shadow/palette-64.csv' | ForEach-Object{$allowed[$_.Hex.ToUpper()]=$true}
$invalid=0;$partial=0;$transparent=0;$colors=@{}
for($y=0;$y -lt 228;$y++){for($x=0;$x -lt 112;$x++){$c=$output.GetPixel($x,$y);if($c.A -eq 0){$transparent++;continue};if($c.A -ne 255){$partial++};$hex='#{0:X2}{1:X2}{2:X2}' -f $c.R,$c.G,$c.B;$colors[$hex]=$true;if(-not $allowed.ContainsKey($hex)){$invalid++}}}
if($invalid -ne 0 -or $partial -ne 0){throw 'Roof export verification failed'}
$output.Save($targetPath,[System.Drawing.Imaging.ImageFormat]::Png);$output.Dispose()
$report=[pscustomobject]@{file=$targetPath;width=112;height=228;transparentPixels=$transparent;usedColors=$colors.Count;offPalettePixels=$invalid;partialAlphaPixels=$partial;fullSourceBounds=$bounds.ToString();paletteConstraint='Colors present in house-roof.png';colors=@($colors.Keys|Sort-Object)}
$report | ConvertTo-Json -Depth 4 | Set-Content 'C:/Paladin/handoffs/art-direction/environment-v4/keep-thatch-roof-verification.json'
$report | Format-List
