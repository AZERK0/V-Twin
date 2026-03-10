import tkinter as tk
from tkinter import filedialog
from pathlib import Path
import math

import config
from screens.mode_selection import ModeSelectionScreen


class EngineSelectionScreen:
    APP_BG = "#16181d"
    SURFACE_BG = "#1f2026"
    BUTTON_SPRITE_FILE = "engine_button_states.png"
    BUTTON_IMAGE_FILES = {
        "normal": "engine_button_normal.png",
        "hover": "engine_button_hover.png",
        "pressed": "engine_button_pressed.png",
    }
    CARD_STYLE = {
        "normal": {"fill": "#1a161b", "outline": "#1a161b", "width": 1},
        "hover": {"fill": "#221218", "outline": "#2a171f", "width": 1},
        "pressed": {"fill": "#1a0f14", "outline": "#dde2ea", "width": 2},
    }

    def __init__(self, root):
        self.root = root
        self.engine_path = config.ENGINE_PATH
        self.card_state = "normal"
        self.card_pressed = False
        self.assets_dir = Path(__file__).resolve().parent.parent / "assets"
        self.card_images = self._load_card_images()
        if self.card_images:
            self.card_width = self.card_images["normal"].width()
            self.card_height = self.card_images["normal"].height()
        else:
            self.card_width = 380
            self.card_height = 240
        self.logo_image = self._load_and_fit_image("logo_v_twin.png", max_width=170, max_height=100)
        self.engine_art_image = self._load_and_fit_image("image_moteur_dessein.png", max_width=220, max_height=120)

        self.root.geometry("980x620")
        self.root.minsize(860, 540)
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
                font=("Segoe UI Black", 48),
                fg="#4d5966",
            )
        self.logo_mark.grid(row=1, column=0, pady=(4, 0))

        self.title_label = tk.Label(
            self.surface,
            text="V-Twin",
            font=("Segoe UI", 20, "bold"),
            fg="#586372",
            bg=self.SURFACE_BG,
        )
        self.title_label.grid(row=2, column=0, pady=(0, 14))

        self.card_canvas = tk.Canvas(
            self.surface,
            width=self.card_width,
            height=self.card_height,
            bg=self.SURFACE_BG,
            bd=0,
            highlightthickness=0,
            cursor="hand2",
        )
        self.card_canvas.grid(row=3, column=0, pady=(0, 12))
        self.card_canvas.bind("<Enter>", self._on_card_enter)
        self.card_canvas.bind("<Leave>", self._on_card_leave)
        self.card_canvas.bind("<ButtonPress-1>", self._on_card_press)
        self.card_canvas.bind("<ButtonRelease-1>", self._on_card_release)

        self.path_label = tk.Label(
            self.surface,
            text=self._format_engine_label(),
            font=("Segoe UI", 10),
            fg="#a8b2bf",
            bg=self.SURFACE_BG,
        )
        self.path_label.grid(row=4, column=0, pady=(0, 14))

        self.next_button = tk.Button(
            self.surface,
            text="Next",
            font=("Segoe UI", 11, "bold"),
            bg="#2f3946",
            activebackground="#3b4858",
            fg="#f1f4f8",
            activeforeground="#ffffff",
            relief="flat",
            bd=0,
            padx=26,
            pady=8,
            cursor="hand2",
            command=self.go_to_mode_selection
        )
        self.next_button.grid(row=5, column=0)

        self._draw_card()

    def _load_card_images(self):
        images = {}
        states = ("normal", "hover", "pressed")

        for state, file_name in self.BUTTON_IMAGE_FILES.items():
            image_path = self.assets_dir / file_name
            if not image_path.exists():
                images = {}
                break
            try:
                images[state] = tk.PhotoImage(file=str(image_path))
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
            images[state] = frame

        return images

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

        x1, y1 = 8, 8
        x2, y2 = self.card_width - 8, self.card_height - 8
        self._draw_rounded_rect(
            x1,
            y1,
            x2,
            y2,
            radius=24,
            fill=style["fill"],
            outline=style["outline"],
            width=style["width"],
            smooth=True,
        )

        center_x = self.card_width // 2
        self.card_canvas.create_text(
            center_x,
            y1 + 28,
            text="Choose your Engine",
            fill="#f4f6f8",
            font=("Segoe UI", 14, "bold"),
        )
        if self.engine_art_image:
            self.card_canvas.create_image(center_x, y1 + 130, image=self.engine_art_image)
        else:
            self._draw_engine_icon(center_x, y1 + 58)

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

        c.create_rectangle(center_x - 86, top_y + 4, center_x + 86, top_y + 34, fill=fill, outline=outline, width=2)
        c.create_rectangle(center_x - 72, top_y + 30, center_x + 72, top_y + 94, fill=fill, outline=outline, width=2)
        c.create_rectangle(center_x - 56, top_y + 88, center_x + 56, top_y + 112, fill=fill, outline=outline, width=2)

        c.create_oval(center_x - 96, top_y + 44, center_x - 58, top_y + 82, fill=fill, outline=outline, width=2)
        c.create_oval(center_x + 58, top_y + 44, center_x + 96, top_y + 82, fill=fill, outline=outline, width=2)

        for offset in (-52, -26, 0, 26, 52):
            c.create_rectangle(center_x + offset - 10, top_y + 34, center_x + offset + 10, top_y + 82, fill="#e3d7c2", outline=outline, width=1)
            c.create_oval(center_x + offset - 4, top_y + 84, center_x + offset + 4, top_y + 92, fill=outline, outline=outline)

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
        start_dir = self.engine_path
        candidate = Path(self.engine_path)
        if candidate.exists():
            start_dir = str(candidate if candidate.is_dir() else candidate.parent)

        file_path = filedialog.askopenfilename(
            initialdir=start_dir,
            title="Select Engine File",
            filetypes=[("Engine script", "*.mr"), ("All files", "*.*")],
        )

        if file_path:
            self.engine_path = file_path
            self.path_label.config(text=self._format_engine_label())

    def go_to_mode_selection(self):
        self.frame.destroy()
        ModeSelectionScreen(self.root, self.engine_path)
