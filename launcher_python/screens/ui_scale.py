import tkinter as tk


BASE_WIDTH = 2560
BASE_HEIGHT = 1440
MIN_SCALE = 0.65
MAX_SCALE = 1.0


def compute_ui_scale(
    widget: tk.Misc,
    base_width: int = BASE_WIDTH,
    base_height: int = BASE_HEIGHT,
    min_scale: float = MIN_SCALE,
    max_scale: float = MAX_SCALE,
) -> float:
    screen_w = max(1, widget.winfo_screenwidth())
    screen_h = max(1, widget.winfo_screenheight())
    scale = min(screen_w / base_width, screen_h / base_height)
    return max(min_scale, min(max_scale, scale))


def scale_int(value: int | float, scale: float, minimum: int = 1) -> int:
    return max(minimum, int(round(value * scale)))


def scale_font(scale: float, family: str, size: int, *styles: str, min_size: int = 8):
    scaled_size = max(min_size, scale_int(size, scale, minimum=1))
    if styles:
        return (family, scaled_size, *styles)
    return (family, scaled_size)
