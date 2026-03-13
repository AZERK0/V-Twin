import tkinter as tk
from screens.engine_selection import EngineSelectionScreen
from screens.ui_scale import compute_ui_scale, scale_int


def configure_large_window(root):
    ui_scale = compute_ui_scale(root)
    screen_w = root.winfo_screenwidth()
    screen_h = root.winfo_screenheight()

    margin_w = scale_int(80, ui_scale)
    margin_h = scale_int(120, ui_scale)
    target_w = min(screen_w - margin_w, max(scale_int(1200, ui_scale), int(screen_w * 0.9)))
    target_h = min(screen_h - margin_h, max(scale_int(760, ui_scale), int(screen_h * 0.88)))

    root.geometry(f"{target_w}x{target_h}+{scale_int(40, ui_scale)}+{scale_int(30, ui_scale)}")
    root.minsize(min(scale_int(1100, ui_scale), target_w), min(scale_int(700, ui_scale), target_h))

    if screen_w >= 1900 and screen_h >= 1050:
        try:
            root.state("zoomed")
        except tk.TclError:
            try:
                root.attributes("-zoomed", True)
            except tk.TclError:
                pass


def main():
    root = tk.Tk()
    root.title("V-Twin - Engine Launcher")
    configure_large_window(root)

    EngineSelectionScreen(root)

    root.mainloop()

if __name__ == "__main__":
    main()
