#!/usr/bin/env python3
"""
Convert a PNG/JPEG (or other Pillow-supported image) to a monochrome C header
for Adafruit_GFX::drawBitmap on SSD1306 (MSB = left pixel in each byte row).

Example:
  pip install Pillow
  python3 tools/image_to_boot_bitmap.py ~/Desktop/fox.png \\
    --width 48 --height 48 \\
    -o firmware/src/boot_logo_bitmap.h

  Poly fox (contrast + trim iStock corner + white-on-black OLED):
  python3 tools/image_to_boot_bitmap.py firmware/assets/boot_logo.png --preset boot-poly-fox

  Clean white line art on black (no line thickening — keeps strokes thin on OLED):
  python3 tools/image_to_boot_bitmap.py firmware/assets/boot_logo.png --preset clean-lines

  Heltec Wireless Tracker 160×80: --splash-ink-pad 3 crops to lit pixels +3px then scales into the panel
  (removes large borders). Optional --inset-frac <1 for extra inset; use 1.0 if splash-ink-pad is enough.
  Tight white-margin crop runs automatically unless --no-trim-white-margin:
  python3 tools/image_to_boot_bitmap.py firmware/assets/boot_logo.png --preset boot-poly-fox \\
    --width 160 --height 80 --stem TrackerBoot --canvas-fit cover --inset-frac 1 --splash-ink-pad 3 \\
    --thicken 0 --lum-cutoff 172 \\
    --trim-left 0.001 --trim-top 0.001 --trim-right 0.001 --trim-bottom 0.001 \\
    -o firmware/src/boot_logo_tracker_bitmap.h
  (0.001 trims skip stock-art fraction crop; lower --lum-cutoff = thinner lines.)

  Light fox on black PNG (red fox / black panel — binarize bright strokes):
  python3 tools/image_to_boot_bitmap.py firmware/assets/boot_logo.png --preset boot-poly-fox \\
    --paper black --light-ink-cutoff 100 --width 160 --height 80 --stem TrackerBoot \\
    --thicken 0 --canvas-fit cover \\
    -o firmware/src/boot_logo_tracker_bitmap.h
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parent
_REPO = _TOOLS.parent
_DEFAULT_OUT = _REPO / "firmware" / "src" / "boot_logo_bitmap.h"


def _require_pil():
    try:
        from PIL import Image  # noqa: WPS433
    except ImportError as e:
        print("Install Pillow: pip install Pillow", file=sys.stderr)
        raise SystemExit(1) from e
    return Image


def _flatten_rgba(im, Image, bg: tuple[int, int, int] = (255, 255, 255)):
    if im.mode == "RGBA":
        bg_im = Image.new("RGB", im.size, bg)
        bg_im.paste(im, mask=im.split()[3])
        return bg_im
    return im.convert("RGB")


def _preset_clean_white_lines(im, Image, args):
    """White strokes on black: flatten on black, optional crop, scale; caller thresholds L > cutoff."""
    from PIL import ImageEnhance

    im = _flatten_rgba(im, Image, bg=(0, 0, 0))
    if not args.no_crop_border:
        im = _crop_black_border(im, Image)
    im = ImageEnhance.Contrast(im).enhance(float(args.clean_contrast))
    if args.canvas_fit == "cover":
        return _fit_canvas_cover(im, Image, args.width, args.height, resample=Image.Resampling.LANCZOS)
    # BOX downscale keeps sharp 1-pixel transitions better than LANCZOS for line art.
    return _fit_canvas(im, Image, args.width, args.height, "black", resample=Image.Resampling.BOX)


def _crop_black_border(im, Image, tol: int = 42, step: int = 4):
    """Remove uniform near-black frame (common on stock art)."""
    rgb = im.convert("RGB")
    w, h = rgb.size
    px = rgb.load()

    def sample_not_frame(y: int, top: bool) -> bool:
        xs = range(0, w, step)
        for x in xs:
            r, g, b = px[x, y]
            if r + g + b > 3 * tol:
                return True
            if max(r, g, b) - min(r, g, b) > 35:
                return True
        return False

    top = 0
    for y in range(0, h, step):
        if sample_not_frame(y, True):
            top = max(0, y - step)
            break
    bottom = h - 1
    for y in range(h - 1, -1, -step):
        if sample_not_frame(y, False):
            bottom = min(h - 1, y + step)
            break

    left = 0
    for x in range(0, w, step):
        col_ok = False
        for y in range(0, h, step):
            r, g, b = px[x, y]
            if r + g + b > 3 * tol or max(r, g, b) - min(r, g, b) > 35:
                col_ok = True
                break
        if col_ok:
            left = max(0, x - step)
            break

    right = w - 1
    for x in range(w - 1, -1, -step):
        col_ok = False
        for y in range(0, h, step):
            r, g, b = px[x, y]
            if r + g + b > 3 * tol or max(r, g, b) - min(r, g, b) > 35:
                col_ok = True
                break
        if col_ok:
            right = min(w - 1, x + step)
            break

    if right > left + 8 and bottom > top + 8:
        return rgb.crop((left, top, right + 1, bottom + 1))
    return rgb


def _crop_white_margin(im, Image, pad_px: int = 3):
    """Shrink orange-on-white art to a tight bbox around ink (more fox after scale-to-cover)."""
    rgb = im.convert("RGB")
    w, h = rgb.size
    px = rgb.load()

    def has_ink(r: int, g: int, b: int) -> bool:
        if r + g + b < 705:
            return True
        return max(r, g, b) - min(r, g, b) > 40

    left, top = w, h
    right, bottom = -1, -1
    for yy in range(h):
        for xx in range(w):
            r, g, b = px[xx, yy]
            if has_ink(r, g, b):
                left = min(left, xx)
                right = max(right, xx)
                top = min(top, yy)
                bottom = max(bottom, yy)
    if right < 0:
        return rgb
    left = max(0, left - pad_px)
    top = max(0, top - pad_px)
    right = min(w - 1, right + pad_px)
    bottom = min(h - 1, bottom + pad_px)
    if right - left > 20 and bottom - top > 20:
        return rgb.crop((left, top, right + 1, bottom + 1))
    return rgb


def _trim_margins(im, left_frac: float, top_frac: float, right_frac: float, bottom_frac: float):
    """Crop stock ID (often bottom-left), watermark (often bottom-right), thin frame."""
    rgb = im.convert("RGB")
    w, h = rgb.size
    x0 = int(w * max(0.0, min(0.2, left_frac)))
    y0 = int(h * max(0.0, min(0.2, top_frac)))
    x1 = int(w * (1.0 - max(0.0, min(0.45, right_frac))))
    y1 = int(h * (1.0 - max(0.0, min(0.35, bottom_frac))))
    if x1 > x0 + 48 and y1 > y0 + 48:
        return rgb.crop((x0, y0, x1, y1))
    return rgb


def _fit_canvas(im, Image, w: int, h: int, pad: str, resample=None) -> Image.Image:
    """Scale image to fit inside w×h; letterbox with pad color."""
    if resample is None:
        resample = Image.Resampling.LANCZOS
    im = im.convert("RGB")
    src_w, src_h = im.size
    scale = min(w / src_w, h / src_h)
    nw = max(1, int(round(src_w * scale)))
    nh = max(1, int(round(src_h * scale)))
    im = im.resize((nw, nh), resample)
    canvas = Image.new("RGB", (w, h), pad)
    ox = (w - nw) // 2
    oy = (h - nh) // 2
    canvas.paste(im, (ox, oy))
    return canvas


def _fit_canvas_cover(im, Image, w: int, h: int, resample=None) -> Image.Image:
    """Scale image to completely cover w×h (aspect-preserving), then center-crop."""
    if resample is None:
        resample = Image.Resampling.LANCZOS
    im = im.convert("RGB")
    src_w, src_h = im.size
    scale = max(w / src_w, h / src_h)
    nw = max(1, int(round(src_w * scale)))
    nh = max(1, int(round(src_h * scale)))
    im = im.resize((nw, nh), resample)
    left = (nw - w) // 2
    top = (nh - h) // 2
    return im.crop((left, top, left + w, top + h))


def _orange_line_art_mono(im, Image, thicken: int):
    """Orange (or saturated warm) strokes on white → lit pixels; ignores light gray watermark."""
    from PIL import ImageFilter

    rgb = im.convert("RGB")
    w, h = rgb.size
    px = rgb.load()
    out = Image.new("1", (w, h), 0)
    opx = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            s = r + g + b
            if s > 745:  # near-white paper
                continue
            mx, mn = max(r, g, b), min(r, g, b)
            if mx - mn < 22 and s > 520:  # desaturated light gray (watermark)
                continue
            if s < 95:  # black border / specks
                continue
            # Orange / warm line: red channel leads, blue relatively low
            if r > 120 and r - b > 45 and g > 25 and g < r + 40:
                opx[x, y] = 1
                continue
            # Fallback: saturated mid tones (thick marker export)
            if mx - mn > 55 and r > 90 and s < 680:
                opx[x, y] = 1
    if thicken > 0:
        for _ in range(thicken):
            out = out.filter(ImageFilter.MaxFilter(3))
    return out


def _to_mono1(im, threshold: int | None, invert: bool, dither: bool):
    g = im.convert("L")
    if dither:
        mono = g.convert("1")
    else:
        t = 128 if threshold is None else max(0, min(255, threshold))
        mono = g.point(lambda p, tt=t: 255 if p >= tt else 0).convert("1")
    if invert:
        mono = mono.point(lambda p: 0 if p else 255, mode="1").convert("1")
    return mono


def _lit_bbox_mono(mono) -> tuple[int, int, int, int] | None:
    """Inclusive left, top, right, bottom of lit pixels in a mode-1 image; None if empty."""
    if mono.mode != "1":
        mono = mono.convert("1")
    w, h = mono.size
    px = mono.load()
    left, top = w, h
    right, bottom = -1, -1
    for yy in range(h):
        for xx in range(w):
            if px[xx, yy] > 0:
                left = min(left, xx)
                right = max(right, xx)
                top = min(top, yy)
                bottom = max(bottom, yy)
    if right < 0:
        return None
    return left, top, right, bottom


def _tighten_mono_splash(mono, Image, out_w: int, out_h: int, pad_px: int):
    """
    Crop to lit-pixel bbox padded by pad_px (source pixels), then uniform scale with 'contain' into
    (out_w − 2·pad) × (out_h − 2·pad), centered on an out_w × out_h black canvas — typical ~pad px margin
    to the panel edge on the limiting axis.
    """
    if pad_px <= 0:
        return mono
    bbox = _lit_bbox_mono(mono)
    if bbox is None:
        return mono
    l, t, r, b = bbox
    sw, sh = mono.size
    pad = min(pad_px, max(0, min(out_w, out_h) // 2 - 1))
    if pad <= 0 or 2 * pad >= out_w or 2 * pad >= out_h:
        return mono
    l2 = max(0, l - pad)
    t2 = max(0, t - pad)
    r2 = min(sw - 1, r + pad)
    b2 = min(sh - 1, b + pad)
    cropped = mono.crop((l2, t2, r2 + 1, b2 + 1))
    inner_w = out_w - 2 * pad
    inner_h = out_h - 2 * pad
    if inner_w < 1 or inner_h < 1:
        return mono
    cw, ch = cropped.size
    if cw < 1 or ch < 1:
        return mono
    scale = min(inner_w / cw, inner_h / ch)
    nw = max(1, int(round(cw * scale)))
    nh = max(1, int(round(ch * scale)))
    resized = cropped.resize((nw, nh), Image.Resampling.NEAREST)
    out = Image.new("1", (out_w, out_h), 0)
    ox = pad + (inner_w - nw) // 2
    oy = pad + (inner_h - nh) // 2
    out.paste(resized, (ox, oy))
    return out


def _bitmap_bytes(mono) -> tuple[int, int, list[int]]:
    assert mono.mode == "1"
    w, h = mono.size
    pixels = mono.load()
    byte_w = (w + 7) // 8
    out: list[int] = []
    for y in range(h):
        for bi in range(byte_w):
            v = 0
            for k in range(8):
                x = bi * 8 + k
                if x < w and pixels[x, y] > 0:  # 1 = lit on OLED
                    v |= 0x80 >> k
            out.append(v)
    return w, h, out


def _emit_header(w: int, h: int, bytes_: list[int], out_path: Path, source_name: str, stem: str) -> None:
    """Emit k{stem}W, k{stem}H, k{stem}[] (e.g. stem BootLogo → kBootLogoW)."""
    lines = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        f"// Auto-generated by tools/image_to_boot_bitmap.py from: {source_name}",
        "// Adafruit_GFX drawBitmap: MSB-first within each byte, row-major.",
        f"static constexpr int k{stem}W = {w};",
        f"static constexpr int k{stem}H = {h};",
        "",
        f"static const uint8_t k{stem}[] PROGMEM = {{",
    ]
    row = []
    for i, b in enumerate(bytes_):
        row.append(f"0x{b:02x}")
        if len(row) >= 12:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};")
    lines.append("")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")


def _preset_boot_poly_fox(im, Image, args):
    """High-contrast RGB, trim stock marks, scale to canvas; caller binarizes L channel."""
    from PIL import ImageEnhance

    pad = (0, 0, 0) if args.paper == "black" else (255, 255, 255)
    im = _flatten_rgba(im, Image, bg=pad)
    if not args.no_crop_border:
        im = _crop_black_border(im, Image)
    if args.paper == "white" and not args.no_trim_white_margin:
        im = _crop_white_margin(im, Image)
    im = _trim_margins(im, args.trim_left, args.trim_top, args.trim_right, args.trim_bottom)
    im = ImageEnhance.Contrast(im).enhance(float(args.contrast_boost))
    im = ImageEnhance.Color(im).enhance(float(args.saturation_boost))
    letter = "black" if args.paper == "black" else "white"
    # Letterbox (contain) vs center-crop (cover — larger subject on panel).
    inset = float(getattr(args, "inset_frac", 1.0) or 1.0)
    if inset < 0.999:
        # Scale art to inset×panel, center on full --width × --height (margins in the bitmap).
        # Without this, shrinking --width/--height + cover still fills the bitmap edge-to-edge.
        inset = max(0.15, min(1.0, inset))
        w, h = int(args.width), int(args.height)
        iw = max(8, int(round(w * inset)))
        ih = max(8, int(round(h * inset)))
        if args.canvas_fit == "cover":
            inner = _fit_canvas_cover(im, Image, iw, ih, resample=Image.Resampling.LANCZOS)
        else:
            inner = _fit_canvas(im, Image, iw, ih, letter, resample=Image.Resampling.LANCZOS)
        canvas = Image.new("RGB", (w, h), pad)
        canvas.paste(inner, ((w - iw) // 2, (h - ih) // 2))
        return canvas
    if args.canvas_fit == "cover":
        return _fit_canvas_cover(im, Image, args.width, args.height, resample=Image.Resampling.LANCZOS)
    return _fit_canvas(im, Image, args.width, args.height, letter)


def main() -> None:
    ap = argparse.ArgumentParser(description="Image → monochrome boot_logo_bitmap.h for SSD1306.")
    ap.add_argument("image", type=Path, help="Input PNG, JPEG, etc.")
    ap.add_argument(
        "--preset",
        choices=("none", "boot-poly-fox", "clean-lines"),
        default="none",
        help="boot-poly-fox: orange-on-white → white strokes on OLED. "
        "clean-lines: already white-on-black art; no dilation (thin on display).",
    )
    ap.add_argument("-o", "--output", type=Path, default=_DEFAULT_OUT, help="Output C header path")
    ap.add_argument(
        "--stem",
        type=str,
        default="BootLogo",
        metavar="NAME",
        help="C symbol stem: k{stem}W, k{stem}H, k{stem}[] (default BootLogo). "
        "Use e.g. TrackerBoot for a second asset in the same firmware tree.",
    )
    ap.add_argument("--width", type=int, default=48, help="Bitmap width (default 48)")
    ap.add_argument("--height", type=int, default=48, help="Bitmap height (default 48)")
    ap.add_argument(
        "--canvas-fit",
        choices=("contain", "cover"),
        default="contain",
        help="contain=letterbox inside width×height (default); "
        "cover=scale to fill then center-crop — subject appears larger on fixed canvas.",
    )
    ap.add_argument(
        "--inset-frac",
        type=float,
        default=1.0,
        metavar="0.15-1.0",
        help="With --preset boot-poly-fox: fit art to (inset×width)×(inset×height) then center on full "
        "width×height (paper background). Example: 0.75 = ~25%% margin; bitmap dimensions stay --width×--height.",
    )
    ap.add_argument(
        "--splash-ink-pad",
        type=int,
        default=3,
        metavar="N",
        help="With --preset boot-poly-fox: after binarization, crop to tight bbox of lit pixels padded by N px, "
        "then scale (contain) into width×height with an N-pixel black band from the panel edge (0=off).",
    )
    ap.add_argument(
        "--paper",
        choices=("white", "black"),
        default="white",
        help="Source art: white=orange/dark fox on light paper (default; mask L< --lum-cutoff). "
        "black=light fox on black (mask L> --light-ink-cutoff). On device both render as red on black.",
    )
    ap.add_argument(
        "--light-ink-cutoff",
        type=int,
        default=100,
        metavar="0-255",
        help="With --paper black: lit mask where grayscale is above this (try 80–130).",
    )
    ap.add_argument(
        "--pad",
        choices=("white", "black"),
        default="black",
        help="Letterbox pad when aspect ratio differs (default black)",
    )
    ap.add_argument(
        "--threshold",
        type=int,
        default=None,
        metavar="0-255",
        help="Luminance threshold (default 128). Ignored if --dither.",
    )
    ap.add_argument("--invert", action="store_true", help="Swap black/white after threshold")
    ap.add_argument("--dither", action="store_true", help="Floyd–Steinberg dithering (can look noisy on OLED)")
    ap.add_argument(
        "--orange-line-art",
        action="store_true",
        help="Detect saturated orange / warm strokes (poly line art on white). Ignores gray watermarks.",
    )
    ap.add_argument(
        "--no-crop-border",
        action="store_true",
        help="Skip auto-crop of near-black picture frame.",
    )
    ap.add_argument(
        "--no-trim-white-margin",
        action="store_true",
        help="With --paper white / boot-poly-fox: skip tight bbox crop around ink (keeps wide white border).",
    )
    ap.add_argument(
        "--thicken",
        type=int,
        default=0,
        metavar="N",
        help="After binarization, dilate lit pixels N times (helps thin vectors at small sizes).",
    )
    ap.add_argument(
        "--trim-left",
        type=float,
        default=0.0,
        metavar="0-0.2",
        help="With --preset boot-poly-fox: fraction of width cropped from the left.",
    )
    ap.add_argument(
        "--trim-top",
        type=float,
        default=0.0,
        metavar="0-0.2",
        help="With --preset boot-poly-fox: fraction of height cropped from the top.",
    )
    ap.add_argument(
        "--trim-right",
        type=float,
        default=0.0,
        metavar="0-0.45",
        help="With --preset boot-poly-fox: fraction of width cropped from the right (watermark).",
    )
    ap.add_argument(
        "--trim-bottom",
        type=float,
        default=0.0,
        metavar="0-0.35",
        help="With --preset boot-poly-fox: fraction of height cropped from the bottom (ID strip).",
    )
    ap.add_argument(
        "--contrast-boost",
        type=float,
        default=10.0,
        help="With --preset boot-poly-fox: ImageEnhance.Contrast factor (default 10).",
    )
    ap.add_argument(
        "--saturation-boost",
        type=float,
        default=3.0,
        help="With --preset boot-poly-fox: ImageEnhance.Color factor (default 3).",
    )
    ap.add_argument(
        "--lum-cutoff",
        type=int,
        default=218,
        metavar="0-255",
        help="With --preset boot-poly-fox: grayscale < this → lit pixel (orange→dark after contrast).",
    )
    ap.add_argument(
        "--bright-cutoff",
        type=int,
        default=118,
        metavar="0-255",
        help="With --preset clean-lines: grayscale > this → lit pixel (white stroke). Raise to thin lines.",
    )
    ap.add_argument(
        "--clean-contrast",
        type=float,
        default=1.65,
        help="With --preset clean-lines: mild contrast boost before resize (default 1.65).",
    )
    args = ap.parse_args()

    Image = _require_pil()
    if not args.image.is_file():
        raise SystemExit(f"Not a file: {args.image}")

    if args.preset == "boot-poly-fox":
        if args.dither or args.orange_line_art:
            ap.error("--preset boot-poly-fox is incompatible with --dither / --orange-line-art")
        if args.width == 48 and args.height == 48:
            args.width, args.height = 56, 44
        if (
            args.trim_left == 0.0
            and args.trim_top == 0.0
            and args.trim_right == 0.0
            and args.trim_bottom == 0.0
        ):
            args.trim_left, args.trim_top = 0.025, 0.025
            args.trim_right, args.trim_bottom = 0.14, 0.09
        # Do not auto-enable dilation: --thicken 0 keeps vector strokes thin (use --thicken 1 if needed on OLED).
        im = Image.open(args.image)
        im = _preset_boot_poly_fox(im, Image, args)
        L = im.convert("L")
        if args.paper == "black":
            bc = max(0, min(255, int(args.light_ink_cutoff)))
            mono = L.point(lambda p, bb=bc: 255 if p > bb else 0, mode="1").convert("1")
        else:
            lc = max(0, min(255, int(args.lum_cutoff)))
            mono = L.point(lambda p, ll=lc: 255 if p < ll else 0, mode="1").convert("1")
        if args.thicken > 0:
            from PIL import ImageFilter

            for _ in range(min(4, int(args.thicken))):
                mono = mono.filter(ImageFilter.MaxFilter(3))
        if int(args.splash_ink_pad) > 0:
            mono = _tighten_mono_splash(mono, Image, args.width, args.height, int(args.splash_ink_pad))
    elif args.preset == "clean-lines":
        if args.dither or args.orange_line_art:
            ap.error("--preset clean-lines is incompatible with --dither / --orange-line-art")
        if args.width == 48 and args.height == 48:
            args.width, args.height = 56, 44
        args.thicken = 0
        im = Image.open(args.image)
        im = _preset_clean_white_lines(im, Image, args)
        L = im.convert("L")
        bc = max(0, min(255, int(args.bright_cutoff)))
        mono = L.point(lambda p, bb=bc: 255 if p > bb else 0, mode="1").convert("1")
    else:
        pad = (0, 0, 0) if args.pad == "black" else (255, 255, 255)
        im = Image.open(args.image)
        im = _flatten_rgba(im, Image)
        if not args.no_crop_border:
            im = _crop_black_border(im, Image)
        im = _fit_canvas(im, Image, args.width, args.height, pad)
        if args.orange_line_art:
            if args.dither:
                ap.error("--orange-line-art is incompatible with --dither")
            mono = _orange_line_art_mono(im, Image, max(0, min(4, args.thicken)))
        else:
            mono = _to_mono1(im, args.threshold, args.invert, args.dither)
    w, h, raw = _bitmap_bytes(mono)
    src_note = f"{args.image.name} preset={args.preset}" if args.preset != "none" else args.image.name
    if not args.stem or not args.stem.isidentifier():
        raise SystemExit("--stem must be a valid C identifier (letters, digits, underscore; not starting with digit).")
    _emit_header(w, h, raw, args.output, src_note, stem=args.stem)
    print(f"Wrote {args.output} ({w}×{h}, {len(raw)} bytes)")


if __name__ == "__main__":
    main()
