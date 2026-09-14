$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$taskRoot = $PSScriptRoot
$projectRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot '../../..'))
$destination = Join-Path $projectRoot 'assets/sprites/characters-v1'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Add-Type -ReferencedAssemblies System.Drawing.Common,System.Drawing.Primitives,System.ComponentModel.Primitives,System.Private.Windows.GdiPlus,System.Private.Windows.Core -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
public static class CharacterExport {
 public static bool Key(Color c) { return c.A < 128 || (c.R > 170 && c.B > 170 && c.G < 100); }
 public static int[] Export(string source, string output, int left, int top, int right, int bottom, int height, int[] palette) {
  using(var src = new Bitmap(source)) {
   int x0=right,y0=bottom,x1=left,y1=top;
   for(int y=top;y<bottom;y++) for(int x=left;x<right;x++) if(!Key(src.GetPixel(x,y))) { x0=Math.Min(x0,x); y0=Math.Min(y0,y); x1=Math.Max(x1,x); y1=Math.Max(y1,y); }
   if(x1<=x0 || y1<=y0) throw new Exception("Empty cell: "+output);
   if(x0<=left || y0<=top || x1>=right-1 || y1>=bottom-1) throw new Exception("Cell touches crop boundary: "+output);
   int width=Math.Min(14,(int)Math.Round((x1-x0+1.0)*height/(y1-y0+1.0)));
   using(var dst = new Bitmap(16,20,PixelFormat.Format32bppArgb)) {
    int ox=(16-width)/2, oy=18-height, count=0;
    for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
     int sx=x0+Math.Min(x1-x0,(int)((x+.5)*(x1-x0+1)/width));
     int sy=y0+Math.Min(y1-y0,(int)((y+.5)*(y1-y0+1)/height));
     Color c=src.GetPixel(sx,sy); if(Key(c)) continue;
     long best=long.MaxValue; int color=0;
     foreach(int p in palette) { int dr=c.R-((p>>16)&255),dg=c.G-((p>>8)&255),db=c.B-(p&255); long d=dr*dr+dg*dg+db*db; if(d<best){best=d;color=p;} }
     dst.SetPixel(ox+x,oy+y,Color.FromArgb(255,(color>>16)&255,(color>>8)&255,color&255));count++;
    }
    if(count<35) throw new Exception("Sparse character: "+output);
    dst.Save(output,ImageFormat.Png);
    return new int[]{x0,y0,x1+1,y1+1,width,height,count};
   }
  }
 }
 public static void Preview(string[] files,string path) {
  using(var dst=new Bitmap(1024, files.Length/4*140)) using(var g=Graphics.FromImage(dst)) {
   g.Clear(Color.FromArgb(35,87,71));
   using(var font=new Font("Consolas",10)) {
    for(int i=0;i<files.Length;i++) using(var src=new Bitmap(files[i])) {
     int ox=i%4*256,oy=i/4*140;
     for(int y=0;y<20;y++) for(int x=0;x<16;x++) { Color c=src.GetPixel(x,y); if(c.A>0) using(var b=new SolidBrush(c))g.FillRectangle(b,ox+80+x*5,oy+y*5,5,5); }
     g.DrawString(System.IO.Path.GetFileNameWithoutExtension(files[i]),font,Brushes.Wheat,ox+3,oy+112);
    }
   }
   dst.Save(path,ImageFormat.Png);
  }
 }
}
'@
$palette = [int[]](Get-Content (Join-Path $projectRoot 'config/art-palette.hex') | Where-Object { $_ -match '^#[0-9A-Fa-f]{6}$' } | ForEach-Object { [Convert]::ToInt32($_.Substring(1),16) })
$sets = @(
 @{file='citizens-a.png'; roles=@('citizen','farmer','fisher','logger','herder','baker'); rows=@(80,310,540,780,1010,1240,1480); height=@(11,12,13,11,12,11)},
 @{file='citizens-b.png'; roles=@('merchant','porter','builder','smith','herbalist','laborer'); rows=@(10,185,345,515,680,845,1060); height=@(11,11,12,11,11,12)},
 @{file='soldiers.png'; roles=@('militia','spearman','archer','swordsman','crossbowman','captain'); rows=@(20,260,510,760,990,1210,1500); height=@(11,14,12,13,12,14)}
)
$variants = @('male.front','male.back','female.front','female.back')
$records = @(); $catalog = @('# Paladin characters v1; 16x20 canvases, feet at pixel (8,18).')
foreach($set in $sets) {
 $source = Join-Path $taskRoot $set.file
 $bitmap = [Drawing.Bitmap]::new($source); $w=$bitmap.Width; $bitmap.Dispose()
 $files = @()
 for($row=0;$row -lt 6;$row++) { for($col=0;$col -lt 4;$col++) {
  $name = $set.roles[$row]+'.'+$variants[$col]
  $out = Join-Path $destination ($name+'.png')
  $bounds = [CharacterExport]::Export($source,$out,[int]($col*$w/4),$set.rows[$row],[int](($col+1)*$w/4),$set.rows[$row+1],$set.height[$row],$palette)
  $files += $out
  $catalog += "citizen.$name characters-v1/$name.png 1 1.25 0.5 0.9 0 0"
  $records += @{name=$name;source=$set.file;crop=$bounds[0..3];width=$bounds[4];height=$bounds[5];pixels=$bounds[6];sha256=(Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash}
 }}
 [CharacterExport]::Preview([string[]]$files,(Join-Path $taskRoot ($set.file.Replace('.png','-review.png'))))
}
$catalog | Set-Content (Join-Path $taskRoot 'characters.catalog')
$records | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $taskRoot 'exports.json')
Write-Output "Exported $($records.Count) palette-constrained sprites."


