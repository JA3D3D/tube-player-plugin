"""Build the Tube Player logo files (Screen Wave mark, hot pink on dark).

usage: python3 brand/make_logo.py <SpaceGrotesk[wght].ttf> <out dir>
Writes SVGs with the lettering converted to outlines (no font needed to open them),
then render_logo.js turns them into PNGs.
"""
import os, sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools.pens.svgPathPen import SVGPathPen

PINK = "#ff2e93"
INK = "#ecebe6"
DIM = "#a3a29d"
BLACK = "#141518"

FONT_PATH, OUT = sys.argv[1], sys.argv[2]
os.makedirs(os.path.join(OUT, "svg"), exist_ok=True)


def load(weight):
    f = TTFont(FONT_PATH)
    return instancer.instantiateVariableFont(f, {"wght": weight})


FONTS = {500: load(500), 700: load(700)}


def text_paths(text, weight, size, x, baseline, fill, tracking=0.0):
    """Lettering as SVG paths; returns (svg, width)."""
    font = FONTS[weight]
    gs = font.getGlyphSet()
    cmap = font.getBestCmap()
    upm = font["head"].unitsPerEm
    s = size / upm
    out, cx = [], x
    for ch in text:
        name = cmap.get(ord(ch))
        if name is None:
            continue
        pen = SVGPathPen(gs)
        gs[name].draw(pen)
        d = pen.getCommands()
        if d:
            out.append('<path transform="translate(%.2f %.2f) scale(%.5f %.5f)" d="%s"/>' % (cx, baseline, s, -s, d))
        cx += gs[name].width * s + tracking * size
    return '<g fill="%s">%s</g>' % (fill, "".join(out)), cx - x - tracking * size


# ------------------------------------------------------------------ the mark (240 x 240 design grid)
def tile(bg_id):
    return ('<defs><radialGradient id="%s" cx="50%%" cy="36%%" r="72%%"><stop offset="0" stop-color="#23262b"/>'
            '<stop offset="1" stop-color="#0c0d0f"/></radialGradient></defs>'
            '<rect width="240" height="240" rx="54" fill="url(#%s)"/>'
            '<rect x="1.5" y="1.5" width="237" height="237" rx="52.5" fill="none" stroke="#fff" stroke-opacity=".08" stroke-width="3"/>') % (bg_id, bg_id)


def mark_full(screen=INK, wave=PINK, rest_opacity=".38", head=INK, screen_fill="#08090a"):
    return (f'<rect x="40" y="58" width="160" height="104" rx="18" fill="{screen_fill}" stroke="{screen}" stroke-width="7"/>'
            f'<path d="M120 162 V184 M96 186 H144" stroke="{screen}" stroke-width="7" stroke-linecap="round"/>'
            f'<polyline points="58,110 68,110 74,98 80,124 86,88 92,132 98,102 104,120 110,82 116,138 124,96" fill="none" stroke="{wave}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>'
            f'<polyline points="124,96 130,126 136,104 142,118 148,106 154,114 160,108 166,112 172,110 182,110" fill="none" stroke="{screen}" stroke-opacity="{rest_opacity}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>'
            f'<line x1="124" y1="74" x2="124" y2="146" stroke="{head}" stroke-width="4" stroke-linecap="round"/>'
            f'<circle cx="124" cy="74" r="6" fill="{head}"/>')


def mark_medium(screen=INK, wave=PINK, rest_opacity=".38", screen_fill="#08090a"):
    return (f'<rect x="40" y="58" width="160" height="104" rx="18" fill="{screen_fill}" stroke="{screen}" stroke-width="10"/>'
            f'<path d="M120 162 V184 M96 186 H144" stroke="{screen}" stroke-width="10" stroke-linecap="round"/>'
            f'<polyline points="58,110 74,96 86,88 98,124 110,82 124,110" fill="none" stroke="{wave}" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/>'
            f'<polyline points="124,110 140,118 156,106 182,110" fill="none" stroke="{screen}" stroke-opacity="{rest_opacity}" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/>')


def mark_small():
    return ('<rect x="36" y="56" width="168" height="112" rx="20" fill="#08090a" stroke="%s" stroke-width="16"/>'
            '<polyline points="60,112 84,88 104,132 124,96 150,118 180,110" fill="none" stroke="%s" stroke-width="16" stroke-linecap="round" stroke-linejoin="round"/>') % (INK, PINK)


def mark_tiny():
    return ('<rect width="240" height="240" rx="54" fill="#17191c"/>'
            '<rect x="34" y="58" width="172" height="120" rx="22" fill="none" stroke="%s" stroke-width="24"/>'
            '<polyline points="64,118 96,92 128,138 168,110" fill="none" stroke="%s" stroke-width="24" stroke-linecap="round" stroke-linejoin="round"/>') % (INK, PINK)


def svg(w, h, body, vb=None):
    return '<svg xmlns="http://www.w3.org/2000/svg" width="%g" height="%g" viewBox="%s">%s</svg>\n' % (w, h, vb or "0 0 %g %g" % (w, h), body)


def write(name, content):
    with open(os.path.join(OUT, "svg", name), "w") as f:
        f.write(content)


# app icon, full bleed square (store pages, website, socials)
write("icon.svg", svg(1024, 1024, tile("bg") + mark_full(), "0 0 240 240"))
write("icon-64.svg", svg(64, 64, tile("bg") + mark_medium(), "0 0 240 240"))
write("icon-32.svg", svg(32, 32, tile("bg") + mark_small(), "0 0 240 240"))
write("icon-16.svg", svg(16, 16, mark_tiny(), "0 0 240 240"))

# macOS app icon layout: the tile sits inside the 1024 canvas with Apple's standard margin and shadow
mac = ('<defs><filter id="sh" x="-20%" y="-20%" width="140%" height="140%"><feDropShadow dx="0" dy="10" stdDeviation="14" flood-opacity=".35"/></filter></defs>'
       '<g filter="url(#sh)"><g transform="translate(100 100) scale(3.4333)">' + tile("bgm") + mark_full() + '</g></g>')
write("icon-macos.svg", svg(1024, 1024, mac))

# mark without the tile, one color
write("mark-white.svg", svg(240, 240, mark_medium(screen="#ffffff", wave="#ffffff", rest_opacity=".45", screen_fill="none"), "0 0 240 240"))
write("mark-black.svg", svg(240, 240, mark_medium(screen=BLACK, wave=BLACK, rest_opacity=".4", screen_fill="none"), "0 0 240 240"))
write("mark-pink.svg", svg(240, 240, mark_medium(screen=INK, wave=PINK, screen_fill="none"), "0 0 240 240"))


def lockup(dark=True):
    """Icon + 'tube player' + 'BY BASS BIBLE'."""
    size, gap = 64, 18
    x0 = size + gap
    if dark:
        t1, w1 = text_paths("tube ", 500, 34, x0, 36, INK, -0.015)
        t2, w2 = text_paths("player", 700, 34, x0 + w1, 36, PINK, -0.015)
        t3, w3 = text_paths("BY BASS BIBLE", 500, 12.5, x0 + 1, 57, DIM, 0.08)
        icon = '<svg x="0" y="0" width="64" height="64" viewBox="0 0 240 240">' + tile("bgl") + mark_medium() + '</svg>'
    else:
        t1, w1 = text_paths("tube ", 500, 34, x0, 36, BLACK, -0.015)
        t2, w2 = text_paths("player", 700, 34, x0 + w1, 36, BLACK, -0.015)
        t3, w3 = text_paths("BY BASS BIBLE", 500, 12.5, x0 + 1, 57, BLACK, 0.08)
        icon = ('<svg x="0" y="0" width="64" height="64" viewBox="0 0 240 240">'
                + mark_medium(screen=BLACK, wave=BLACK, rest_opacity=".4", screen_fill="none") + '</svg>')
    width = x0 + max(w1 + w2, w3) + 2
    return svg(round(width), 64, icon + t1 + t2 + t3), round(width)


dark_svg, lw = lockup(True)
light_svg, _ = lockup(False)
write("logo-dark.svg", dark_svg)
write("logo-light.svg", light_svg)

# wordmark alone
t1, w1 = text_paths("tube ", 500, 34, 0, 30, INK, -0.015)
t2, w2 = text_paths("player", 700, 34, w1, 30, PINK, -0.015)
write("wordmark-dark.svg", svg(round(w1 + w2 + 2), 40, t1 + t2))
print("logo files written; lockup width", lw)
