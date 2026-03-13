import math
import tkinter as tk
from pathlib import Path

from utils.launcher import launch_engine
from screens.ui_scale import compute_ui_scale, scale_font, scale_int


ASSETS_DIR = Path(__file__).resolve().parent.parent / "assets"


MONO = "Courier"
BG = "#1a1a1a"
BG_CARD = "#242424"
BG_CARD_HOVER = "#2e2e2e"
BG_CARD_SELECTED = "#3a1a1a"
FG = "#dde2ea"
FG_DIM = "#7a8a99"
BORDER = "#3a3a3a"
BORDER_BRIGHT = "#dde2ea"
BORDER_SELECTED = "#c0392b"
ACCENT = "#c0392b"
BTN_BG = "#2a2a2a"
BTN_HOVER = "#3a3a3a"


class ModeSelectionScreen:
    def __init__(self, root, engine_path):
        self.root = root
        self.engine_path = engine_path
        self.ui_scale = compute_ui_scale(self.root)
        self.mode = tk.StringVar(value="virtual")
        self._card_nodes = {}
        self._last_size = (0, 0)

        self.root.configure(bg=BG)

        self.frame = tk.Frame(root, bg=BG)
        self.frame.pack(fill="both", expand=True)

        self.content = tk.Frame(self.frame, bg=BG)
        self.content.place(relx=0.5, rely=0.5, anchor="center")
        self.content.columnconfigure(0, weight=1)

        self._build_header()
        self._build_cards()
        self._build_engine_info()
        self._build_actions()

        self._select_mode(self.mode.get())
        self.root.bind("<Configure>", self._on_resize)
        self.root.after(0, self._on_resize)

    def _s(self, value, minimum=1):
        return scale_int(value, self.ui_scale, minimum=minimum)

    def _font(self, size, *styles):
        return scale_font(self.ui_scale, MONO, size, *styles)

    def _on_resize(self, _event=None):
        if not self.frame.winfo_exists():
            return
        size = (self.root.winfo_width(), self.root.winfo_height())
        if size == self._last_size:
            return
        self._last_size = size
        self._layout_cards()
        self._update_wraplengths()

    def _load_logo(self):
        logo_path = ASSETS_DIR / "logo_v_twin.png"
        if not logo_path.exists():
            return None
        try:
            img = tk.PhotoImage(file=str(logo_path))
            max_w, max_h = self._s(280), self._s(168)
            factor = max(img.width() / max_w, img.height() / max_h)
            sample = max(1, math.ceil(factor))
            if sample > 1:
                img = img.subsample(sample, sample)
            return img
        except tk.TclError:
            return None

    def _build_header(self):
        header_frame = tk.Frame(self.content, bg=BG)
        header_frame.grid(row=0, column=0, pady=(0, self._s(28)), sticky="ew")
        header_frame.columnconfigure(0, weight=1)

        self._logo_image = self._load_logo()
        if self._logo_image:
            tk.Label(header_frame, image=self._logo_image, bg=BG).grid(row=0, column=0)
        else:
            tk.Label(
                header_frame,
                text="V-TWIN",
                font=self._font(28, "bold"),
                fg=FG,
                bg=BG,
            ).grid(row=0, column=0)
            tk.Label(
                header_frame,
                text="EPITECH EIP",
                font=self._font(10),
                fg=FG_DIM,
                bg=BG,
            ).grid(row=1, column=0)

        tk.Frame(header_frame, bg=BORDER, height=1).grid(row=2, column=0, sticky="ew", pady=(self._s(12), 0))

        tk.Label(
            header_frame,
            text="SELECT SIMULATION MODE",
            font=self._font(12, "bold"),
            fg=ACCENT,
            bg=BG,
        ).grid(row=3, column=0, pady=(self._s(10), 0))

        self._subtitle = tk.Label(
            header_frame,
            text="Choose how the simulator should connect and stream engine data.",
            font=self._font(9),
            fg=FG_DIM,
            bg=BG,
            justify="center",
        )
        self._subtitle.grid(row=4, column=0, pady=(self._s(4), 0))

    def _build_cards(self):
        self.card_container = tk.Frame(self.content, bg=BG)
        self.card_container.grid(row=1, column=0, sticky="ew", padx=self._s(8))
        self.card_container.columnconfigure(0, weight=1)
        self.card_container.columnconfigure(1, weight=1)

        self._create_mode_card(
            mode="virtual",
            title="FULL VIRTUAL",
            subtitle="Run entirely in simulation with no external input.",
            bullets=[
                "Ideal for tuning and testing",
                "Zero hardware required",
                "Fast startup with presets",
            ],
        )

        self._create_mode_card(
            mode="twin",
            title="TWIN OBD",
            subtitle="Pair with an OBD bridge and stream live data.",
            bullets=[
                "Realtime sensor feed",
                "Blended sim + live telemetry",
                "Use when hardware is attached",
            ],
        )

    def _build_engine_info(self):
        info_frame = tk.Frame(self.content, bg=BG)
        info_frame.grid(row=2, column=0, pady=(self._s(16), 0), sticky="ew")
        info_frame.columnconfigure(0, weight=1)

        tk.Frame(info_frame, bg=BORDER, height=1).grid(row=0, column=0, sticky="ew", pady=(0, self._s(6)))

        from pathlib import Path
        engine_name = Path(self.engine_path).stem.replace("_", " ").replace("-", " ").upper()
        self._engine_label = tk.Label(
            info_frame,
            text=f"ENGINE : {engine_name}",
            font=self._font(9),
            fg=FG_DIM,
            bg=BG,
        )
        self._engine_label.grid(row=1, column=0)

    def _build_actions(self):
        action_frame = tk.Frame(self.content, bg=BG)
        action_frame.grid(row=3, column=0, pady=(self._s(20), 0))

        self.start_button = tk.Button(
            action_frame,
            text="START SIMULATION",
            font=self._font(11, "bold"),
            bg=ACCENT,
            activebackground="#a93226",
            fg=FG,
            activeforeground=FG,
            relief="flat",
            bd=0,
            padx=self._s(36),
            pady=self._s(12),
            cursor="hand2",
            command=self.start_simulation,
        )
        self.start_button.pack()
        self.start_button.bind("<Enter>", lambda _e: self.start_button.configure(bg="#e74c3c"))
        self.start_button.bind("<Leave>", lambda _e: self.start_button.configure(bg=ACCENT))

        tk.Label(
            action_frame,
            text="TIP : YOU CAN SWITCH MODES BEFORE LAUNCH",
            font=self._font(8),
            fg=FG_DIM,
            bg=BG,
        ).pack(pady=(self._s(8), 0))

    def _create_mode_card(self, mode, title, subtitle, bullets):
        frame = tk.Frame(
            self.card_container,
            bg=BG_CARD,
            bd=0,
            highlightthickness=2,
            highlightbackground=BORDER,
        )

        header = tk.Frame(frame, bg=BG_CARD)
        header.pack(fill="x", padx=self._s(16), pady=(self._s(14), self._s(6)))

        indicator = tk.Canvas(
            header,
            width=self._s(14),
            height=self._s(14),
            highlightthickness=0,
            bg=BG_CARD,
        )
        dot = indicator.create_oval(
            self._s(1), self._s(1),
            self._s(13), self._s(13),
            outline=FG_DIM,
            width=self._s(2),
            fill=BG_CARD,
        )
        indicator.pack(side="left", padx=(0, self._s(10)))

        title_label = tk.Label(
            header,
            text=title,
            font=self._font(13, "bold"),
            fg=FG,
            bg=BG_CARD,
        )
        title_label.pack(side="left")

        subtitle_label = tk.Label(
            frame,
            text=subtitle,
            font=self._font(9),
            fg=FG_DIM,
            bg=BG_CARD,
            justify="left",
            wraplength=self._s(360),
        )
        subtitle_label.pack(fill="x", padx=self._s(16), pady=(0, self._s(6)))

        bullet_text = "\n".join([f"- {item.upper()}" for item in bullets])
        bullet_label = tk.Label(
            frame,
            text=bullet_text,
            font=self._font(9),
            fg=FG,
            bg=BG_CARD,
            justify="left",
            wraplength=self._s(360),
        )
        bullet_label.pack(fill="x", padx=self._s(16), pady=(0, self._s(16)))

        widgets = [frame, header, title_label, subtitle_label, bullet_label, indicator]
        for widget in widgets:
            widget.bind("<Button-1>", lambda _e, m=mode: self._select_mode(m))
            widget.bind("<Enter>", lambda _e, m=mode: self._on_card_enter(m))
            widget.bind("<Leave>", lambda _e, m=mode: self._on_card_leave(m))

        self._card_nodes[mode] = {
            "frame": frame,
            "widgets": widgets,
            "indicator": indicator,
            "dot": dot,
            "title": title_label,
            "subtitle": subtitle_label,
            "bullets": bullet_label,
        }

    def _layout_cards(self):
        if not getattr(self, "card_container", None):
            return
        width = max(1, self.root.winfo_width())
        use_single_column = width < self._s(900)

        for node in self._card_nodes.values():
            node["frame"].grid_forget()

        if use_single_column:
            for row_idx, mode in enumerate(("virtual", "twin")):
                if mode in self._card_nodes:
                    self._card_nodes[mode]["frame"].grid(
                        row=row_idx, column=0, sticky="ew",
                        padx=self._s(12), pady=self._s(8),
                    )
            self.card_container.columnconfigure(0, weight=1)
        else:
            for col_idx, mode in enumerate(("virtual", "twin")):
                if mode in self._card_nodes:
                    self._card_nodes[mode]["frame"].grid(
                        row=0, column=col_idx, sticky="nsew",
                        padx=self._s(10), pady=self._s(8),
                    )
            self.card_container.columnconfigure(0, weight=1)
            self.card_container.columnconfigure(1, weight=1)

    def _update_wraplengths(self):
        wrap = max(self._s(320), int(self.root.winfo_width() * 0.35))
        if hasattr(self, "_subtitle"):
            self._subtitle.configure(wraplength=max(self._s(520), int(self.root.winfo_width() * 0.55)))
        for node in self._card_nodes.values():
            node["subtitle"].configure(wraplength=wrap)
            node["bullets"].configure(wraplength=wrap)

    def _on_card_enter(self, mode):
        if self.mode.get() == mode:
            return
        self._apply_card_style(mode, state="hover")

    def _on_card_leave(self, mode):
        if self.mode.get() == mode:
            return
        self._apply_card_style(mode, state="normal")

    def _select_mode(self, mode):
        self.mode.set(mode)
        for key in self._card_nodes:
            self._apply_card_style(key, state="selected" if key == mode else "normal")

    def _apply_card_style(self, mode, state="normal"):
        node = self._card_nodes.get(mode)
        if not node:
            return

        if state == "selected":
            bg = BG_CARD_SELECTED
            border = BORDER_SELECTED
            fg_title = FG
            fg_text = FG
            dot_fill = ACCENT
            dot_outline = ACCENT
        elif state == "hover":
            bg = BG_CARD_HOVER
            border = BORDER_BRIGHT
            fg_title = FG
            fg_text = FG
            dot_fill = BG_CARD_HOVER
            dot_outline = FG_DIM
        else:
            bg = BG_CARD
            border = BORDER
            fg_title = FG
            fg_text = FG_DIM
            dot_fill = BG_CARD
            dot_outline = FG_DIM

        node["frame"].configure(bg=bg, highlightbackground=border)
        for widget in node["widgets"]:
            widget.configure(bg=bg)  # type: ignore[call-arg]
        node["title"].configure(fg=fg_title)
        node["subtitle"].configure(fg=fg_text)
        node["bullets"].configure(fg=FG)
        node["indicator"].itemconfig(node["dot"], fill=dot_fill, outline=dot_outline)

    def start_simulation(self):
        launch_engine(self.engine_path, self.mode.get())
