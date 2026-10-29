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
    public int FieldLeft, FieldTop, FieldRight, FieldBottom;
    public PlateSpec(string rarity, string file, int fieldLeft, int fieldTop, int fieldRight, int fieldBottom)
    {
        Rarity = rarity; File = file;
        FieldLeft = fieldLeft; FieldTop = fieldTop; FieldRight = fieldRight; FieldBottom = fieldBottom;
    }
}

// Where a plate lies in the atlas and its field, in atlas pixels from the plate's top left corner.
public class PlateArt
{
    public string Rarity;
    public int X, Y, Width, Height;
    public float FieldLeft, FieldTop, FieldRight, FieldBottom;
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
        new PlateSpec("ice", "4.png", 68, 83, 1494, 274),
        new PlateSpec("jade", "3.png", 295, 134, 1710, 318),
        new PlateSpec("gold", "2.png", 373, 131, 1790, 310),
        new PlateSpec("crimson", "1.png", 523, 202, 1931, 383),
        new PlateSpec("violet", "6 premium.png", 162, 124, 1574, 304),
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
                        });
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

    static string F(float value) { return value.ToString("0.##", CultureInfo.InvariantCulture) + "f"; }

    public static string HeaderText(List<PlateArt> arts)
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
