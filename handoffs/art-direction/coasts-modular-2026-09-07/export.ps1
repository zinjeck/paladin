param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
# The existing exporter provides nearest-neighbour sampling, binary alpha and
# exact nearest-palette quantization. Do not alter the artist's source masters.
$helper=Get-Content "$Project/handoffs/art-direction/tribal-v10/tools/export.ps1" -Raw
Invoke-Expression ($helper.Substring($helper.IndexOf('Add-Type -AssemblyName'),$helper.IndexOf('$palette=@')-$helper.IndexOf('Add-Type -AssemblyName')))
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System.Drawing;
public static class ExportBounds {
 public static Rectangle Content(string path) {
  using(var b=new Bitmap(path)) {
   int l=b.Width,t=b.Height,r=0,d=0;
   for(int y=0;y<b.Height;y++)for(int x=0;x<b.Width;x++)if(b.GetPixel(x,y).A>=128){l=System.Math.Min(l,x);t=System.Math.Min(t,y);r=System.Math.Max(r,x);d=System.Math.Max(d,y);}
   return Rectangle.FromLTRB(l,t,r+1,d+1);
  }
 }
 public static void World(string source,string target,string[] palette) {
  // Integrate each source area before palette snapping: strategic materials
  // keep broad painted masses instead of aliasing tiny source flecks.
  var colors=System.Array.ConvertAll(palette,s=>ColorTranslator.FromHtml(s));
  using(var b=new Bitmap(source))using(var output=new Bitmap(32,32)) {
   for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
    long r=0,g=0,bl=0,n=0;
    for(int yy=y*b.Height/32;yy<(y+1)*b.Height/32;yy++)
     for(int xx=x*b.Width/32;xx<(x+1)*b.Width/32;xx++) {
      var c=b.GetPixel(xx,yy);r+=c.R;g+=c.G;bl+=c.B;n++;
     }
    int best=0;double distance=double.MaxValue;
    for(int i=0;i<colors.Length;i++) {
     double dr=r/(double)n-colors[i].R,dg=g/(double)n-colors[i].G,db=bl/(double)n-colors[i].B;
     double d=dr*dr*.3+dg*dg*.59+db*db*.11;
     if(d<distance){distance=d;best=i;}
    }
    output.SetPixel(x,y,Color.FromArgb(255,colors[best]));
   }
   output.Save(target,System.Drawing.Imaging.ImageFormat.Png);
  }
 }
}
'@
$master=@(Get-Content "$Project/config/art-palette.hex")
$source="$Project/handoffs/art-direction/coasts-modular-2026-09-07/source"
$target="$Project/assets/sprites/modular-v1"
$earth=@('#B78350','#C18B5A','#A78D72','#A99478','#886044','#74513F','#D9C79F','#7A5038','#633E4B','#49352F')
function Export-Asset($name,$file,$width,$height,$colors) {
 foreach($c in $colors){if($master -notcontains $c){throw "Off-palette material $c"}}
 $path="$source/$name.png"
 $box=[ExportBounds]::Content($path)
 [TribalExport]::Export($path,$file,$box.X,$box.Y,$box.Width,$box.Height,$width,$height,$colors,$false)
}
Export-Asset wall "$target/adobe-front.png" 48 14 $earth
Export-Asset roofDormer "$target/roof-dormer.png" 55 49 $earth
Export-Asset roofBound "$target/roof-bound.png" 55 49 $earth
[TribalExport]::Rotate("$target/roof-dormer.png","$target/roof-dormer-side.png")
[TribalExport]::Rotate("$target/roof-bound.png","$target/roof-bound-side.png")
Export-Asset bedClean "$target/bed-0.png" 12 15 ($earth+@('#F4E7C7','#A63545','#874D50','#392B3C'))
$cloth=@('#A63545','#3F5F9A','#6F4A7E','#4F8C7A','#B78350','#D9C79F','#874D50','#63BFC3')
for($i=1;$i -lt 8;$i++) {
 [TribalExport]::Remap("$target/bed-0.png","$target/bed-$i.png",@('#A63545','#D75056'),@($cloth[$i],$cloth[$i]))
}
Export-Asset keepFloor "$target/hall-floor.png" 48 112 $master
Export-Asset table "$target/communal-table.png" 32 16 $earth
$front="$Project/assets/sprites/tribal-v11/house-front.png"
$b=[Drawing.Bitmap]::new($front);$fw=$b.Width;$fh=$b.Height;$b.Dispose()
[TribalExport]::Export($front,"$target/timber-door.png",[int]($fw*.435),[int]($fh*.20),[int]($fw*.13),[int]($fh*.71),8,11,$master,$false)
$biomes=@{
 plain=@('#337A58','#49975B','#235747'); forest=@('#235747','#337A58');
 jungle=@('#193E42','#235747','#337A58'); taiga=@('#235747','#4F8C7A','#716D70');
 desert=@('#886044','#B78350','#A99478'); tundra=@('#716D70','#A78D72','#9AA7AF');
 water=@('#202C43','#30455D'); shallow=@('#30455D','#46627D','#193E42');
 beach=@('#A99478','#B78350','#886044')
}
foreach($name in $biomes.Keys) {
 if(Test-Path "$source/world-$name.png") {
  foreach($color in $biomes[$name]) {if($master -notcontains $color){throw "Off-palette world color $color"}}
  [ExportBounds]::World("$source/world-$name.png","$Project/assets/sprites/world-biomes/$name.png",$biomes[$name])
 }
}
