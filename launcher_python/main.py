import tkinter as tk
from screens.engine_selection import EngineSelectionScreen


def configure_large_window(root):
    screen_w = root.winfo_screenwidth()
    screen_h = root.winfo_screenheight()

    target_w = min(screen_w - 80, max(1200, int(screen_w * 0.9)))
    target_h = min(screen_h - 120, max(760, int(screen_h * 0.88)))

    root.geometry(f"{target_w}x{target_h}+40+30")
    root.minsize(min(1100, target_w), min(700, target_h))

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
