# MU Moldova: packs the title plates' art into src/bin/Data/Interface/newui_titles.OZT and writes where each plate
# lies in it to src/source/UI/Character/TitlePlatesArt.h (UI/Character/TitlePlates.cpp reads both).
#
# The art: six drawn plates, one per rarity, by ghinzuro and his wife (2026-10-29, "titles.zip"); the five ranks get
# ever richer plates and the unique violet titles (tester, streamer) the laurels. The originals are large (up to
# 2452 x 439) and are kept outside this public repository, in E:\Mu Online Server\art\title-plates-2026-10-29:
#   silver 5.png, ice 4.png, jade 3.png, gold 2.png, crimson 1.png, violet "6 premium.png".
#
# The field of every plate (the coloured rectangle the text sits on) is drawn the same size, about 1420 x 185 of the
# originals' pixels, and the game shows each plate whole, never squeezed or stretched (his wife, 2026-10-29: the
# central rectangles must be the same size on all six plates, nothing may be squeezed or distorted). So all
# plates are scaled by the same PlateScale and stacked one under another in a 1024 x 1024 atlas. The file is marked as
# twice its original size (MUX 2, see Render/Textures/UpscaleMarker.h), so the game builds its mipmaps and the plates
# stay smooth when drawn small.
#
# The numbers below are each field's edges in the original's pixels (left, top, right, bottom), measured inside the
# frame's bevel: across the middle row for left and right, down a column at 30 % of the width for top and bottom.
#
# The plates' effects (ghinzuro, 2026-11-03, variant D) come from a second atlas, newui_titles_fx.OZT (1024 x 512),
# baked here from the same art: each plate's silhouette at half the plates' atlas size (the running shine), its
# blurred silhouette at a quarter with a margin (the glow around the edge), a sparkle star and an ember dot. All of it
# white, its shape in the alpha, to be added (BlendMode::Additive natively; in RmlUi, which premultiplies it on load,
# with a zero vertex alpha) and tinted by the vertex colour. The gems drawn on each plate (where the sparkles go) are
# listed per plate below, as fractions of the plate's width and height.
#
# Run from anywhere with Windows PowerShell 5.1 (System.Drawing):
#   powershell -ExecutionPolicy Bypass -File tools/make_title_plates.ps1 [-Src folder] [-Out ozt] [-FxOut ozt] [-Header h] [-Png png] [-Json json]
param(
    [string]$Src = 'E:\Mu Online Server\art\title-plates-2026-10-29',
    [string]$Out = (Join-Path $PSScriptRoot '..\src\bin\Data\Interface\newui_titles.OZT'),
    [string]$FxOut = (Join-Path $PSScriptRoot '..\src\bin\Data\Interface\newui_titles_fx.OZT'),
    [string]$Header = (Join-Path $PSScriptRoot '..\src\source\UI\Character\TitlePlatesArt.h'),
    [string]$Png = '',
    [string]$Json = ''
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Text;

public class PlateSpec
{
    public string Rarity, File;
    public int FieldLeft, FieldTop, FieldRight, FieldBottom;
    // The gems' centres as fractions of the plate's width and height: x, y, x, y...; the first is the main one.
    public float[] Gems;
    public PlateSpec(string rarity, string file, int fieldLeft, int fieldTop, int fieldRight, int fieldBottom,
                     params float[] gems)
    {
        Rarity = rarity; File = file;
        FieldLeft = fieldLeft; FieldTop = fieldTop; FieldRight = fieldRight; FieldBottom = fieldBottom;
        Gems = gems;
    }
}

// Where a plate lies in the atlas and its field, in atlas pixels from the plate's top left corner.
public class PlateArt
{
    public string Rarity;
    public int X, Y, Width, Height;
    public float FieldLeft, FieldTop, FieldRight, FieldBottom;
    public float[] Gems;
}

// Where a plate's effect layers lie in the effects atlas, in its pixels: the silhouette and the glow; the glow covers
// the plate and GlowPad of the plates' atlas pixels more on every side.
public class PlateFx
{
    public int SilX, SilY, SilWidth, SilHeight;
    public int GlowX, GlowY, GlowWidth, GlowHeight;
    public float GlowPad;
}

public static class TitlePlateAtlas
{
    public const int Size = 1024, Margin = 8, Gap = 16, Scale = 2;
    // The originals' size in the atlas, the same for every plate: the crimson one (2452 wide) just fits.
    public const float PlateScale = 0.4f;

    // In the order of GameLogic::Titles::Rarity.
    public static readonly PlateSpec[] Specs =
    {
        new PlateSpec("silver", "5.png", 71, 56, 1491, 243),
        // the amethyst
        new PlateSpec("ice", "4.png", 68, 83, 1494, 274, 0.5f, 0.1f),
        // the emerald
        new PlateSpec("jade", "3.png", 295, 134, 1710, 318, 0.5f, 0.21f),
        // three sapphires, the middle one higher
        new PlateSpec("gold", "2.png", 373, 131, 1790, 310, 0.5f, 0.17f, 0.432f, 0.22f, 0.568f, 0.22f),
        // the crown's ruby and its three points
        new PlateSpec("crimson", "1.png", 523, 202, 1931, 383, 0.5f, 0.2f, 0.456f, 0.075f, 0.5f, 0.03f, 0.544f, 0.075f),
        // three sapphires between the laurels
        new PlateSpec("violet", "6 premium.png", 162, 124, 1574, 304, 0.5f, 0.16f, 0.42f, 0.2f, 0.58f, 0.2f),
    };

    public static Bitmap Pack(string folder, List<PlateArt> arts)
    {
        var atlas = new Bitmap(Size, Size, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(atlas))
        {
            g.Clear(Color.Transparent);
            g.CompositingMode = CompositingMode.SourceCopy;
            int y = Margin;
            foreach (var spec in Specs)
            {
                using (var source = new Bitmap(Path.Combine(folder, spec.File)))
                {
                    int w = (int)Math.Round(source.Width * PlateScale), h = (int)Math.Round(source.Height * PlateScale);
                    if (Margin + w > Size - Margin || y + h > Size - Margin)
                        throw new InvalidOperationException(spec.File + " does not fit in the atlas");

                    using (var plate = Resize(source, w, h))
                    {
                        g.DrawImage(plate, Margin, y, w, h);
                        float sx = (float)w / source.Width, sy = (float)h / source.Height;
                        // The field's edges are the pixels' outer sides: right and bottom are the last pixels.
                        arts.Add(new PlateArt
                        {
                            Rarity = spec.Rarity, X = Margin, Y = y, Width = w, Height = h,
                            FieldLeft = spec.FieldLeft * sx, FieldTop = spec.FieldTop * sy,
                            FieldRight = (spec.FieldRight + 1) * sx, FieldBottom = (spec.FieldBottom + 1) * sy,
                            Gems = spec.Gems,
                        });
                    }
                    y += h + Gap;
                    y = (y + 7) / 8 * 8;
                }
            }
        }
        return atlas;
    }

    // The effects atlas (see the script's header): silhouettes on the left, glows from FxGlowColumn, the sprites at
    // the top right.
    public const int FxWidth = 1024, FxHeight = 512, FxMargin = 4, FxGlowColumn = 512, FxGlowPad = 10;
    public const int StarX = 800, StarY = 8, StarSize = 64, EmberX = 880, EmberY = 8, EmberSize = 32;
    const int SilShrink = 2, GlowShrink = 4, GlowBlurRadius = 3, GlowBlurPasses = 3;
    const float GlowBoost = 1.8f;

    public static Bitmap PackFx(string folder, List<PlateArt> arts, List<PlateFx> fxs)
    {
        var atlas = new byte[FxWidth * FxHeight];
        int silY = FxMargin, glowY = FxMargin;
        for (int i = 0; i < Specs.Length; ++i)
        {
            var art = arts[i];
            var fx = new PlateFx();
            using (var source = new Bitmap(Path.Combine(folder, Specs[i].File)))
            {
                int sw = art.Width / SilShrink, sh = art.Height / SilShrink;
                using (var sil = Resize(source, sw, sh))
                    Blit(atlas, Alpha(sil), sw, sh, FxMargin, silY);
                fx.SilX = FxMargin; fx.SilY = silY; fx.SilWidth = sw; fx.SilHeight = sh;
                silY += sh + FxMargin;

                int gw = art.Width / GlowShrink, gh = art.Height / GlowShrink;
                int pw = gw + 2 * FxGlowPad, ph = gh + 2 * FxGlowPad;
                using (var small = Resize(source, gw, gh))
                    Blit(atlas, Glow(Alpha(small), gw, gh), pw, ph, FxGlowColumn, glowY);
                fx.GlowX = FxGlowColumn; fx.GlowY = glowY; fx.GlowWidth = pw; fx.GlowHeight = ph;
                fx.GlowPad = FxGlowPad * (float)art.Width / gw;
                glowY += ph + FxMargin;

                if (silY > FxHeight || glowY > FxHeight || FxMargin + sw > FxGlowColumn || FxGlowColumn + pw > StarX)
                    throw new InvalidOperationException(Specs[i].File + " does not fit in the effects atlas");
            }
            fxs.Add(fx);
        }
        Blit(atlas, Star(StarSize), StarSize, StarSize, StarX, StarY);
        Blit(atlas, Ember(EmberSize), EmberSize, EmberSize, EmberX, EmberY);
        return WhiteBitmap(atlas, FxWidth, FxHeight);
    }

    // Every pixel white, its alpha the value.
    static Bitmap WhiteBitmap(byte[] values, int w, int h)
    {
        var bitmap = new Bitmap(w, h, PixelFormat.Format32bppArgb);
        var data = bitmap.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try
        {
            var row = new byte[w * 4];
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    row[x * 4] = 255; row[x * 4 + 1] = 255; row[x * 4 + 2] = 255; row[x * 4 + 3] = values[y * w + x];
                }
                System.Runtime.InteropServices.Marshal.Copy(row, 0, data.Scan0 + y * data.Stride, row.Length);
            }
        }
        finally { bitmap.UnlockBits(data); }
        return bitmap;
    }

    static byte[] Alpha(Bitmap bitmap)
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
        var alpha = new byte[w * h];
        for (int i = 0; i < alpha.Length; ++i)
            alpha[i] = pixels[i * 4 + 3];
        return alpha;
    }

    static void Blit(byte[] atlas, byte[] values, int w, int h, int x0, int y0)
    {
        for (int y = 0; y < h; ++y)
            Array.Copy(values, y * w, atlas, (y0 + y) * FxWidth + x0, w);
    }

    // The silhouette with FxGlowPad more on every side, blurred and brightened.
    static byte[] Glow(byte[] alpha, int w, int h)
    {
        int pw = w + 2 * FxGlowPad, ph = h + 2 * FxGlowPad;
        var v = new float[pw * ph];
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                v[(y + FxGlowPad) * pw + x + FxGlowPad] = alpha[y * w + x] / 255f;
        for (int pass = 0; pass < GlowBlurPasses; ++pass)
            v = BoxBlur(BoxBlur(v, pw, ph, 1, 0), pw, ph, 0, 1);
        var result = new byte[pw * ph];
        for (int i = 0; i < v.Length; ++i)
            result[i] = (byte)Math.Min(255.0, Math.Round(v[i] * GlowBoost * 255));
        return result;
    }

    static float[] BoxBlur(float[] v, int w, int h, int dx, int dy)
    {
        var result = new float[v.Length];
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                float sum = 0;
                for (int k = -GlowBlurRadius; k <= GlowBlurRadius; ++k)
                {
                    int sx = x + k * dx, sy = y + k * dy;
                    if (sx >= 0 && sx < w && sy >= 0 && sy < h)
                        sum += v[sy * w + sx];
                }
                result[y * w + x] = sum / (2 * GlowBlurRadius + 1);
            }
        return result;
    }

    // A four-pointed sparkle with shorter diagonal rays and a soft core, black at its edges.
    static byte[] Star(int size)
    {
        var result = new byte[size * size];
        float m = (size - 1) / 2f;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                float dx = (x - m) / m, dy = (y - m) / m, d = (float)Math.Sqrt(dx * dx + dy * dy);
                float core = (float)Math.Exp(-(d / 0.2f) * (d / 0.2f));
                float u = (dx + dy) * 0.7071f, w = (dx - dy) * 0.7071f;
                float rays = Ray(dx, dy, 1f) + Ray(dy, dx, 1f) + 0.5f * (Ray(u, w, 0.55f) + Ray(w, u, 0.55f));
                float value = Math.Min(1f, core + rays) * Math.Max(0f, 1f - d * d);
                result[y * size + x] = (byte)Math.Round(value * 255);
            }
        return result;
    }

    // A thin ray along "along", fading out at its length.
    static float Ray(float along, float across, float length)
    {
        float fade = Math.Max(0f, 1f - Math.Abs(along) / length);
        return fade * fade * (float)Math.Exp(-(across / 0.045f) * (across / 0.045f));
    }

    // A soft round dot.
    static byte[] Ember(int size)
    {
        var result = new byte[size * size];
        float m = (size - 1) / 2f;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                float dx = (x - m) / m, dy = (y - m) / m, d = (float)Math.Sqrt(dx * dx + dy * dy);
                float value = (float)Math.Exp(-(d / 0.4f) * (d / 0.4f)) * Math.Max(0f, 1f - d);
                result[y * size + x] = (byte)Math.Round(value * 255);
            }
        return result;
    }

    static Bitmap Resize(Bitmap source, int w, int h)
    {
        var plate = new Bitmap(w, h, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(plate))
        using (var attributes = new ImageAttributes())
        {
            g.Clear(Color.Transparent);
            g.CompositingMode = CompositingMode.SourceCopy;
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.SmoothingMode = SmoothingMode.HighQuality;
            // Without it the bicubic filter fades the picture's outer rows into nothing.
            attributes.SetWrapMode(WrapMode.TileFlipXY);
            g.DrawImage(source, new Rectangle(0, 0, w, h), 0, 0, source.Width, source.Height, GraphicsUnit.Pixel, attributes);
        }
        return plate;
    }

    // A C++ float literal: "401.f", "64.78f".
    static string F(float value)
    {
        string text = value.ToString("0.##", CultureInfo.InvariantCulture);
        return text + (text.Contains(".") ? "f" : ".f");
    }

    public static string HeaderText(List<PlateArt> arts, List<PlateFx> fxs)
    {
        float fieldWidth = 0, fieldHeight = 0;
        foreach (var a in arts)
        {
            fieldWidth += (a.FieldRight - a.FieldLeft) / arts.Count;
            fieldHeight += (a.FieldBottom - a.FieldTop) / arts.Count;
        }

        var sb = new StringBuilder();
        sb.Append("#pragma once\n\n");
        sb.Append("// Written by tools/make_title_plates.ps1 together with Data/Interface/newui_titles.OZT; change the tool, not this file.\n");
        sb.Append("// Where each rarity's plate lies in the atlas and its field (the coloured rectangle the text sits on), in atlas\n");
        sb.Append("// pixels: the plate's place, then its field's left, top, right and bottom from the plate's top left corner. All\n");
        sb.Append("// plates have the same scale, so their fields are the same size: FieldWidth x FieldHeight on average.\n");
        sb.Append("namespace UI::TitlePlates::Art\n{\n");
        sb.AppendFormat("inline constexpr float AtlasSize = {0}.f;\n", Size);
        sb.AppendFormat("inline constexpr float FieldWidth = {0};\n", F(fieldWidth));
        sb.AppendFormat("inline constexpr float FieldHeight = {0};\n\n", F(fieldHeight));
        sb.Append("struct Plate\n{\n");
        sb.Append("    float x, y, width, height;\n");
        sb.Append("    float fieldLeft, fieldTop, fieldRight, fieldBottom;\n");
        sb.Append("};\n\n");
        sb.AppendFormat("inline constexpr Plate Plates[{0}] = {{\n", arts.Count);
        foreach (var a in arts)
        {
            sb.AppendFormat("    // {0}\n", a.Rarity);
            sb.AppendFormat("    {{{0}.f, {1}.f, {2}.f, {3}.f, {4}, {5}, {6}, {7}}},\n", a.X, a.Y, a.Width, a.Height,
                F(a.FieldLeft), F(a.FieldTop), F(a.FieldRight), F(a.FieldBottom));
        }
        sb.Append("};\n\n");
        AppendEffects(sb, arts, fxs);
        sb.Append("} // namespace UI::TitlePlates::Art\n");
        return sb.ToString();
    }

    static void AppendEffects(StringBuilder sb, List<PlateArt> arts, List<PlateFx> fxs)
    {
        sb.Append("// The effects atlas, Data/Interface/newui_titles_fx.OZT: white layers, their shape in the alpha, to be added\n");
        sb.Append("// and tinted. Each plate's silhouette (the shine) and glow (around the edge; it covers the plate and glowPad\n");
        sb.Append("// of the plates' atlas pixels more on every side), in effects atlas pixels; then the sparkle and the ember.\n");
        sb.AppendFormat("inline constexpr float FxAtlasWidth = {0}.f;\n", FxWidth);
        sb.AppendFormat("inline constexpr float FxAtlasHeight = {0}.f;\n\n", FxHeight);
        sb.Append("struct Rect\n{\n    float x, y, width, height;\n};\n\n");
        sb.Append("struct PlateFx\n{\n    Rect silhouette;\n    Rect glow;\n    float glowPad;\n};\n\n");
        sb.AppendFormat("inline constexpr PlateFx PlateEffects[{0}] = {{\n", fxs.Count);
        for (int i = 0; i < fxs.Count; ++i)
        {
            var f = fxs[i];
            sb.AppendFormat("    // {0}\n", arts[i].Rarity);
            sb.AppendFormat("    {{{{{0}.f, {1}.f, {2}.f, {3}.f}}, {{{4}.f, {5}.f, {6}.f, {7}.f}}, {8}}},\n", f.SilX, f.SilY,
                f.SilWidth, f.SilHeight, f.GlowX, f.GlowY, f.GlowWidth, f.GlowHeight, F(f.GlowPad));
        }
        sb.Append("};\n\n");
        sb.AppendFormat("inline constexpr Rect Sparkle = {{{0}.f, {1}.f, {2}.f, {2}.f}};\n", StarX, StarY, StarSize);
        sb.AppendFormat("inline constexpr Rect Ember = {{{0}.f, {1}.f, {2}.f, {2}.f}};\n\n", EmberX, EmberY, EmberSize);

        sb.Append("// The gems drawn on each plate, where the sparkles go: their centres in the plates' atlas pixels from the plate's\n");
        sb.Append("// top left corner, the main one first.\n");
        sb.Append("inline constexpr int MaxGems = 4;\n\n");
        sb.Append("struct Point\n{\n    float x, y;\n};\n\n");
        sb.Append("struct Gems\n{\n    int count;\n    Point at[MaxGems];\n};\n\n");
        sb.AppendFormat("inline constexpr Gems PlateGems[{0}] = {{\n", arts.Count);
        foreach (var a in arts)
        {
            int n = a.Gems.Length / 2;
            if (n > 4)
                throw new InvalidOperationException(a.Rarity + " has more than 4 gems");
            var points = new List<string>();
            for (int g = 0; g < n; ++g)
                points.Add("{" + F(a.Gems[2 * g] * a.Width) + ", " + F(a.Gems[2 * g + 1] * a.Height) + "}");
            sb.AppendFormat("    // {0}\n    {{{1}, {{{2}}}}},\n", a.Rarity, n, string.Join(", ", points));
        }
        sb.Append("};\n");
    }

    public static string JsonText(List<PlateArt> arts)
    {
        var sb = new StringBuilder("[\n");
        for (int i = 0; i < arts.Count; ++i)
        {
            var a = arts[i];
            sb.AppendFormat(CultureInfo.InvariantCulture,
                "  {{\"rarity\":\"{0}\",\"x\":{1},\"y\":{2},\"width\":{3},\"height\":{4},\"fieldLeft\":{5},\"fieldTop\":{6},\"fieldRight\":{7},\"fieldBottom\":{8}}}{9}\n",
                a.Rarity, a.X, a.Y, a.Width, a.Height, a.FieldLeft, a.FieldTop, a.FieldRight, a.FieldBottom,
                i + 1 < arts.Count ? "," : "");
        }
        return sb.Append("]\n").ToString();
    }

    // The game's OZT: 4 bytes (here the MUX marker and the scale), an 18 byte TGA header (uncompressed, 32 bit,
    // bottom row first), BGRA pixels.
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
            file.Write(new byte[] { (byte)'M', (byte)'U', (byte)'X', (byte)Scale });
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

$arts = New-Object 'System.Collections.Generic.List[PlateArt]'
$atlas = [TitlePlateAtlas]::Pack([System.IO.Path]::GetFullPath($Src), $arts)
try {
    $full = [System.IO.Path]::GetFullPath($Out)
    [TitlePlateAtlas]::SaveOzt($atlas, $full)
    Write-Output "Wrote $full"
    $fxs = New-Object 'System.Collections.Generic.List[PlateFx]'
    $fx = [TitlePlateAtlas]::PackFx([System.IO.Path]::GetFullPath($Src), $arts, $fxs)
    try {
        $fxFull = [System.IO.Path]::GetFullPath($FxOut)
        [TitlePlateAtlas]::SaveOzt($fx, $fxFull)
        Write-Output "Wrote $fxFull"
        if ($Png) {
            $fxPng = [System.IO.Path]::ChangeExtension([System.IO.Path]::GetFullPath($Png), '.fx.png')
            $fx.Save($fxPng, [System.Drawing.Imaging.ImageFormat]::Png)
            Write-Output "Wrote $fxPng"
        }
    }
    finally {
        $fx.Dispose()
    }
    $headerFull = [System.IO.Path]::GetFullPath($Header)
    [System.IO.File]::WriteAllText($headerFull, [TitlePlateAtlas]::HeaderText($arts, $fxs), (New-Object System.Text.UTF8Encoding $false))
    Write-Output "Wrote $headerFull"
    if ($Png) {
        $atlas.Save([System.IO.Path]::GetFullPath($Png), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Output "Wrote $Png"
    }
    if ($Json) {
        [System.IO.File]::WriteAllText([System.IO.Path]::GetFullPath($Json), [TitlePlateAtlas]::JsonText($arts))
        Write-Output "Wrote $Json"
    }
}
finally {
    $atlas.Dispose()
}
