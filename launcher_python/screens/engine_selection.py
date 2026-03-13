import tkinter as tk
from pathlib import Path
import math

import config
from screens.engine_folder_selection import EngineFolderSelectionDialog
from screens.mode_selection import ModeSelectionScreen
from screens.ui_scale import compute_ui_scale, scale_font, scale_int


MONO = "Courier"

BG = "#1a1a1a"
BG_SURFACE = "#202020"
TEXT = "#dde2ea"
TEXT_DIM = "#7a8a99"
BORDER = "#3a3a3a"
BORDER_BRIGHT = "#dde2ea"
ACCENT = "#c0392b"
BTN_BG = "#2a2a2a"
BTN_HOVER = "#3a3a3a"


class EngineSelectionScreen:
    CARD_STYLE = {
        "normal": {"fill": "#1a1616", "outline": BORDER, "width": 2},
        "hover":  {"fill": "#221010", "outline": TEXT,   "width": 3},
        "pressed":{"fill": "#1a0a0a", "outline": ACCENT, "width": 4},
    }

    def __init__(self, root):
        self.root = root
        self.engine_path = config.ENGINE_PATH
        self.card_state = "normal"
        self.card_pressed = False
        self.ui_scale = compute_ui_scale(self.root)
        self.assets_dir = Path(__file__).resolve().parent.parent / "assets"
        self.logo_image = self._load_and_fit_image(
            "logo_v_twin.png",
            max_width=self._s(300),
            max_height=self._s(180),
        )
        self.engine_art_image = self._load_and_fit_image(
            "image_moteur_dessein.png",
            max_width=max(240, self._s(420)),
            max_height=max(180, self._s(320)),
        )
        self.card_width = self._s(760)
        self.card_height = self._s(560)

        self._ensure_large_window()
        self.root.configure(bg=BG)

        self.frame = tk.Frame(root, bg=BG)
        self.frame.pack(fill="both", expand=True)

        self.surface = tk.Frame(self.frame, bg=BG)
        self.surface.place(relx=0.5, rely=0.5, anchor="center", relwidth=0.94, relheight=0.90)

        self.surface.grid_columnconfigure(0, weight=1)
        self.surface.grid_rowconfigure(0, weight=1)
        self.surface.grid_rowconfigure(6, weight=1)

        self._build_logo_area()
        self._build_card()
        self._build_path_label()
        self._build_next_button()

        self._draw_card()

    def _s(self, value, minimum=1):
        return scale_int(value, self.ui_scale, minimum=minimum)

    def _sv(self, value):
        return int(round(value * self.ui_scale))

    def _font(self, size, *styles):
        return scale_font(self.ui_scale, MONO, size, *styles)

    def _load_and_fit_image(self, file_name, max_width, max_height):
        image_path = self.assets_dir / file_name
        if not image_path.exists():
            return None
        try:
            image = tk.PhotoImage(file=str(image_path))
        except tk.TclError:
            return None
        scale = max(image.width() / max_width, image.height() / max_height)
        sample = max(1, math.ceil(scale))
        if sample > 1:
            image = image.subsample(sample, sample)
        return image

    def _ensure_large_window(self):
        screen_w = self.root.winfo_screenwidth()
        screen_h = self.root.winfo_screenheight()
        margin_w = self._s(80)
        margin_h = self._s(120)
        target_w = min(screen_w - margin_w, max(self._s(1200), int(screen_w * 0.9)))
        target_h = min(screen_h - margin_h, max(self._s(760), int(screen_h * 0.88)))
        self.root.geometry(f"{target_w}x{target_h}+{self._s(40)}+{self._s(30)}")
        self.root.minsize(min(self._s(1100), target_w), min(self._s(700), target_h))
        if screen_w >= 1900 and screen_h >= 1050:
            try:
                self.root.state("zoomed")
            except tk.TclError:
                try:
                    self.root.attributes("-zoomed", True)
                except tk.TclError:
                    pass

    def _build_logo_area(self):
        container = tk.Frame(self.surface, bg=BG)
        container.grid(row=1, column=0, pady=(self._s(8), self._s(28)))

        if self.logo_image:
            tk.Label(container, image=self.logo_image, bg=BG).pack()
        else:
            tk.Label(
                container,
                text="V-TWIN",
                font=self._font(36, "bold"),
                fg=TEXT,
                bg=BG,
            ).pack()
            tk.Label(
                container,
                text="EPITECH EIP",
                font=self._font(10),
                fg=TEXT_DIM,
                bg=BG,
            ).pack()

    def _build_card(self):
        self.card_canvas = tk.Canvas(
            self.surface,
            width=self.card_width,
            height=self.card_height,
            bg=BG,
            bd=0,
            highlightthickness=0,
            cursor="hand2",
        )
        self.card_canvas.grid(row=3, column=0, pady=(0, self._s(12)))
        self.card_canvas.bind("<Enter>", self._on_card_enter)
        self.card_canvas.bind("<Leave>", self._on_card_leave)
        self.card_canvas.bind("<ButtonPress-1>", self._on_card_press)
        self.card_canvas.bind("<ButtonRelease-1>", self._on_card_release)

    def _build_path_label(self):
        self.path_label = tk.Label(
            self.surface,
            text=self._format_engine_label(),
            font=self._font(9),
            fg=TEXT_DIM,
            bg=BG,
        )
        self.path_label.grid(row=4, column=0, pady=(0, self._s(24)))

    def _build_next_button(self):
        btn_frame = tk.Frame(self.surface, bg=BG)
        btn_frame.grid(row=5, column=0)

        self.next_button = tk.Button(
            btn_frame,
            text="NEXT  >>",
            font=self._font(11, "bold"),
            bg=BTN_BG,
            activebackground=BTN_HOVER,
            fg=TEXT,
            activeforeground=TEXT,
            relief="flat",
            bd=0,
            padx=self._s(32),
            pady=self._s(10),
            cursor="hand2",
            command=self.go_to_mode_selection,
        )
        self.next_button.pack()
        self.next_button.bind("<Enter>", lambda _e: self.next_button.configure(bg=BTN_HOVER, fg=ACCENT))
        self.next_button.bind("<Leave>", lambda _e: self.next_button.configure(bg=BTN_BG, fg=TEXT))

    def _format_engine_label(self):
        engine = Path(self.engine_path)
        if engine.is_dir():
            return f"ENGINE FOLDER : {engine.name.upper()}"
        if engine.name:
            return f"ENGINE FILE   : {engine.name.upper()}"
        return f"PATH          : {self.engine_path.upper()}"

    def _draw_card(self):
        self.card_canvas.delete("all")
        style = self.CARD_STYLE[self.card_state]
        stroke_w = max(1, self._s(style["width"]))

        x1, y1 = self._s(6), self._s(6)
        x2, y2 = self.card_width - self._s(6), self.card_height - self._s(6)
        self._draw_border_rect(x1, y1, x2, y2, fill=style["fill"], outline=style["outline"], width=stroke_w)

        center_x = self.card_width // 2
        center_y = self.card_height // 2

        label = "CHOOSE YOUR ENGINE"
        if self.card_state == "hover":
            label = "[ CHOOSE YOUR ENGINE ]"
        elif self.card_state == "pressed":
            label = "> CHOOSE YOUR ENGINE <"

        self.card_canvas.create_text(
            center_x,
            y1 + self._s(40),
            text=label,
            fill=TEXT if self.card_state != "pressed" else ACCENT,
            font=self._font(13, "bold"),
            anchor="n",
        )

        if self.engine_art_image:
            self.card_canvas.create_image(center_x, center_y + self._s(20), image=self.engine_art_image)
        else:
            self._draw_engine_icon(center_x, center_y - self._s(30))

        engine_name = Path(self.engine_path).stem.replace("_", " ").replace("-", " ").upper()
        self.card_canvas.create_text(
            center_x,
            y2 - self._s(24),
            text=engine_name,
            fill=TEXT_DIM,
            font=self._font(9),
            anchor="s",
        )

    def _draw_border_rect(self, x1, y1, x2, y2, fill, outline, width):
        self.card_canvas.create_rectangle(x1, y1, x2, y2, fill=fill, outline=outline, width=width)
        tick = self._s(16)
        corners = [
            (x1, y1, x1 + tick, y1, x1, y1 + tick),
            (x2 - tick, y1, x2, y1, x2, y1 + tick),
            (x1, y2 - tick, x1, y2, x1 + tick, y2),
            (x2, y2 - tick, x2, y2, x2 - tick, y2),
        ]
        for pts in corners:
            self.card_canvas.create_line(*pts[:2], *pts[2:4], fill=BORDER_BRIGHT, width=self._s(2))
            self.card_canvas.create_line(*pts[2:4], *pts[4:], fill=BORDER_BRIGHT, width=self._s(2))

    def _draw_engine_icon(self, center_x, top_y):
        c = self.card_canvas
        fill = "#c8bca8"
        outline = "#7a6e5e"
        lw = max(1, self._s(2))
        c.create_rectangle(center_x - self._s(80), top_y, center_x + self._s(80), top_y + self._s(28), fill=fill, outline=outline, width=lw)
        c.create_rectangle(center_x - self._s(66), top_y + self._s(24), center_x + self._s(66), top_y + self._s(84), fill=fill, outline=outline, width=lw)
        c.create_rectangle(center_x - self._s(50), top_y + self._s(78), center_x + self._s(50), top_y + self._s(104), fill=fill, outline=outline, width=lw)
        c.create_oval(center_x - self._s(90), top_y + self._s(36), center_x - self._s(54), top_y + self._s(72), fill=fill, outline=outline, width=lw)
        c.create_oval(center_x + self._s(54), top_y + self._s(36), center_x + self._s(90), top_y + self._s(72), fill=fill, outline=outline, width=lw)
        for off in (-48, -24, 0, 24, 48):
            ox = self._sv(off)
            c.create_rectangle(center_x + ox - self._s(9), top_y + self._s(28), center_x + ox + self._s(9), top_y + self._s(72), fill="#ddd0b8", outline=outline, width=max(1, self._s(1)))

    def _set_card_state(self, state):
        if state != self.card_state:
            self.card_state = state
            self._draw_card()

    def _on_card_enter(self, _event):
        if not self.card_pressed:
            self._set_card_state("hover")

    def _on_card_leave(self, _event):
        if not self.card_pressed:
            self._set_card_state("normal")

    def _on_card_press(self, _event):
        self.card_pressed = True
        self._set_card_state("pressed")

    def _on_card_release(self, event):
        inside = 0 <= event.x <= self.card_width and 0 <= event.y <= self.card_height
        was_pressed = self.card_pressed
        self.card_pressed = False
        self._set_card_state("hover" if inside else "normal")
        if inside and was_pressed:
            self.choose_engine()

    def choose_engine(self):
        def on_confirm(engine_rel_path: str):
            self.engine_path = engine_rel_path
            self.path_label.config(text=self._format_engine_label())
            self._draw_card()

        dialog = EngineFolderSelectionDialog(
            parent=self.root,
            current_engine_path=self.engine_path,
            on_confirm=on_confirm,
        )
        self.root.wait_window(dialog)

    def go_to_mode_selection(self):
        self.frame.destroy()
        ModeSelectionScreen(self.root, self.engine_path)
