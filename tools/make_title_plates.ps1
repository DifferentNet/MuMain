# MU Moldova: packs the title plates' art into src/bin/Data/Interface/newui_titles.OZT and writes where each plate
# lies in it to src/source/UI/Character/TitlePlatesArt.h (UI/Character/TitlePlates.cpp reads both).
#
# The art: six drawn plates, one per rarity, by ghinzuro and his wife (2026-10-29, "titles.zip"); the five ranks get
# ever richer plates and the unique violet titles (tester, streamer) the laurels. The originals are large (up to
# 2452 x 439) and are kept outside this public repository, in E:\Mu Online Server\art\title-plates-2026-10-29:
#   silver 5.png, ice 4.png, jade 3.png, gold 2.png, crimson 1.png, violet "6 premium.png".
#
# Every plate is scaled so its core (the coloured field the text sits on) is CoreHeight pixels high, and the plates
# are stacked one under another in a 1024 x 1024 atlas. The file is marked as twice its original size (MUX 2, see
# Render/Textures/UpscaleMarker.h), so the game builds its mipmaps and the plates stay smooth when drawn small.
#
# The game cuts each plate into its left end (wings, laurels or the point), a straight part of the frame, the middle
# (the gems or the crown) and the same on the right; for a longer title it widens the straight parts, for a short
# one it cuts them down to nothing. The numbers below are in the original's pixels, measured on its left half; the
# right half is the mirror image:
#   CoreTop, CoreBottom  the core's top and bottom edges on a straight part;
#   CapEnd               where the left end stops and the straight part begins;
#   DecoStart            where the middle (the gems, the crown) begins;
#   TextInset            how far left the text may reach, where the core is still about two thirds of its height.
#
# Run from anywhere with Windows PowerShell 5.1 (System.Drawing):
#   powershell -ExecutionPolicy Bypass -File tools/make_title_plates.ps1 [-Src folder] [-Out ozt] [-Header h] [-Png png] [-Json json]
param(
    [string]$Src = 'E:\Mu Online Server\art\title-plates-2026-10-29',
    [string]$Out = (Join-Path $PSScriptRoot '..\src\bin\Data\Interface\newui_titles.OZT'),
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
    public int CoreTop, CoreBottom, CapEnd, DecoStart, TextInset;
    public PlateSpec(string rarity, string file, int coreTop, int coreBottom, int capEnd, int decoStart, int textInset)
    {
        Rarity = rarity; File = file; CoreTop = coreTop; CoreBottom = coreBottom;
        CapEnd = capEnd; DecoStart = decoStart; TextInset = textInset;
    }
}

// Where a plate lies in the atlas and its parts, in atlas pixels from the plate's top left corner.
public class PlateArt
{
    public string Rarity;
    public int X, Y, Width, Height;
    public float CoreTop, CoreBottom, CapLeft, CapRight, DecoLeft, DecoRight, TextLeft, TextRight, CleanLeft, CleanRight;
}

public static class TitlePlateAtlas
{
    public const int Size = 1024, CoreHeight = 80, Margin = 8, Gap = 16, Scale = 2;

    // In the order of GameLogic::Titles::Rarity.
    public static readonly PlateSpec[] Specs =
    {
        new PlateSpec("silver", "5.png", 50, 245, 200, 771, 125),
        new PlateSpec("ice", "4.png", 75, 285, 200, 725, 117),
        new PlateSpec("jade", "3.png", 148, 320, 440, 905, 365),
        new PlateSpec("gold", "2.png", 120, 318, 500, 855, 420),
        new PlateSpec("crimson", "1.png", 190, 395, 700, 1040, 618),
        new PlateSpec("violet", "6 premium.png", 125, 318, 290, 645, 215),
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
                    float t = (float)CoreHeight / (spec.CoreBottom - spec.CoreTop);
                    int w = (int)Math.Round(source.Width * t), h = (int)Math.Round(source.Height * t);
                    if (Margin + w > Size - Margin || y + h > Size - Margin)
                        throw new InvalidOperationException(spec.File + " does not fit in the atlas");

                    using (var plate = Resize(source, w, h))
                    {
                        g.DrawImage(plate, Margin, y, w, h);
                        float sx = (float)w / source.Width, sy = (float)h / source.Height;
                        var art = new PlateArt
                        {
                            Rarity = spec.Rarity, X = Margin, Y = y, Width = w, Height = h,
                            CoreTop = spec.CoreTop * sy, CoreBottom = spec.CoreBottom * sy,
                            CapLeft = spec.CapEnd * sx, CapRight = (source.Width - spec.CapEnd) * sx,
                            DecoLeft = spec.DecoStart * sx, DecoRight = (source.Width - spec.DecoStart) * sx,
                            TextLeft = spec.TextInset * sx, TextRight = (source.Width - spec.TextInset) * sx,
                        };
                        art.CleanLeft = CleanColumn(plate, art.CapLeft, art.DecoLeft, art.CoreTop, art.CoreBottom);
                        art.CleanRight = CleanColumn(plate, art.DecoRight, art.CapRight, art.CoreTop, art.CoreBottom);
                        arts.Add(art);
                    }
                    y += h + Gap;
                    y = (y + 7) / 8 * 8;
                }
            }
        }
        return atlas;
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

    // The column of a straight part closest to the part's usual look (row by row, the median of its columns), the one
    // the game stretches for a long title: no glint gets smeared along the frame. Returns its centre.
    static float CleanColumn(Bitmap plate, float from, float to, float coreTop, float coreBottom)
    {
        int x0 = (int)Math.Ceiling(from) + 2, x1 = (int)Math.Floor(to) - 2;
        if (x1 <= x0)
            return (from + to) / 2f;

        int h = plate.Height, n = x1 - x0;
        var columns = new Color[n][];
        for (int i = 0; i < n; ++i)
        {
            columns[i] = new Color[h];
            for (int y = 0; y < h; ++y)
                columns[i][y] = plate.GetPixel(x0 + i, y);
        }

        var median = new int[h, 4];
        var values = new int[n];
        for (int y = 0; y < h; ++y)
        {
            for (int channel = 0; channel < 4; ++channel)
            {
                for (int i = 0; i < n; ++i)
                {
                    var c = columns[i][y];
                    values[i] = channel == 0 ? c.R : channel == 1 ? c.G : channel == 2 ? c.B : c.A;
                }
                Array.Sort(values);
                median[y, channel] = values[n / 2];
            }
        }

        int best = 0;
        long bestDistance = long.MaxValue;
        for (int i = 0; i < n; ++i)
        {
            long distance = 0;
            for (int y = 0; y < h; ++y)
            {
                var c = columns[i][y];
                distance += Math.Abs(c.R - median[y, 0]) + Math.Abs(c.G - median[y, 1]) + Math.Abs(c.B - median[y, 2]) +
                            Math.Abs(c.A - median[y, 3]);
            }
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
        }
        return x0 + best + 0.5f;
    }

    static string F(float value) { return value.ToString("0.##", CultureInfo.InvariantCulture) + "f"; }

    public static string HeaderText(List<PlateArt> arts)
    {
        var sb = new StringBuilder();
        sb.Append("#pragma once\n\n");
        sb.Append("// Written by tools/make_title_plates.ps1 together with Data/Interface/newui_titles.OZT; change the tool, not this file.\n");
        sb.Append("// Where each rarity's plate lies in the atlas and its parts (UI/Character/TitlePlates.cpp), in atlas pixels:\n");
        sb.Append("// the plate's place, its core's top and bottom, where the left end stops and the right end starts, where the\n");
        sb.Append("// middle (gems, crown) starts and stops, how far the text may reach on each side, and the straight column\n");
        sb.Append("// the game stretches on each side; all but the place are from the plate's top left corner.\n");
        sb.Append("namespace UI::TitlePlates::Art\n{\n");
        sb.AppendFormat("inline constexpr float AtlasSize = {0}.f;\n\n", Size);
        sb.Append("struct Plate\n{\n");
        sb.Append("    float x, y, width, height;\n");
        sb.Append("    float coreTop, coreBottom;\n");
        sb.Append("    float capLeft, capRight;\n");
        sb.Append("    float decoLeft, decoRight;\n");
        sb.Append("    float textLeft, textRight;\n");
        sb.Append("    float cleanLeft, cleanRight;\n");
        sb.Append("};\n\n");
        sb.AppendFormat("inline constexpr Plate Plates[{0}] = {{\n", arts.Count);
        foreach (var a in arts)
        {
            sb.AppendFormat("    // {0}\n", a.Rarity);
            sb.AppendFormat("    {{{0}.f, {1}.f, {2}.f, {3}.f, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}, {13}}},\n",
                a.X, a.Y, a.Width, a.Height, F(a.CoreTop), F(a.CoreBottom), F(a.CapLeft), F(a.CapRight),
                F(a.DecoLeft), F(a.DecoRight), F(a.TextLeft), F(a.TextRight), F(a.CleanLeft), F(a.CleanRight));
        }
        sb.Append("};\n");
        sb.Append("} // namespace UI::TitlePlates::Art\n");
        return sb.ToString();
    }

    public static string JsonText(List<PlateArt> arts)
    {
        var sb = new StringBuilder("[\n");
        for (int i = 0; i < arts.Count; ++i)
        {
            var a = arts[i];
            sb.AppendFormat(CultureInfo.InvariantCulture,
                "  {{\"rarity\":\"{0}\",\"x\":{1},\"y\":{2},\"width\":{3},\"height\":{4},\"coreTop\":{5},\"coreBottom\":{6},\"capLeft\":{7},\"capRight\":{8},\"decoLeft\":{9},\"decoRight\":{10},\"textLeft\":{11},\"textRight\":{12},\"cleanLeft\":{13},\"cleanRight\":{14}}}{15}\n",
                a.Rarity, a.X, a.Y, a.Width, a.Height, a.CoreTop, a.CoreBottom, a.CapLeft, a.CapRight, a.DecoLeft,
                a.DecoRight, a.TextLeft, a.TextRight, a.CleanLeft, a.CleanRight, i + 1 < arts.Count ? "," : "");
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
    $headerFull = [System.IO.Path]::GetFullPath($Header)
    [System.IO.File]::WriteAllText($headerFull, [TitlePlateAtlas]::HeaderText($arts), (New-Object System.Text.UTF8Encoding $false))
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
