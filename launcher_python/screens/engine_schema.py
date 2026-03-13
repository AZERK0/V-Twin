import math
import re
import tkinter as tk
from pathlib import Path
from typing import List, Optional, Tuple


def _resolve_mr_path(engine_path: str) -> Optional[Path]:
    root = Path(__file__).resolve().parents[2]
    p = Path(engine_path)
    absolute = p if p.is_absolute() else root / p
    if absolute.is_file() and absolute.suffix == ".mr":
        return absolute
    if absolute.is_dir():
        files = sorted(absolute.rglob("*.mr"))
        return files[0] if files else None
    return None


def _eval_angle_expr(expr: str) -> Optional[float]:
    expr = expr.strip()
    expr = re.sub(r"\*\s*units\.deg", "", expr)
    expr = re.sub(r"\*\s*units\.[a-z_]+", "", expr)
    expr = re.sub(r"[a-z_][a-z_0-9]*\s*/\s*[0-9.]+\s*\*\s*360", lambda m: _handle_radial_expr(m.group(0)), expr)

    try:
        result = eval(expr, {"__builtins__": {}}, {})  # noqa: S307
        return float(result)
    except Exception:
        return None


def _handle_radial_expr(expr: str) -> str:
    m = re.match(r"\(?\s*([0-9.]+)\s*/\s*([0-9.]+)\s*\)?\s*\*\s*360", expr)
    if m:
        return str(float(m.group(1)) / float(m.group(2)) * 360)
    return expr


def parse_engine_banks(engine_path: str) -> List[Tuple[float, int]]:
    path = _resolve_mr_path(engine_path)
    if not path:
        return []

    try:
        source = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return []

    v_angle = _extract_v_angle(source)

    banks_raw = _extract_banks(source, v_angle)
    if not banks_raw:
        return []

    cyl_per_bank = _count_cylinders_per_bank(source, banks_raw)

    return [(angle_deg, count) for angle_deg, count in cyl_per_bank if count > 0]


def _extract_v_angle(source: str) -> Optional[float]:
    m = re.search(r"label\s+v_angle\s*\(\s*([0-9.]+)\s*\*\s*units\.deg\s*\)", source)
    if m:
        return float(m.group(1))
    return None


def _extract_banks(source: str, v_angle: Optional[float]) -> List[Tuple[str, float, int]]:
    line_pattern = re.compile(r"^[^\S\n]*cylinder_bank\s+(b\d+)\s*\((.+)\)\s*$", re.MULTILINE)
    banks = []
    for m in line_pattern.finditer(source):
        name = m.group(1)
        params = m.group(2)
        angle_m = re.search(r"\bangle\s*:\s*(.+?)(?:,\s*\w|\s*$)", params)
        if not angle_m:
            continue
        raw_angle = angle_m.group(1).strip()
        angle = _resolve_angle(raw_angle, v_angle)
        if angle is not None:
            banks.append((name, angle, m.start()))
    return banks


def _resolve_angle(raw: str, v_angle: Optional[float]) -> Optional[float]:
    raw = raw.strip()

    if "v_angle" in raw and v_angle is not None:
        raw = raw.replace("v_angle", str(v_angle))

    m = re.match(r"\(\s*([0-9.]+)\s*/\s*([0-9.]+)\s*\)\s*\*\s*360(?:\s*\*\s*units\.deg)?", raw)
    if m:
        return float(m.group(1)) / float(m.group(2)) * 360.0

    clean = re.sub(r"\*?\s*units\.deg", "", raw)
    clean = re.sub(r"\*?\s*units\.[a-z_]+", "", clean)
    try:
        return float(eval(clean, {"__builtins__": {}}, {}))  # noqa: S307
    except Exception:
        return None


def _count_cylinders_per_bank(
    source: str, banks: List[Tuple[str, float, int]]
) -> List[Tuple[float, int]]:
    add_cyl_positions = [m.start() for m in re.finditer(r"\.add_cylinder\(", source)]

    bank_chain_starts = _find_bank_chain_starts(source, [name for name, _, _ in banks])

    result = []
    chain_positions = sorted(bank_chain_starts.items(), key=lambda x: x[1])

    for i, (name, chain_start) in enumerate(chain_positions):
        chain_end = chain_positions[i + 1][1] if i + 1 < len(chain_positions) else len(source)
        count = sum(1 for pos in add_cyl_positions if chain_start <= pos < chain_end)

        angle = next(a for n, a, _ in banks if n == name)
        if count == 0:
            count = 1
        result.append((angle, count))

    return result


def _find_bank_chain_starts(source: str, bank_names: List[str]) -> dict:
    starts = {}
    for name in bank_names:
        pattern = re.compile(r"^\s*" + re.escape(name) + r"\s*$", re.MULTILINE)
        for m in pattern.finditer(source):
            starts[name] = m.start()
            break
    return starts


def draw_engine_schema(
    canvas: tk.Canvas,
    engine_path: str,
    cx: float,
    cy: float,
    size: float,
    fg: str = "#dde2ea",
    dim: str = "#5a6a79",
    accent: str = "#c0392b",
) -> None:
    banks = parse_engine_banks(engine_path)
    if not banks:
        _draw_fallback(canvas, cx, cy, size, fg, dim)
        return

    total_cylinders = sum(c for _, c in banks)
    num_banks = len(banks)

    is_radial = num_banks >= 4 and all(abs(banks[i][1] - banks[i - 1][1] - 360.0 / num_banks) < 5 for i in range(1, num_banks))
    is_inline = num_banks == 1
    is_flat = num_banks == 2 and abs(abs(banks[0][0] - banks[1][0]) - 180) < 5

    if is_radial:
        _draw_radial(canvas, banks, cx, cy, size, fg, dim, accent)
    elif is_inline:
        _draw_inline(canvas, banks[0], cx, cy, size, fg, dim, accent)
    elif is_flat:
        _draw_flat(canvas, banks, cx, cy, size, fg, dim, accent)
    else:
        _draw_v(canvas, banks, cx, cy, size, fg, dim, accent)

    _draw_label(canvas, engine_path, cx, cy, size, dim)


def _cyl_radius(size: float, count: int) -> float:
    return max(3.0, min(10.0, size * 0.06 * (6 / max(count, 3))))


def _draw_inline(canvas, bank, cx, cy, size, fg, dim, accent):
    angle_deg, count = bank
    angle_rad = math.radians(angle_deg - 90)
    perp = angle_rad + math.pi / 2

    spacing = min(size * 0.14, size * 0.9 / max(count, 1))
    total_len = spacing * (count - 1)
    r = _cyl_radius(size, count)
    block_w = size * 0.06
    block_h = total_len + r * 3

    bx = math.cos(angle_rad)
    by = math.sin(angle_rad)
    px = math.cos(perp)
    py = math.sin(perp)

    half = total_len / 2
    corners = [
        (cx + px * half + bx * block_w, cy + py * half + by * block_w),
        (cx - px * half + bx * block_w, cy - py * half + by * block_w),
        (cx - px * half - bx * block_w, cy - py * half - by * block_w),
        (cx + px * half - bx * block_w, cy + py * half - by * block_w),
    ]
    pts = [coord for pt in corners for coord in pt]
    canvas.create_polygon(pts, fill=dim, outline=fg, width=1, smooth=False)

    for i in range(count):
        t = -half + i * spacing
        x = cx + px * t
        y = cy + py * t
        canvas.create_oval(x - r, y - r, x + r, y + r, fill=accent, outline=fg, width=1)


def _draw_v(canvas, banks, cx, cy, size, fg, dim, accent):
    for angle_deg, count in banks:
        angle_rad = math.radians(angle_deg - 90)
        perp = angle_rad + math.pi / 2
        bank_offset = size * 0.12

        spacing = min(size * 0.13, size * 0.75 / max(count, 1))
        total_len = spacing * (count - 1)
        r = _cyl_radius(size, count)

        ox = cx + math.cos(angle_rad) * bank_offset
        oy = cy + math.sin(angle_rad) * bank_offset

        px = math.cos(perp)
        py = math.sin(perp)

        half = total_len / 2
        bw = size * 0.055
        corners = [
            (ox + px * half + math.cos(angle_rad) * bw, oy + py * half + math.sin(angle_rad) * bw),
            (ox - px * half + math.cos(angle_rad) * bw, oy - py * half + math.sin(angle_rad) * bw),
            (ox - px * half - math.cos(angle_rad) * bw, oy - py * half - math.sin(angle_rad) * bw),
            (ox + px * half - math.cos(angle_rad) * bw, oy + py * half - math.sin(angle_rad) * bw),
        ]
        pts = [coord for pt in corners for coord in pt]
        canvas.create_polygon(pts, fill=dim, outline=fg, width=1)

        for i in range(count):
            t = -half + i * spacing
            x = ox + px * t
            y = oy + py * t
            canvas.create_oval(x - r, y - r, x + r, y + r, fill=accent, outline=fg, width=1)

    canvas.create_oval(cx - size * 0.06, cy - size * 0.06, cx + size * 0.06, cy + size * 0.06,
                       fill="#3a3a3a", outline=fg, width=1)


def _draw_flat(canvas, banks, cx, cy, size, fg, dim, accent):
    _draw_v(canvas, banks, cx, cy, size, fg, dim, accent)


def _draw_radial(canvas, banks, cx, cy, size, fg, dim, accent):
    num_banks = len(banks)
    ring_r = size * 0.32
    r = _cyl_radius(size, num_banks)

    canvas.create_oval(cx - size * 0.08, cy - size * 0.08, cx + size * 0.08, cy + size * 0.08,
                       fill=dim, outline=fg, width=1)

    for i, (angle_deg, _) in enumerate(banks):
        angle_rad = math.radians(angle_deg - 90)
        x = cx + math.cos(angle_rad) * ring_r
        y = cy + math.sin(angle_rad) * ring_r

        rod_x1 = cx + math.cos(angle_rad) * size * 0.09
        rod_y1 = cy + math.sin(angle_rad) * size * 0.09
        canvas.create_line(rod_x1, rod_y1, x, y, fill=dim, width=max(1, int(r * 0.6)))

        canvas.create_oval(x - r, y - r, x + r, y + r, fill=accent, outline=fg, width=1)


def _draw_fallback(canvas, cx, cy, size, fg, dim):
    r = size * 0.3
    canvas.create_oval(cx - r, cy - r, cx + r, cy + r, fill=dim, outline=fg, width=1)
    canvas.create_text(cx, cy, text="?", fill=fg, font=("Courier", int(size * 0.2), "bold"))


def _draw_label(canvas, engine_path: str, cx, cy, size, dim):
    name = Path(engine_path).stem.replace("_", " ").replace("-", " ").upper()
    canvas.create_text(cx, cy + size * 0.52, text=name, fill=dim,
                       font=("Courier", max(7, int(size * 0.055))), anchor="n")
