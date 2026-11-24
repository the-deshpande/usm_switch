"""Button icons for the tutorial prompts (in the style of the PS2 version).

    py tools/make_glyphs.py [--target vita|switch]

--target vita (default): draws the PlayStation button icons into src/vita/glyphs.h, a
256x32 RGBA atlas of 8 slots: 0 cross, 1 circle, 2 square, 3 triangle, 4 L, 5 R.
The icons are drawn in code (a dark disc with the coloured symbol, L/R as grey keys);
gl.c draws font glyphs that point at it (native.c f_7791a0).

--target switch: builds src/switch/glyphs.h from Kenney's Input Prompts 1.5A (CC0,
tools/button-art, the same set Brandon's SPM project vendors at tools/button-art/),
same 256x32 RGBA atlas, same 8 slots, in the slot order the game expects:
0 B (Jump), 1 A (Wall Crawl / Web), 2 Y (Punch), 3 X (Kick), 4 L (Web Zip),
5 R (Web Swing). Each Kenney glyph (a white disc / shoulder shape with the letter
knocked out) is set on the same dark disc / grey key backing the Vita atlas uses, so
the icon reads on light comic panels as well as dark ones. Rendering needs
ImageMagick (magick, as SPM's tools/make-button-icons.py uses) or cairosvg.
tools/button-art/nintendo also vendors switch_button_plus.svg and
switch_stick_l/r_press.svg: the Kenney art for Plus and the stick clicks exists, but
the atlas has only 8 slots and Pause / Map / Center Camera are words in
src/switch/native_prompts.c (PLUS, LS CLICK, RS CLICK), following the Vita precedent
(SELECT / START / REAR TOUCH) -- those three SVGs are the source art if the atlas is
ever extended to cover them.
"""
import argparse
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

S, K = 32, 4             # slot size, supersampling
B = S * K
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
ART = os.path.join(ROOT, "tools", "button-art", "nintendo")

# Game Button 1..6 -> Kenney SVG, in atlas slot order (see module docstring).
SWITCH_SVGS = ["switch_button_b", "switch_button_a", "switch_button_y",
               "switch_button_x", "switch_button_l", "switch_button_r"]


def render_svg(name, size):
    """Kenney SVG rendered to fit size x size, RGBA. ImageMagick first (SPM's route),
    cairosvg as the fallback where magick is not installed."""
    path = os.path.join(ART, name + ".svg")
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "r.png")
        try:
            subprocess.run(["magick", "-background", "none", "-density", "1200", path,
                            "-resize", "%dx%d" % (size, size), out], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except (OSError, subprocess.CalledProcessError):
            try:
                import cairosvg
            except ImportError:
                raise SystemExit("make_glyphs: need ImageMagick (magick) or cairosvg to render %s" % path)
            with open(path, "rb") as f:
                cairosvg.svg2png(bytestring=f.read(), output_width=size, output_height=size, write_to=out)
        return Image.open(out).convert("RGBA").copy()


def disc(draw):
    draw.ellipse((2 * K, 2 * K, B - 2 * K, B - 2 * K), fill=(18, 18, 20, 255))         # dark rim
    draw.ellipse((4 * K, 4 * K, B - 4 * K, B - 4 * K), fill=(66, 66, 72, 255))         # grey face


def key_backing(draw):
    draw.rounded_rectangle((2 * K, 5 * K, B - 2 * K, B - 5 * K), radius=6 * K, fill=(18, 18, 20, 255))
    draw.rounded_rectangle((4 * K, 7 * K, B - 4 * K, B - 7 * K), radius=5 * K, fill=(86, 86, 94, 255))


def vita_icon(k, font):
    im = Image.new("RGBA", (B, B), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c, w = B / 2, int(2.6 * K)
    if k < 4:
        disc(d)
        r = B * 0.22
        if k == 0:    # cross, blue
            col = (124, 178, 232, 255)
            d.line((c - r, c - r, c + r, c + r), fill=col, width=w)
            d.line((c - r, c + r, c + r, c - r), fill=col, width=w)
        elif k == 1:  # circle, red
            d.ellipse((c - r, c - r, c + r, c + r), outline=(255, 102, 102, 255), width=w)
        elif k == 2:  # square, pink
            d.rectangle((c - r * 0.9, c - r * 0.9, c + r * 0.9, c + r * 0.9), outline=(232, 143, 208, 255), width=w)
        else:         # triangle, green
            p = [(c, c - r * 1.05), (c + r * 1.1, c + r * 0.8), (c - r * 1.1, c + r * 0.8)]
            d.line(p + [p[0], p[1]], fill=(64, 224, 176, 255), width=w, joint="curve")
    else:             # L / R key
        key_backing(d)
        d.text((c, c), "L" if k == 4 else "R", font=font, fill=(255, 255, 255, 255), anchor="mm")
    return im.resize((S, S), Image.LANCZOS)


def switch_icon(k):
    """Kenney glyph on the Vita-style backing, so the white glyph reads on any panel."""
    im = Image.new("RGBA", (B, B), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    if k < 4:
        disc(d)
    else:
        key_backing(d)
    glyph = render_svg(SWITCH_SVGS[k], B)
    im.paste(glyph, (0, 0), glyph)
    return im.resize((S, S), Image.LANCZOS)


def write_atlas(out, icons, comment):
    atlas = Image.new("RGBA", (S * 8, S), (0, 0, 0, 0))
    for k in range(6):
        atlas.paste(icons[k], (k * S, 0))
    px = atlas.tobytes()
    assert len(px) == S * 8 * S * 4
    with open(out, "w") as f:
        f.write("/* %s */\n" % comment)
        f.write("#define GLYPHS_W %d\n#define GLYPHS_H %d\nstatic const unsigned char g_glyphs_rgba[] = {\n" % (S * 8, S))
        for i in range(0, len(px), 32):
            f.write("".join("%d," % b for b in px[i:i + 32]) + "\n")
        f.write("};\n")
    print("wrote", out)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--target", choices=["vita", "switch"], default="vita")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()
    if args.target == "vita":
        font = ImageFont.truetype("arialbd.ttf", int(B * 0.62))
        icons = [vita_icon(k, font) for k in range(6)]
        out = args.out or os.path.join(ROOT, "src", "vita", "glyphs.h")
        write_atlas(out, icons,
                    "generated by tools/make_glyphs.py: PlayStation button icons, %dx%d RGBA, 8 slots of %d px"
                    % (S * 8, S, S))
    else:
        icons = [switch_icon(k) for k in range(6)]
        out = args.out or os.path.join(ROOT, "src", "switch", "glyphs.h")
        write_atlas(out, icons,
                    "generated by tools/make_glyphs.py --target switch: Nintendo Switch button icons "
                    "(Kenney Input Prompts 1.5A, CC0, tools/button-art), %dx%d RGBA, 8 slots of %d px; "
                    "slots 0 B, 1 A, 2 Y, 3 X, 4 L, 5 R" % (S * 8, S, S))


if __name__ == "__main__":
    main()
