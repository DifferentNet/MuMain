# MU Moldova: draws the title plates' texture, src/bin/Data/Interface/newui_titles.OZT (512 x 512).
#
# One row per rarity (silver, ice, jade, gold, crimson, violet; GameLogic::Titles::Rarity), in the colours of the
# titles concept (ghinzuro, 2026-10-27):
#   - the ribbon every title wears, x 0-87, rows of 52 from y 0: its left end with the gem (48 wide), a middle
#     column the game stretches under the text (8), its right end (24); 48 high with a 4 px glow above and under;
#   - the banner of the gold and crimson titles, x 96-447, rows of 84 from y 0: the left wing and the right wing
#     (96 x 80 each), the frame's left end, middle and right end (32, 8, 32; 64 high), the gem over the frame
#     (28 x 28). Every rarity has one, so a catalogue entry could ask for it.
# UI/Character/TitlePlates.cpp reads the pieces at these places; change both together.
#
# Run from anywhere with Windows PowerShell 5.1 (System.Drawing):
#   powershell -ExecutionPolicy Bypass -File tools/make_title_plates.ps1 [-Out path] [-Png path]
param(
    [string]$Out = (Join-Path $PSScriptRoot '..\src\bin\Data\Interface\newui_titles.OZT'),
    [string]$Png = ''
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

public static class TitlePlates
{
    // hi, mid, lo, edge, glow (alpha in the glow).
    static readonly Color[][] Rarities =
    {
        new[] { C(0xf4f7fa), C(0xa9b3bf), C(0x3d4550), C(0xe3e9f0), Color.FromArgb(90, 225, 235, 245) },
        new[] { C(0xe2f6ff), C(0x5fb4e6), C(0x0c355c), C(0xa5e1ff), Color.FromArgb(128, 120, 200, 255) },
        new[] { C(0xdcffe2), C(0x3fbf68), C(0x0b4224), C(0x8dffb0), Color.FromArgb(102, 110, 255, 150) },
        new[] { C(0xfff4c8), C(0xe3aa2b), C(0x5e3a05), C(0xffd23c), Color.FromArgb(140, 255, 200, 60) },
        new[] { C(0xffe0d6), C(0xe2442f), C(0x520b07), C(0xff8466), Color.FromArgb(140, 255, 90, 60) },
        new[] { C(0xf1e3ff), C(0xa26ae6), C(0x33115e), C(0xd1a8ff), Color.FromArgb(128, 190, 140, 255) },
    };

    public const int Size = 512;
    public const int RibbonRow = 52, RibbonHeight = 48, RibbonLeft = 48, RibbonMiddle = 8, RibbonRight = 24;
    public const int BannerX = 96, BannerRow = 84, WingWidth = 96, WingHeight = 80;
    public const int FrameEnd = 32, FrameMiddle = 8, FrameHeight = 64, GemSize = 28;

    static Color C(int rgb) { return Color.FromArgb(255, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
    static Color A(Color c, int alpha) { return Color.FromArgb(alpha, c.R, c.G, c.B); }

    public static Bitmap Draw()
    {
        var atlas = new Bitmap(Size, Size, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(atlas))
        {
            g.Clear(Color.Transparent);
            g.CompositingMode = CompositingMode.SourceCopy;
            for (int r = 0; r < Rarities.Length; ++r)
            {
                var c = Rarities[r];
                using (var ribbon = Ribbon(c))
                {
                    int y = r * RibbonRow;
                    Put(g, ribbon, 0, 0, RibbonLeft, RibbonHeight, 0, y);
                    Put(g, ribbon, ribbon.Width / 2 - RibbonMiddle / 2, 0, RibbonMiddle, RibbonHeight, RibbonLeft + 4, y);
                    Put(g, ribbon, ribbon.Width - RibbonRight, 0, RibbonRight, RibbonHeight, RibbonLeft + 4 + RibbonMiddle + 4, y);
                }

                int by = r * BannerRow;
                using (var wing = Wing(c))
                {
                    Put(g, wing, 0, 0, WingWidth, WingHeight, BannerX, by);
                    wing.RotateFlip(RotateFlipType.RotateNoneFlipX);
                    Put(g, wing, 0, 0, WingWidth, WingHeight, BannerX + WingWidth + 4, by);
                }
                using (var frame = Frame(c))
                {
                    int fx = BannerX + 2 * (WingWidth + 4);
                    Put(g, frame, 0, 0, FrameEnd, FrameHeight, fx, by);
                    Put(g, frame, frame.Width / 2 - FrameMiddle / 2, 0, FrameMiddle, FrameHeight, fx + FrameEnd + 4, by);
                    Put(g, frame, frame.Width - FrameEnd, 0, FrameEnd, FrameHeight, fx + FrameEnd + 4 + FrameMiddle + 4, by);
                    using (var gem = Gem(c))
                        Put(g, gem, 0, 0, GemSize, GemSize, fx + 2 * FrameEnd + FrameMiddle + 12, by);
                }
            }
        }
        return atlas;
    }

    static void Put(Graphics g, Bitmap from, int sx, int sy, int w, int h, int dx, int dy)
    {
        g.DrawImage(from, new Rectangle(dx, dy, w, h), new Rectangle(sx, sy, w, h), GraphicsUnit.Pixel);
    }

    static Graphics Smooth(Bitmap b)
    {
        var g = Graphics.FromImage(b);
        g.Clear(Color.Transparent);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        return g;
    }

    // A long hexagon: points at both ends, flat top and bottom.
    static GraphicsPath Hexagon(float x, float y, float w, float h, float point)
    {
        var path = new GraphicsPath();
        path.AddPolygon(new[]
        {
            new PointF(x, y + h / 2), new PointF(x + point, y), new PointF(x + w - point, y),
            new PointF(x + w, y + h / 2), new PointF(x + w - point, y + h), new PointF(x + point, y + h),
        });
        return path;
    }

    static void Glow(Graphics g, GraphicsPath path, Color glow, float reach)
    {
        // A soft halo: wide faint strokes under narrower stronger ones.
        for (int i = 4; i >= 1; --i)
        {
            using (var pen = new Pen(A(glow, glow.A * (5 - i) / 10), reach * i / 2f))
            {
                pen.LineJoin = LineJoin.Round;
                g.DrawPath(pen, path);
            }
        }
    }

    static Bitmap Ribbon(Color[] c)
    {
        const int w = 160;
        var b = new Bitmap(w, RibbonHeight, PixelFormat.Format32bppArgb);
        using (var g = Smooth(b))
        {
            float top = 5, h = RibbonHeight - 10, point = 19;
            using (var outer = Hexagon(1.5f, top, w - 3, h, point))
            {
                Glow(g, outer, c[4], 3.5f);
                using (var edge = new SolidBrush(c[3])) g.FillPath(edge, outer);
            }
            using (var inner = Hexagon(4.5f, top + 2.5f, w - 9, h - 5, point - 2.5f))
            using (var fill = new LinearGradientBrush(new PointF(0, top + 2), new PointF(0, top + h - 2), c[1], Color.Black))
            {
                var blend = new ColorBlend();
                blend.Colors = new[] { c[1], c[2], C(0x07090c) };
                blend.Positions = new[] { 0f, 0.62f, 1f };
                fill.InterpolationColors = blend;
                fill.WrapMode = WrapMode.TileFlipXY;
                g.FillPath(fill, inner);
            }
            // A thin light line under the top edge.
            using (var shine = new Pen(A(c[0], 70), 1.2f))
                g.DrawLine(shine, point + 3, top + 4, w - point - 3, top + 4);
            DrawGem(g, point + 11, RibbonHeight / 2f, 8.5f, c, false);
        }
        return b;
    }

    static Bitmap Frame(Color[] c)
    {
        const int w = 200;
        var b = new Bitmap(w, FrameHeight, PixelFormat.Format32bppArgb);
        using (var g = Smooth(b))
        {
            float top = 7, h = FrameHeight - 14, point = 22;
            using (var outer = Hexagon(2, top, w - 4, h, point))
            {
                Glow(g, outer, c[4], 4.5f);
                using (var frame = new LinearGradientBrush(new PointF(0, top), new PointF(0, top + h), c[0], c[1]))
                {
                    var blend = new ColorBlend();
                    blend.Colors = new[] { c[0], c[3], c[1] };
                    blend.Positions = new[] { 0f, 0.4f, 1f };
                    frame.InterpolationColors = blend;
                    frame.WrapMode = WrapMode.TileFlipXY;
                    g.FillPath(frame, outer);
                }
            }
            using (var core = Hexagon(6.5f, top + 4.5f, w - 13, h - 9, point - 4))
            {
                using (var fill = new LinearGradientBrush(new PointF(0, top + 4), new PointF(0, top + h - 4), c[2], C(0x07080b)))
                {
                    fill.WrapMode = WrapMode.TileFlipXY;
                    g.FillPath(fill, core);
                }
                // The light from above, the core's top half.
                using (var light = new LinearGradientBrush(new PointF(0, top), new PointF(0, top + h), A(c[1], 150), A(c[1], 0)))
                {
                    var fade = new ColorBlend();
                    fade.Colors = new[] { A(c[1], 150), A(c[1], 0), A(c[1], 0) };
                    fade.Positions = new[] { 0f, 0.6f, 1f };
                    light.InterpolationColors = fade;
                    light.WrapMode = WrapMode.TileFlipXY;
                    g.FillPath(light, core);
                }
                using (var line = new Pen(A(c[2], 255), 1.2f))
                    g.DrawPath(line, core);
            }
        }
        return b;
    }

    static Bitmap Gem(Color[] c)
    {
        var b = new Bitmap(GemSize, GemSize, PixelFormat.Format32bppArgb);
        using (var g = Smooth(b))
            DrawGem(g, GemSize / 2f, GemSize / 2f, 10f, c, true);
        return b;
    }

    // A diamond lit from its top left.
    static void DrawGem(Graphics g, float cx, float cy, float r, Color[] c, bool border)
    {
        var points = new[] { new PointF(cx, cy - r), new PointF(cx + r, cy), new PointF(cx, cy + r), new PointF(cx - r, cy) };
        using (var path = new GraphicsPath())
        {
            path.AddPolygon(points);
            Glow(g, path, c[4], 2.5f);
            using (var brush = new PathGradientBrush(path))
            {
                brush.CenterPoint = new PointF(cx - r * 0.3f, cy - r * 0.3f);
                var blend = new ColorBlend();
                blend.Colors = new[] { c[2], c[1], border ? c[3] : c[0], Color.White };
                blend.Positions = new[] { 0f, 0.4f, 0.75f, 1f };
                brush.InterpolationColors = blend;
                g.FillPath(brush, path);
            }
            using (var pen = new Pen(border ? c[2] : A(c[2], 200), border ? 1.4f : 1f))
                g.DrawPath(pen, path);
        }
    }

    // Five feathers fanned from one root on the right, like the concept's SVG wing
    // (M58 21 C 42 16, 20 9, 2 6 C 14 13, 32 20, 58 26 Z in the box -4 -14 66 60).
    static Bitmap Wing(Color[] c)
    {
        var b = new Bitmap(WingWidth, WingHeight, PixelFormat.Format32bppArgb);
        using (var g = Smooth(b))
        {
            float scale = Math.Min((WingWidth - 8) / 66f, (WingHeight - 2) / 60f);
            float ox = (WingWidth - 66 * scale) / 2f + 4 * scale, oy = (WingHeight - 60 * scale) / 2f + 14 * scale;
            float[] angles = { -34, -18, -3, 12, 26 };
            using (var halo = new GraphicsPath())
            {
                for (int i = 0; i < angles.Length; ++i)
                    halo.AddPath(Feather(angles[i], 1 - i * 0.1f, scale, ox, oy), false);
                using (var pen = new Pen(A(c[4], c[4].A / 2), 5f)) { pen.LineJoin = LineJoin.Round; g.DrawPath(pen, halo); }
            }
            for (int i = angles.Length - 1; i >= 0; --i)
            {
                using (var feather = Feather(angles[i], 1 - i * 0.1f, scale, ox, oy))
                {
                    var bounds = feather.GetBounds();
                    int alpha = (int)(255 * (1 - i * 0.08f));
                    using (var fill = new LinearGradientBrush(new RectangleF(bounds.X - 1, bounds.Y - 1, bounds.Width + 2, bounds.Height + 2), c[2], c[0], LinearGradientMode.Horizontal))
                    {
                        var blend = new ColorBlend();
                        blend.Colors = new[] { A(c[2], alpha), A(c[1], alpha), A(c[0], alpha) };
                        blend.Positions = new[] { 0f, 0.6f, 1f };
                        fill.InterpolationColors = blend;
                        g.FillPath(fill, feather);
                    }
                    using (var pen = new Pen(A(c[2], 160), 0.8f))
                        g.DrawPath(pen, feather);
                }
            }
        }
        return b;
    }

    static GraphicsPath Feather(float angle, float s, float scale, float ox, float oy)
    {
        var path = new GraphicsPath();
        path.AddBezier(58, 21, 42, 16, 20, 9, 2, 6);
        path.AddBezier(2, 6, 14, 13, 32, 20, 58, 26);
        path.CloseFigure();
        // SVG: rotate(a 58 23) translate(58 - 58 s, 0) scale(s, 1), then the box into the bitmap.
        var m = new Matrix();
        m.Translate(ox, oy, MatrixOrder.Append);
        m.Scale(scale, scale, MatrixOrder.Prepend);
        m.RotateAt(angle, new PointF(58, 23), MatrixOrder.Prepend);
        m.Translate(58 - 58 * s, 0, MatrixOrder.Prepend);
        m.Scale(s, 1, MatrixOrder.Prepend);
        path.Transform(m);
        return path;
    }

    // The game's OZT: 4 bytes, an 18 byte TGA header (uncompressed, 32 bit, bottom row first), BGRA pixels.
    public static void SaveOzt(Bitmap bitmap, string path)
    {
        int w = bitmap.Width, h = bitmap.Height;
        var data = bitmap.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var pixels = new byte[w * h * 4];
        try
        {
            for (int y = 0; y < h; ++y)
                System.Runtime.InteropServices.Marshal.Copy(data.Scan0 + y * data.Stride, pixels, y * w * 4, w * 4);
        }
        finally { bitmap.UnlockBits(data); }

        using (var file = new BinaryWriter(File.Create(path)))
        {
            file.Write(new byte[] { 0, 0, 2, 0 });
            file.Write(new byte[] { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
            file.Write((ushort)w);
            file.Write((ushort)h);
            file.Write((byte)32);
            file.Write((byte)8);
            for (int y = h - 1; y >= 0; --y)
                file.Write(pixels, y * w * 4, w * 4); // Format32bppArgb is BGRA in memory.
        }
    }
}
'@

$atlas = [TitlePlates]::Draw()
try {
    $full = [System.IO.Path]::GetFullPath($Out)
    [TitlePlates]::SaveOzt($atlas, $full)
    Write-Output "Wrote $full"
    if ($Png) {
        $atlas.Save([System.IO.Path]::GetFullPath($Png), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Output "Wrote $Png"
    }
}
finally {
    $atlas.Dispose()
}
