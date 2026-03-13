import tkinter as tk
from pathlib import Path
import math

import config
from screens.engine_folder_selection import EngineFolderSelectionDialog
from screens.mode_selection import ModeSelectionScreen
from screens.ui_scale import compute_ui_scale, scale_font, scale_int


class EngineSelectionScreen:
    APP_BG = "#252323"
    SURFACE_BG = "#252323"
    BUTTON_SPRITE_FILE = "engine_button_states.png"
    BUTTON_IMAGE_FILES = {
        "normal": "engine_button_normal.png",
        "hover": "engine_button_hover.png",
        "pressed": "engine_button_pressed.png",
    }
    CARD_STYLE = {
        "normal": {"fill": "#1a161b", "outline": "#dde2ea", "width": 3},
        "hover": {"fill": "#221218", "outline": "#dde2ea", "width": 10},
        "pressed": {"fill": "#1a0f14", "outline": "#dde2ea", "width": 18},
    }

    def __init__(self, root):
        self.root = root
        self.engine_path = config.ENGINE_PATH
        self.card_state = "normal"
        self.card_pressed = False
        self.ui_scale = compute_ui_scale(self.root)
        self.assets_dir = Path(__file__).resolve().parent.parent / "assets"
        self.card_images = self._load_card_images()
        if self.card_images:
            self.card_width = self.card_images["normal"].width()
            self.card_height = self.card_images["normal"].height()
        else:
            self.card_width = self._s(780)
            self.card_height = self._s(640)
        self.logo_image = self._load_and_fit_image(
            "logo_v_twin.png",
            max_width=max(320, self._s(870)),
            max_height=max(260, self._s(750)),
        )
        self.engine_art_image = self._load_and_fit_image(
            "image_moteur_dessein.png",
            max_width=max(240, self._s(420)),
            max_height=max(180, self._s(320)),
        )

        self._ensure_large_window()
        self.root.configure(bg=self.APP_BG)

        self.frame = tk.Frame(root, bg=self.APP_BG)
        self.frame.pack(fill="both", expand=True)

        self.surface = tk.Frame(self.frame, bg=self.SURFACE_BG)
        self.surface.place(relx=0.5, rely=0.5, anchor="center", relwidth=0.94, relheight=0.86)

        self.surface.grid_columnconfigure(0, weight=1)
        self.surface.grid_rowconfigure(0, weight=1)
        self.surface.grid_rowconfigure(6, weight=1)

        self.logo_mark = tk.Label(self.surface, bg=self.SURFACE_BG)
        if self.logo_image:
            self.logo_mark.config(image=self.logo_image)
        else:
            self.logo_mark.config(
                text="V",
                font=self._font("Segoe UI Black", 48),
                fg="#4d5966",
            )
        self.logo_mark.grid(row=1, column=0, pady=(self._s(4), self._s(200)))

        self.card_canvas = tk.Canvas(
            self.surface,
            width=self.card_width,
            height=self.card_height,
            bg=self.SURFACE_BG,
            bd=0,
            highlightthickness=0,
            cursor="hand2",
        )
        self.card_canvas.grid(row=3, column=0, pady=(0, self._s(12)))
        self.card_canvas.bind("<Enter>", self._on_card_enter)
        self.card_canvas.bind("<Leave>", self._on_card_leave)
        self.card_canvas.bind("<ButtonPress-1>", self._on_card_press)
        self.card_canvas.bind("<ButtonRelease-1>", self._on_card_release)

        self.path_label = tk.Label(
            self.surface,
            text=self._format_engine_label(),
            font=self._font("Segoe UI", 10),
            fg="#a8b2bf",
            bg=self.SURFACE_BG,
        )
        self.path_label.grid(row=4, column=0, pady=(0, self._s(84)))

        self.next_button = tk.Button(
            self.surface,
            text="Next",
            font=self._font("Segoe UI", 11, "bold"),
            bg="#2f3946",
            activebackground="#3b4858",
            fg="#f1f4f8",
            activeforeground="#ffffff",
            relief="flat",
            bd=0,
            height=max(1, self._s(2, minimum=1)),
            width=max(8, self._s(10, minimum=1)),
            padx=self._s(26),
            pady=self._s(8),
            cursor="hand2",
            command=self.go_to_mode_selection
        )
        self.next_button.grid(row=5, column=0)

        self._draw_card()

    def _s(self, value, minimum=1):
        return scale_int(value, self.ui_scale, minimum=minimum)

    def _sv(self, value):
        return int(round(value * self.ui_scale))

    def _font(self, family, size, *styles):
        return scale_font(self.ui_scale, family, size, *styles)

    def _load_card_images(self):
        images = {}
        states = ("normal", "hover", "pressed")
        max_card_w = max(420, self._s(780))
        max_card_h = max(320, self._s(640))

        for state, file_name in self.BUTTON_IMAGE_FILES.items():
            image_path = self.assets_dir / file_name
            if not image_path.exists():
                images = {}
                break
            try:
                image = tk.PhotoImage(file=str(image_path))
                images[state] = self._fit_image_to_bounds(image, max_card_w, max_card_h)
            except tk.TclError:
                images = {}
                break

        if images:
            return images

        sprite_path = self.assets_dir / self.BUTTON_SPRITE_FILE
        if not sprite_path.exists():
            return {}

        try:
            sprite = tk.PhotoImage(file=str(sprite_path))
        except tk.TclError:
            return {}

        if sprite.width() < 3 or sprite.width() % 3 != 0:
            return {}

        frame_width = sprite.width() // 3
        frame_height = sprite.height()
        self._sprite_source = sprite

        for index, state in enumerate(states):
            frame = tk.PhotoImage(width=frame_width, height=frame_height)
            frame.tk.call(
                frame,
                "copy",
                sprite,
                "-from",
                index * frame_width,
                0,
                (index + 1) * frame_width,
                frame_height,
                "-to",
                0,
                0,
            )
            images[state] = self._fit_image_to_bounds(frame, max_card_w, max_card_h)

        return images

    def _fit_image_to_bounds(self, image, max_width, max_height):
        factor = max(image.width() / max_width, image.height() / max_height)
        sample = max(1, math.ceil(factor))
        if sample > 1:
            image = image.subsample(sample, sample)
        return image

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

    def _format_engine_label(self):
        engine = Path(self.engine_path)
        if engine.is_dir():
            return f"Current engine folder: {engine.name}"
        if engine.name:
            return f"Selected engine file: {engine.name}"
        return f"Selected path: {self.engine_path}"

    def _draw_card(self):
        self.card_canvas.delete("all")
        if self.card_images:
            self.card_canvas.create_image(
                self.card_width // 2,
                self.card_height // 2,
                image=self.card_images[self.card_state],
            )
            return

        style = self.CARD_STYLE[self.card_state]
        stroke_w = max(1, self._s(style["width"]))

        x1, y1 = self._s(8), self._s(8)
        x2, y2 = self.card_width - self._s(8), self.card_height - self._s(8)
        self._draw_rounded_rect(
            x1,
            y1,
            x2,
            y2,
            radius=self._s(200),
            fill=style["fill"],
            outline=style["outline"],
            width=stroke_w,
            smooth=True,
        )

        center_x = self.card_width // 2
        self.card_canvas.create_text(
            center_x,
            y1 + self._s(178),
            text="Choose your Engine",
            fill="#f4f6f8",
            font=self._font("Segoe UI", 14, "bold"),
        )
        if self.engine_art_image:
            self.card_canvas.create_image(center_x, y1 + self._s(430), image=self.engine_art_image)
        else:
            self._draw_engine_icon(center_x, y1 + self._s(58))

    def _draw_rounded_rect(self, x1, y1, x2, y2, radius=20, **kwargs):
        points = [
            x1 + radius, y1,
            x1 + radius, y1,
            x2 - radius, y1,
            x2 - radius, y1,
            x2, y1,
            x2, y1 + radius,
            x2, y1 + radius,
            x2, y2 - radius,
            x2, y2 - radius,
            x2, y2,
            x2 - radius, y2,
            x2 - radius, y2,
            x1 + radius, y2,
            x1 + radius, y2,
            x1, y2,
            x1, y2 - radius,
            x1, y2 - radius,
            x1, y1 + radius,
            x1, y1 + radius,
            x1, y1,
        ]
        self.card_canvas.create_polygon(points, splinesteps=24, **kwargs)

    def _draw_engine_icon(self, center_x, top_y):
        c = self.card_canvas
        fill = "#d8ccb8"
        outline = "#8a7c6a"
        line_w = max(1, self._s(2))
        inner_line_w = max(1, self._s(1))

        c.create_rectangle(
            center_x - self._s(86),
            top_y + self._s(4),
            center_x + self._s(86),
            top_y + self._s(34),
            fill=fill,
            outline=outline,
            width=line_w,
        )
        c.create_rectangle(
            center_x - self._s(72),
            top_y + self._s(30),
            center_x + self._s(72),
            top_y + self._s(94),
            fill=fill,
            outline=outline,
            width=line_w,
        )
        c.create_rectangle(
            center_x - self._s(56),
            top_y + self._s(88),
            center_x + self._s(56),
            top_y + self._s(112),
            fill=fill,
            outline=outline,
            width=line_w,
        )

        c.create_oval(
            center_x - self._s(96),
            top_y + self._s(44),
            center_x - self._s(58),
            top_y + self._s(82),
            fill=fill,
            outline=outline,
            width=line_w,
        )
        c.create_oval(
            center_x + self._s(58),
            top_y + self._s(44),
            center_x + self._s(96),
            top_y + self._s(82),
            fill=fill,
            outline=outline,
            width=line_w,
        )

        for offset in (-52, -26, 0, 26, 52):
            signed_offset = self._sv(offset)
            c.create_rectangle(
                center_x + signed_offset - self._s(10),
                top_y + self._s(34),
                center_x + signed_offset + self._s(10),
                top_y + self._s(82),
                fill="#e3d7c2",
                outline=outline,
                width=inner_line_w,
            )
            c.create_oval(
                center_x + signed_offset - self._s(4),
                top_y + self._s(84),
                center_x + signed_offset + self._s(4),
                top_y + self._s(92),
                fill=outline,
                outline=outline,
            )

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

        dialog = EngineFolderSelectionDialog(
            parent=self.root,
            current_engine_path=self.engine_path,
            on_confirm=on_confirm,
        )
        self.root.wait_window(dialog)

    def go_to_mode_selection(self):
        self.frame.destroy()
        ModeSelectionScreen(self.root, self.engine_path)
