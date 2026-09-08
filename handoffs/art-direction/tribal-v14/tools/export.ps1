param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
$helper=Get-Content "$Project/handoffs/art-direction/tribal-v10/tools/export.ps1" -Raw
Invoke-Expression ($helper.Substring($helper.IndexOf('Add-Type -AssemblyName'),$helper.IndexOf('$palette=@')-$helper.IndexOf('Add-Type -AssemblyName')))
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System; using System.Drawing; using System.Drawing.Imaging; using System.Collections.Generic;
public static class AtlasCutout {
 public static void Extract(string source,string target,int cx,int cy) {
  using(var src=new Bitmap(source)) using(var b=new Bitmap(512,512,PixelFormat.Format32bppArgb)) {
   // Authored silhouettes cross nominal cell boundaries. Crop the reviewed
   // object bounds, not an assumed grid that imports neighboring foliage.
   var boxes=new Rectangle[]{new Rectangle(48,130,496,286),new Rectangle(620,132,364,324),new Rectangle(1040,80,440,410),new Rectangle(48,550,500,390),new Rectangle(600,615,360,335),new Rectangle(1090,570,350,360)};
   var box=boxes[cy*3+cx];
   for(int y=0;y<box.Height;y++)for(int x=0;x<box.Width;x++)b.SetPixel(x,y,src.GetPixel(box.X+x,box.Y+y));
   var seen=new bool[512*512]; var q=new Queue<int>();
   for(int i=0;i<512;i++){q.Enqueue(i);q.Enqueue(511*512+i);q.Enqueue(i*512);q.Enqueue(i*512+511);}
   while(q.Count>0){int i=q.Dequeue();if(seen[i])continue;seen[i]=true;int x=i%512,y=i/512;var c=b.GetPixel(x,y);
    int hi=Math.Max(c.R,Math.Max(c.G,c.B)),lo=Math.Min(c.R,Math.Min(c.G,c.B));
    if(c.A!=0 && !(lo>220 && hi-lo<15))continue;
    b.SetPixel(x,y,Color.Transparent);if(x>0)q.Enqueue(i-1);if(x<511)q.Enqueue(i+1);if(y>0)q.Enqueue(i-512);if(y<511)q.Enqueue(i+512);
   }
   int l=511,t=511,r=0,d=0;
   for(int y=0;y<512;y++)for(int x=0;x<512;x++)if(b.GetPixel(x,y).A>0){l=Math.Min(l,x);t=Math.Min(t,y);r=Math.Max(r,x);d=Math.Max(d,y);}
   using(var crop=b.Clone(Rectangle.FromLTRB(Math.Max(0,l-2),Math.Max(0,t-2),Math.Min(512,r+3),Math.Min(512,d+3)),PixelFormat.Format32bppArgb))crop.Save(target,ImageFormat.Png);
  }
 }
}
'@
$dest="$Project/assets/sprites/tribal-v14"
$source="$Project/handoffs/art-direction/tribal-v14/source"
New-Item -ItemType Directory -Force $dest,$source | Out-Null
$palette=@(Get-Content "$Project/config/art-palette.hex" | Where-Object {$_ -match '^#[0-9A-Fa-f]{6}$'})
$names=@('fence','meat','fish','rock-a','rock-b','crate')
for($i=0;$i -lt 6;$i++) {
 $cut="$source/$($names[$i])-cutout.png"
 [AtlasCutout]::Extract("$source/atlas.png",$cut,($i%3),[int][Math]::Floor($i/3))
 $b=[Drawing.Bitmap]::new($cut);$w=$b.Width;$h=$b.Height;$b.Dispose()
 $th=if($i -eq 0){18}elseif($i -eq 5){24}else{28}
 [TribalExport]::Export($cut,"$dest/$($names[$i]).png",0,0,$w,$h,32,$th,$palette,$false)
}
