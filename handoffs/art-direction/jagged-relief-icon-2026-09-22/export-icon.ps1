$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

public static class PaladinHelmetExporter
{
    static Color Nearest(Color c, Color[] palette)
    {
        int best = int.MaxValue;
        Color result = palette[0];
        foreach (Color p in palette)
        {
            int r = c.R-p.R, g = c.G-p.G, b = c.B-p.B;
            int distance = r*r + g*g + b*b;
            if (distance < best) { best = distance; result = p; }
        }
        return result;
    }

    static byte[] IconImage(Bitmap image)
    {
        using (var stream = new MemoryStream())
        {
            if (image.Width == 256)
            {
                image.Save(stream, ImageFormat.Png);
                return stream.ToArray();
            }
            int size = image.Width, maskStride = ((size+31)/32)*4;
            using (var writer = new BinaryWriter(stream))
            {
                writer.Write(40); writer.Write(size); writer.Write(size*2);
                writer.Write((ushort)1); writer.Write((ushort)32);
                writer.Write(0); writer.Write(size*size*4);
                writer.Write(0); writer.Write(0); writer.Write(0); writer.Write(0);
                for (int y=size-1; y>=0; --y)
                for (int x=0; x<size; ++x)
                {
                    Color c=image.GetPixel(x,y);
                    writer.Write(c.B); writer.Write(c.G); writer.Write(c.R); writer.Write(c.A);
                }
                for (int y=size-1; y>=0; --y)
                {
                    var row=new byte[maskStride];
                    for (int x=0; x<size; ++x)
                        if (image.GetPixel(x,y).A==0) row[x/8] |= (byte)(128>>(x%8));
                    writer.Write(row);
                }
                return stream.ToArray();
            }
        }
    }

    public static void Export(string source, string paletteFile, string iconFile, string review)
    {
        var colors=new List<Color>();
        foreach (string line in File.ReadAllLines(paletteFile))
            if (line.Trim().Length==7 && line.Trim()[0]=='#')
                colors.Add(ColorTranslator.FromHtml(line.Trim()));
        if (colors.Count!=64) throw new InvalidDataException("Expected the approved 64-color palette.");
        Color[] palette=colors.ToArray();
        using (var original=new Bitmap(source))
        using (var cutout=new Bitmap(original.Width,original.Height,PixelFormat.Format32bppArgb))
        {
            int w=original.Width, h=original.Height;
            var outside=new bool[w*h];
            var pending=new Queue<int>();
            Action<int,int> visit=(x,y)=> {
                if (x<0 || x>=w || y<0 || y>=h || outside[y*w+x]) return;
                Color c=original.GetPixel(x,y);
                int high=Math.Max(c.R,Math.Max(c.G,c.B)), low=Math.Min(c.R,Math.Min(c.G,c.B));
                // Only the edge-connected near-neutral matte is removed. Dark
                // blue steel, enclosed visor shadows and gold outlines survive.
                if (c.A!=0 && (high>20 || high-low>8)) return;
                outside[y*w+x]=true; pending.Enqueue(y*w+x);
            };
            for (int x=0;x<w;++x) { visit(x,0); visit(x,h-1); }
            for (int y=0;y<h;++y) { visit(0,y); visit(w-1,y); }
            while (pending.Count!=0)
            {
                int p=pending.Dequeue(), x=p%w, y=p/w;
                visit(x-1,y); visit(x+1,y); visit(x,y-1); visit(x,y+1);
            }
            int left=w, top=h, right=-1, bottom=-1;
            for (int y=0;y<h;++y)
            for (int x=0;x<w;++x)
            {
                if (outside[y*w+x] || original.GetPixel(x,y).A==0) continue;
                cutout.SetPixel(x,y,Nearest(original.GetPixel(x,y),palette));
                left=Math.Min(left,x); right=Math.Max(right,x);
                top=Math.Min(top,y); bottom=Math.Max(bottom,y);
            }
            if (right<left || bottom<top) throw new InvalidDataException("Empty helmet cutout.");
            var crop=new Rectangle(left,top,right-left+1,bottom-top+1);
            cutout.Save(Path.Combine(review,"helmet-cutout.png"),ImageFormat.Png);
            int[] sizes={16,20,24,32,40,48,64,96,128,256};
            var payloads=new List<byte[]>();
            using (var board=new Bitmap(640,352,PixelFormat.Format32bppArgb))
            using (var paint=Graphics.FromImage(board))
            {
                paint.Clear(Color.FromArgb(32,44,67));
                paint.FillRectangle(new SolidBrush(Color.FromArgb(215,224,227)),0,176,640,176);
                int slot=0;
                foreach (int size in sizes)
                using (var frame=new Bitmap(size,size,PixelFormat.Format32bppArgb))
                {
                    int margin=Math.Max(1,(int)Math.Round(size*.025));
                    double scale=(size-2.0*margin)/Math.Max(crop.Width,crop.Height);
                    int fw=Math.Max(1,(int)Math.Round(crop.Width*scale));
                    int fh=Math.Max(1,(int)Math.Round(crop.Height*scale));
                    using (var g=Graphics.FromImage(frame))
                    {
                        g.CompositingMode=CompositingMode.SourceCopy;
                        g.InterpolationMode=size<64 ? InterpolationMode.HighQualityBicubic : InterpolationMode.NearestNeighbor;
                        g.PixelOffsetMode=PixelOffsetMode.Half;
                        g.DrawImage(cutout,new Rectangle((size-fw)/2,(size-fh)/2,fw,fh),crop,GraphicsUnit.Pixel);
                    }
                    for (int y=0;y<size;++y)
                    for (int x=0;x<size;++x)
                    {
                        Color c=frame.GetPixel(x,y);
                        frame.SetPixel(x,y,c.A<96 ? Color.Transparent : Nearest(c,palette));
                    }
                    payloads.Add(IconImage(frame));
                    frame.Save(Path.Combine(review,"helmet-"+size+".png"),ImageFormat.Png);
                    if (size<=128)
                    {
                        paint.DrawImageUnscaled(frame,slot+(size<48 ? 6 : 0),20);
                        paint.DrawImageUnscaled(frame,slot+(size<48 ? 6 : 0),196);
                        slot+=Math.Max(40,size+12);
                    }
                    Console.WriteLine("Helmet {0}: transparent margin {1}px; occupied frame {2}x{3}",size,margin,fw,fh);
                }
                board.Save(Path.Combine(review,"icon-native-size-review.png"),ImageFormat.Png);
            }
            using (var writer=new BinaryWriter(File.Create(iconFile)))
            {
                writer.Write((ushort)0); writer.Write((ushort)1); writer.Write((ushort)sizes.Length);
                int offset=6+sizes.Length*16;
                for (int i=0;i<sizes.Length;++i)
                {
                    writer.Write((byte)(sizes[i]%256)); writer.Write((byte)(sizes[i]%256));
                    writer.Write((byte)0); writer.Write((byte)0);
                    writer.Write((ushort)1); writer.Write((ushort)32);
                    writer.Write(payloads[i].Length); writer.Write(offset);
                    offset+=payloads[i].Length;
                }
                foreach (byte[] bytes in payloads) writer.Write(bytes);
            }
            Console.WriteLine("Removed exterior matte; preserved dark internal detail. Original helmet bounds: {0}",crop);
        }
    }
}
'@
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
[PaladinHelmetExporter]::Export(
    (Join-Path $PSScriptRoot 'icon-original-256.png'),
    (Join-Path $root 'config/art-palette.hex'),
    (Join-Path $root 'assets/icons/paladin.ico'),
    $PSScriptRoot)
