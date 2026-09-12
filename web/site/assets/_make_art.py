#!/usr/bin/env python3
"""Author original FOON marketing PNGs. Not IWAD / PLAYPAL data."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).resolve().parent
MONO = "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf"
MONO_BOLD = "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Bold.ttf"

BG = (7, 7, 5)
AMBER = (255, 176, 0)
AMBER_HOT = (255, 211, 106)
PHOSPHOR = (200, 255, 74)
INK = (216, 199, 160)
INK_DIM = (138, 122, 85)
LINE = (42, 36, 20)
RUST = (196, 74, 42)
PANEL = (14, 13, 9)

# Authored 32-color marketing strip. Not Doom PLAYPAL.
PALETTE32 = [
    (7, 7, 5),
    (18, 17, 12),
    (36, 30, 18),
    (58, 44, 24),
    (90, 62, 28),
    (122, 78, 32),
    (168, 96, 36),
    (196, 74, 42),
    (210, 120, 48),
    (232, 160, 56),
    (255, 176, 0),
    (255, 211, 106),
    (232, 210, 150),
    (216, 199, 160),
    (180, 164, 120),
    (138, 122, 85),
    (74, 86, 36),
    (98, 132, 40),
    (148, 188, 52),
    (200, 255, 74),
    (42, 64, 56),
    (48, 92, 88),
    (64, 120, 110),
    (28, 36, 48),
    (48, 56, 72),
    (72, 84, 104),
    (28, 16, 14),
    (72, 24, 20),
    (120, 36, 28),
    (168, 48, 36),
    (88, 72, 48),
    (12, 10, 8),
]


def font(path: str, size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(path, size)


def blit_px(dst: Image.Image, src: Image.Image, box: tuple[int, int, int, int]) -> None:
    x0, y0, x1, y1 = box
    scaled = src.resize((x1 - x0, y1 - y0), Image.Resampling.NEAREST)
    dst.paste(scaled, (x0, y0))


def draw_foon_bitmap(img: Image.Image, x: int, y: int, scale: int, color: tuple[int, int, int]) -> None:
    """5x7 block letters for F O O N."""
    glyphs = {
        "F": ["11111", "10000", "11110", "10000", "10000", "10000", "10000"],
        "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
        "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    }
    px = ImageDraw.Draw(img)
    cursor = x
    for ch in "FOON":
        rows = glyphs[ch]
        for j, row in enumerate(rows):
            for i, bit in enumerate(row):
                if bit == "1":
                    px.rectangle(
                        [
                            cursor + i * scale,
                            y + j * scale,
                            cursor + (i + 1) * scale - 1,
                            y + (j + 1) * scale - 1,
                        ],
                        fill=color,
                    )
        cursor += 6 * scale


def make_framebuffer() -> Image.Image:
    fb = Image.new("RGB", (320, 200), (14, 12, 10))
    d = ImageDraw.Draw(fb)

    # Ceiling
    d.rectangle([0, 0, 319, 71], fill=(22, 20, 16))
    # Floor
    d.polygon([(0, 167), (128, 148), (192, 148), (319, 167)], fill=(48, 36, 22))
    for i in range(0, 12):
        y = 148 + i * 2
        d.line([(int(128 - i * 11), y), (int(192 + i * 11), y)], fill=(36, 26, 16))

    # Left / right walls as indexed blocks
    d.polygon([(0, 0), (128, 72), (128, 148), (0, 199)], fill=(58, 42, 26))
    d.polygon([(319, 0), (192, 72), (192, 148), (319, 199)], fill=(50, 36, 22))
    for i in range(6):
        lx = int(8 + i * 20)
        rx = 319 - lx
        d.line([(lx, 8 + i * 10), (lx, 190 - i * 8)], fill=(36, 26, 16))
        d.line([(rx, 8 + i * 10), (rx, 190 - i * 8)], fill=(32, 22, 14))

    # Far wall + amber door
    d.rectangle([128, 72, 192, 148], fill=(36, 28, 20))
    d.rectangle([146, 96, 174, 148], fill=(118, 62, 18))
    d.rectangle([152, 112, 158, 122], fill=(255, 211, 106))

    draw_foon_bitmap(fb, 116, 8, 4, AMBER)

    # Status strip (32px, HUD-like, original blocks)
    d.rectangle([0, 168, 319, 199], fill=(16, 14, 12))
    d.rectangle([6, 172, 48, 196], fill=(90, 42, 26))
    d.rectangle([54, 176, 140, 182], fill=(61, 90, 42))
    d.rectangle([54, 188, 112, 194], fill=(138, 58, 26))
    d.rectangle([236, 172, 314, 196], fill=(36, 36, 28))
    d.rectangle([244, 176, 260, 192], fill=(196, 68, 68))
    d.rectangle([266, 176, 282, 192], fill=(68, 168, 68))
    d.rectangle([288, 176, 304, 192], fill=(204, 204, 68))
    return fb


def make_crt() -> Image.Image:
    w, h = 1152, 864
    canvas = Image.new("RGB", (w, h), (4, 4, 3))
    d = ImageDraw.Draw(canvas)

    bezel = [96, 48, 1056, 816]
    d.rounded_rectangle(bezel, radius=72, fill=(18, 18, 16), outline=(36, 32, 24), width=4)
    d.rounded_rectangle([112, 64, 1040, 760], radius=58, fill=(8, 8, 6))

    glass = [168, 108, 984, 700]
    d.rounded_rectangle(glass, radius=28, fill=(10, 10, 8))

    fb = make_framebuffer()
    blit_px(canvas, fb, (196, 132, 956, 676))

    overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    for y in range(132, 676, 2):
        od.line([(196, y), (955, y)], fill=(0, 0, 0, 38 if y % 4 == 0 else 18))
    od.rectangle([196, 132, 955, 676], outline=(255, 176, 0, 40), width=1)
    # Corner vignette
    od.rectangle([196, 132, 955, 676], outline=(0, 0, 0, 90), width=18)
    canvas = Image.alpha_composite(canvas.convert("RGBA"), overlay).convert("RGB")
    d = ImageDraw.Draw(canvas)

    d.ellipse([1008, 772, 1028, 792], fill=AMBER)
    d.rectangle([520, 788, 632, 796], fill=(28, 26, 20))
    return canvas


def make_palette() -> Image.Image:
    w, h = 1280, 280
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)
    title = font(MONO_BOLD, 22)
    small = font(MONO, 14)
    d.text((40, 28), "FOON authored palette — not IWAD PLAYPAL", font=title, fill=AMBER)

    x0, y0, cell_w, cell_h = 40, 88, 37, 96
    for i, color in enumerate(PALETTE32):
        x = x0 + i * cell_w
        d.rectangle([x, y0, x + cell_w - 3, y0 + cell_h], fill=color)
        d.text((x + 4, y0 + cell_h + 12), f"{i:02X}", font=small, fill=INK_DIM)

    d.text(
        (40, 236),
        "32 marketing swatches · amber / phosphor / rust · original, not a PLAYPAL dump",
        font=small,
        fill=INK_DIM,
    )
    return img


def make_diagram() -> Image.Image:
    w, h = 1400, 780
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)
    title = font(MONO_BOLD, 42)
    box_title = font(MONO_BOLD, 16)
    box_body = font(MONO, 15)
    foot = font(MONO, 16)

    d.rectangle([1, 1, w - 2, h - 2], outline=LINE, width=2)
    d.text((70, 42), "FOON PIXEL PIPELINE", font=title, fill=AMBER)

    boxes = [
        ("1  LINUX DOOM 1.10", "software renderer\nr_*.c · source of\nevery pixel"),
        ("2  320×200 R8", "screens[0]\nindices stay on\nthe CPU"),
        ("3  AUTHORED LUT", "palette expand\nnearest · RGB\nafter indices"),
        ("4  INTEGER UPSCALE", "4:3 present\nnearest sample\nnot square px"),
        ("5  WEBGL CRT", "bloom / grade\nbrowser canvas\nuser IWAD only"),
    ]

    bx, by, bw, bh, gap = 48, 160, 228, 360, 28
    for i, (head, body) in enumerate(boxes):
        x = bx + i * (bw + gap)
        d.rounded_rectangle([x, by, x + bw, by + bh], radius=8, fill=PANEL, outline=AMBER, width=2)
        d.rectangle([x, by, x + bw, by + 54], fill=(28, 22, 10))
        d.text((x + 14, by + 18), head, font=box_title, fill=AMBER_HOT)
        d.multiline_text((x + 14, by + 86), body, font=box_body, fill=INK, spacing=10)
        if i < len(boxes) - 1:
            ax0 = x + bw + 4
            ax1 = x + bw + gap - 8
            mid = by + bh // 2
            d.line([(ax0, mid), (ax1, mid)], fill=AMBER, width=3)
            d.polygon([(ax1, mid), (ax1 - 10, mid - 7), (ax1 - 10, mid + 7)], fill=AMBER)

    d.text(
        (70, 580),
        "indices stay on CPU  ·  RGB and FX after expand  ·  compositor must not rewrite r_*.c",
        font=foot,
        fill=PHOSPHOR,
    )
    d.text(
        (70, 640),
        "FOON marketing diagram  ·  original / transformative  ·  no IWAD data",
        font=foot,
        fill=INK_DIM,
    )
    d.line([(70, 548), (1330, 548)], fill=LINE, width=1)
    return img


def main() -> None:
    crt = make_crt()
    pal = make_palette()
    dia = make_diagram()
    crt.save(HERE / "crt-screenshot-mock.png", "PNG")
    pal.save(HERE / "palette-strip.png", "PNG")
    dia.save(HERE / "architecture-diagram.png", "PNG")
    for name in (
        "crt-screenshot-mock.png",
        "palette-strip.png",
        "architecture-diagram.png",
    ):
        p = HERE / name
        print(p.name, p.stat().st_size, Image.open(p).format, Image.open(p).size)


if __name__ == "__main__":
    main()
