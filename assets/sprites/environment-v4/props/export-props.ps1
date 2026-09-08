$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies @('System.Drawing.Common','System.Drawing.Primitives','System.Private.Windows.GdiPlus','System.Private.Windows.Core','System.Collections') -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;
public static class PropExport {
 public static string Export(string source,string target,int width,int height,string[] colors) {
   using(var src = new Bitmap(source)) using(var dst = new Bitmap(width,height,PixelFormat.Format32bppArgb)) {
     var pal = new Color[colors.Length]; for(int i=0;i<colors.Length;i++) pal[i] = ColorTranslator.FromHtml(colors[i]);
     int l=src.Width,t=src.Height,r=0,b=0;
     for(int y=0;y<src.Height;y++) for(int x=0;x<src.Width;x++) if(src.GetPixel(x,y).A>=160) {l=Math.Min(l,x);r=Math.Max(r,x);t=Math.Min(t,y);b=Math.Max(b,y);}
     double scale=Math.Min((width-2.0)/(r-l+1),(height-2.0)/(b-t+1));
     int dw=(int)Math.Round((r-l+1)*scale),dh=(int)Math.Round((b-t+1)*scale),ox=(width-dw)/2,oy=(height-dh)/2;
     var used = new HashSet<int>(); int opaque=0;
     for(int y=0;y<dh;y++) for(int x=0;x<dw;x++) {
       int sx=Math.Min(r,l+(int)((x+.5)*(r-l+1)/dw)),sy=Math.Min(b,t+(int)((y+.5)*(b-t+1)/dh));
       var c=src.GetPixel(sx,sy);if(c.A<160)continue;
       int best=0;double bestD=double.MaxValue;
       for(int i=0;i<pal.Length;i++){double dr=c.R-pal[i].R,dg=c.G-pal[i].G,db=c.B-pal[i].B,d=dr*dr+dg*dg+db*db;if(d<bestD){bestD=d;best=i;}}
       dst.SetPixel(x+ox,y+oy,pal[best]);used.Add(pal[best].ToArgb());opaque++;
     }
     dst.Save(target,ImageFormat.Png);
     return width+"x"+height+", opaque="+opaque+", colors="+used.Count+", alpha=0/255; source bbox="+l+","+t+","+(r-l+1)+","+(b-t+1);
   }
 }
}
'@
$propRoot = Split-Path -Parent $PSCommandPath
$sources = Get-Content -Raw -LiteralPath (Join-Path $propRoot 'props-generation.json') | ConvertFrom-Json
$approved = Import-Csv -LiteralPath 'C:/Paladin/handoffs/art-direction/sunlight-and-shadow/palette-64.csv'
$settings = @{
 'market-stall' = @{w=64;h=64;ids=@(1,2,6,7,8,9,10,12,17,19,24,25,26,27,28,29,33,34,35,37,39,45,53,54,55,58,64)}
 'stockpile-stack' = @{w=48;h=40;ids=@(1,2,6,10,17,18,19,24,29,33,34,35,37,39,51,52,53,54,55,58)}
 'fishing-station' = @{w=48;h=64;ids=@(1,2,6,8,9,10,17,18,19,24,27,28,33,34,35,37,39,45,52,53,54,55,58,64)}
 'wheat-crop' = @{w=48;h=40;ids=@(1,2,7,8,9,17,18,19,25,26,27,28,33)}
}
$report = foreach($entry in $sources){
 $setting=$settings[$entry.name]
 $colors=@($approved | Where-Object {[int]$_.Index -in $setting.ids} | ForEach-Object {$_.Hex})
 $target=Join-Path $propRoot ($entry.name+'.png')
 $result=[PropExport]::Export($entry.source,$target,$setting.w,$setting.h,[string[]]$colors)
 "$($entry.name): $result"
}
$report | Set-Content -LiteralPath (Join-Path $propRoot 'props-validation.txt')
$report
