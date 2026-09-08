param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
public static class TribalExport {
    public static void Export(string source,string target,int x,int y,int w,int h,int tw,int th,string[] palette,bool key) {
        Color[] colors=Array.ConvertAll(palette,s=>ColorTranslator.FromHtml(s));
        using(var image=new Bitmap(source))using(var output=new Bitmap(tw,th,PixelFormat.Format32bppArgb)) {
            for(int yy=0;yy<th;yy++)for(int xx=0;xx<tw;xx++) {
                var c=image.GetPixel(x+(int)((xx+.5)*w/tw),y+(int)((yy+.5)*h/th));
                int spread=Math.Max(c.R,Math.Max(c.G,c.B))-Math.Min(c.R,Math.Min(c.G,c.B));
                if(c.A<128 || (key && ((c.R>180 && c.B>180 && c.G<90) || (spread<28 && c.R>140 && c.G>140 && c.B>140)))) {output.SetPixel(xx,yy,Color.Transparent);continue;}
                int best=0;double distance=Double.MaxValue;
                for(int i=0;i<colors.Length;i++){double dr=c.R-colors[i].R,dg=c.G-colors[i].G,db=c.B-colors[i].B;double d=dr*dr*.3+dg*dg*.59+db*db*.11;if(d<distance){distance=d;best=i;}}
                output.SetPixel(xx,yy,Color.FromArgb(255,colors[best]));
            }
            output.Save(target,ImageFormat.Png);
        }
    }
    public static void Rotate(string source,string target) {using(var b=new Bitmap(source)){b.RotateFlip(RotateFlipType.Rotate90FlipNone);b.Save(target,ImageFormat.Png);}}
    public static void Remap(string source,string target,string[] from,string[] to) {
        using(var b=new Bitmap(source)){for(int y=0;y<b.Height;y++)for(int x=0;x<b.Width;x++){var c=b.GetPixel(x,y);for(int i=0;i<from.Length;i++)if(c.ToArgb()==ColorTranslator.FromHtml(from[i]).ToArgb()){b.SetPixel(x,y,ColorTranslator.FromHtml(to[i]));break;}}b.Save(target,ImageFormat.Png);}
    }
    public static void CalmGround(string path) {
        using(var b=new Bitmap(path)){
            using(var copy=new Bitmap(b))for(int y=0;y<b.Height;y++)for(int x=0;x<b.Width;x++){
                // A material filter merges tiny flecks into broad grass patches;
                // keep the same output grid, without enlarging individual pixels.
                int r=0,g=0,bl=0,n=0;
                for(int yy=-3;yy<=3;yy++)for(int xx=-3;xx<=3;xx++){var c=copy.GetPixel((x+xx+b.Width)%b.Width,(y+yy+b.Height)%b.Height);r+=c.R;g+=c.G;bl+=c.B;n++;}
                var dark=ColorTranslator.FromHtml("#49975B");var mid=ColorTranslator.FromHtml("#79B56D");var light=ColorTranslator.FromHtml("#A6CD59");
                b.SetPixel(x,y,g/n<171?dark:r/n>132?light:mid);
            }
            b.Save(path+".tmp.png",ImageFormat.Png);
        }
        File.Copy(path+".tmp.png",path,true);File.Delete(path+".tmp.png");
    }
}
'@
$palette=@(Get-Content -LiteralPath "$Project/config/art-palette.hex" | Where-Object {$_ -match '^#[0-9A-Fa-f]{6}$'})
$earth=@('#BD864C','#D5A454','#886044','#7A5038','#633E4B','#49352F','#A99478','#D9C79F','#392B3C')
foreach($color in $earth) {if($palette -notcontains $color) {throw "Material color missing from master palette: $color"}}
$source="$Project/handoffs/art-direction/tribal-v10/source/tribal-clean-atlas.png"
$dest="$Project/assets/sprites/tribal-v10"
# Crop the generated atlas and produce crisp, binary-alpha, exact-palette exports.
[TribalExport]::Export($source,"$dest/roof.png",59,103,515,452,96,88,$earth,$true)
[TribalExport]::Rotate("$dest/roof.png","$dest/roof-side.png")
[TribalExport]::Export($source,"$dest/front.png",685,217,510,270,96,28,$earth,$true)
[TribalExport]::Export($source,"$dest/back.png",58,844,511,270,96,28,$earth,$true)
[TribalExport]::Export($source,"$dest/mud.png",685,844,510,270,32,32,$earth,$false)
$grass="$Project/handoffs/art-direction/tribal-v10/source/grass.png"
if(Test-Path -LiteralPath $grass) {
    $g=[System.Drawing.Bitmap]::new($grass)
    $gw=$g.Width;$gh=$g.Height;$g.Dispose()
    $greens=@('#235747','#337A58','#49975B','#79B56D','#A6CD59','#D0E58A')
    [TribalExport]::Export($grass,"$dest/grass.png",0,0,$gw,$gh,32,32,$greens,$false)
    [TribalExport]::CalmGround("$dest/grass.png")
    [TribalExport]::Remap("$dest/grass.png","$dest/grass-cold.png",@('#D0E58A','#A6CD59','#79B56D'),@('#A6CD59','#79B56D','#49975B'))
    [TribalExport]::Remap("$dest/grass.png","$dest/grass-warm.png",@('#49975B','#79B56D','#A6CD59'),@('#79B56D','#A6CD59','#D0E58A'))
    [TribalExport]::Remap("$dest/grass.png","$dest/grass-jungle.png",@('#D0E58A','#A6CD59','#79B56D'),@('#A6CD59','#79B56D','#49975B'))
}
