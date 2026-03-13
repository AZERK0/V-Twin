import tkinter as tk

from utils.launcher import launch_engine
from screens.ui_scale import compute_ui_scale, scale_font, scale_int


class ModeSelectionScreen:
    BG = "#0b0f14"
    SURFACE = "#111827"
    SURFACE_HOVER = "#182235"
    SURFACE_ACTIVE = "#1b2a3f"
    BORDER = "#2b364a"
    BORDER_HOVER = "#3a4558"
    BORDER_ACTIVE = "#f97316"
    TEXT = "#e2e8f0"
    TEXT_MUTED = "#94a3b8"
    ACCENT = "#f97316"
    ACCENT_DARK = "#c2410c"

    def __init__(self, root, engine_path):
        self.root = root
        self.engine_path = engine_path
        self.ui_scale = compute_ui_scale(self.root)
        self.mode = tk.StringVar(value="virtual")
        self._card_nodes = {}
        self._last_size = (0, 0)

        self.root.configure(bg=self.BG)

        self.frame = tk.Frame(root, bg=self.BG)
        self.frame.pack(fill="both", expand=True)

        self.bg_canvas = tk.Canvas(self.frame, bg=self.BG, highlightthickness=0, bd=0)
        self.bg_canvas.place(relx=0, rely=0, relwidth=1, relheight=1)

        self.content = tk.Frame(self.frame, bg=self.BG)
        self.content.place(relx=0.5, rely=0.5, anchor="center")
        self.content.columnconfigure(0, weight=1)

        self._build_header()
        self._build_cards()
        self._build_actions()

        self._select_mode(self.mode.get())
        self.root.bind("<Configure>", self._on_resize)
        self.root.after(0, self._on_resize)

    def _s(self, value, minimum=1):
        return scale_int(value, self.ui_scale, minimum=minimum)

    def _font(self, family, size, *styles):
        return scale_font(self.ui_scale, family, size, *styles)

    def _on_resize(self, _event=None):
        if not self.frame.winfo_exists():
            return
        size = (self.root.winfo_width(), self.root.winfo_height())
        if size == self._last_size:
            return
        self._last_size = size
        self._draw_background()
        self._layout_cards()
        self._update_wraplengths()

    def _draw_background(self):
        width = max(1, self.root.winfo_width())
        height = max(1, self.root.winfo_height())
        if width < 20 or height < 20:
            return
        self.bg_canvas.delete("all")
        self.bg_canvas.create_rectangle(0, 0, width, height, fill=self.BG, outline="")
        self.bg_canvas.create_oval(
            int(width * 0.55),
            -int(height * 0.25),
            int(width * 1.2),
            int(height * 0.55),
            fill="#172033",
            outline="",
        )
        self.bg_canvas.create_oval(
            -int(width * 0.2),
            int(height * 0.5),
            int(width * 0.4),
            int(height * 1.2),
            fill="#121a27",
            outline="",
        )
        self.bg_canvas.create_rectangle(
            0,
            int(height * 0.68),
            width,
            height,
            fill="#0d131c",
            outline="",
        )

    def _build_header(self):
        eyebrow = tk.Label(
            self.content,
            text="V-Twin Launcher",
            font=self._font("Fira Sans", 10, "bold"),
            fg=self.ACCENT,
            bg=self.BG,
        )
        eyebrow.grid(row=0, column=0, pady=(0, self._s(6)))

        title = tk.Label(
            self.content,
            text="Choose your drive mode",
            font=self._font("DejaVu Serif", 22, "bold"),
            fg=self.TEXT,
            bg=self.BG,
        )
        title.grid(row=1, column=0, pady=(0, self._s(8)))

        subtitle = tk.Label(
            self.content,
            text="Pick how the simulator should connect and stream engine data.",
            font=self._font("Fira Sans", 11),
            fg=self.TEXT_MUTED,
            bg=self.BG,
            justify="center",
        )
        subtitle.grid(row=2, column=0, pady=(0, self._s(18)))
        self._subtitle = subtitle

    def _build_cards(self):
        self.card_container = tk.Frame(self.content, bg=self.BG)
        self.card_container.grid(row=3, column=0, sticky="ew", padx=self._s(8))
        self.card_container.columnconfigure(0, weight=1)
        self.card_container.columnconfigure(1, weight=1)

        self._create_mode_card(
            mode="virtual",
            title="Full Virtual",
            subtitle="Run entirely in simulation with no external input.",
            bullets=[
                "Ideal for tuning and testing",
                "Zero hardware required",
                "Fast startup with presets",
            ],
        )

        self._create_mode_card(
            mode="twin",
            title="Twin OBD",
            subtitle="Pair with an OBD bridge and stream live data.",
            bullets=[
                "Realtime sensor feed",
                "Blended sim + live telemetry",
                "Use when hardware is attached",
            ],
        )

    def _build_actions(self):
        action_frame = tk.Frame(self.content, bg=self.BG)
        action_frame.grid(row=4, column=0, pady=(self._s(18), 0))

        self.start_button = tk.Button(
            action_frame,
            text="Start Simulation",
            font=self._font("Fira Sans", 11, "bold"),
            bg=self.ACCENT,
            activebackground=self.ACCENT_DARK,
            fg="#0b0f14",
            activeforeground="#ffffff",
            relief="flat",
            bd=0,
            padx=self._s(24),
            pady=self._s(10),
            cursor="hand2",
            command=self.start_simulation,
        )
        self.start_button.pack()

        hint = tk.Label(
            action_frame,
            text="Tip: You can switch modes anytime before launch.",
            font=self._font("Fira Sans", 9),
            fg=self.TEXT_MUTED,
            bg=self.BG,
        )
        hint.pack(pady=(self._s(8), 0))

        self.start_button.bind(
            "<Enter>",
            lambda _e: self.start_button.configure(bg="#fb923c"),
        )
        self.start_button.bind(
            "<Leave>",
            lambda _e: self.start_button.configure(bg=self.ACCENT),
        )

    def _create_mode_card(self, mode, title, subtitle, bullets):
        frame = tk.Frame(
            self.card_container,
            bg=self.SURFACE,
            highlightthickness=self._s(2),
            highlightbackground=self.BORDER,
            bd=0,
        )

        header = tk.Frame(frame, bg=self.SURFACE)
        header.pack(fill="x", padx=self._s(16), pady=(self._s(14), self._s(6)))

        indicator = tk.Canvas(
            header,
            width=self._s(16),
            height=self._s(16),
            highlightthickness=0,
            bg=self.SURFACE,
        )
        dot = indicator.create_oval(
            self._s(2),
            self._s(2),
            self._s(14),
            self._s(14),
            outline=self.TEXT_MUTED,
            width=self._s(2),
            fill=self.SURFACE,
        )
        indicator.pack(side="left", padx=(0, self._s(10)))

        title_label = tk.Label(
            header,
            text=title,
            font=self._font("DejaVu Serif", 14, "bold"),
            fg=self.TEXT,
            bg=self.SURFACE,
        )
        title_label.pack(side="left")

        subtitle_label = tk.Label(
            frame,
            text=subtitle,
            font=self._font("Fira Sans", 10),
            fg=self.TEXT_MUTED,
            bg=self.SURFACE,
            justify="left",
            wraplength=self._s(360),
        )
        subtitle_label.pack(fill="x", padx=self._s(16), pady=(0, self._s(6)))

        bullet_text = "\n".join([f"- {item}" for item in bullets])
        bullet_label = tk.Label(
            frame,
            text=bullet_text,
            font=self._font("Fira Sans", 10),
            fg=self.TEXT,
            bg=self.SURFACE,
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
                        row=row_idx,
                        column=0,
                        sticky="ew",
                        padx=self._s(12),
                        pady=self._s(8),
                    )
            self.card_container.columnconfigure(0, weight=1)
        else:
            for col_idx, mode in enumerate(("virtual", "twin")):
                if mode in self._card_nodes:
                    self._card_nodes[mode]["frame"].grid(
                        row=0,
                        column=col_idx,
                        sticky="nsew",
                        padx=self._s(10),
                        pady=self._s(8),
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
            state = "selected" if key == mode else "normal"
            self._apply_card_style(key, state=state)

    def _apply_card_style(self, mode, state="normal"):
        node = self._card_nodes.get(mode)
        if not node:
            return
        if state == "selected":
            bg = self.SURFACE_ACTIVE
            border = self.BORDER_ACTIVE
            title_fg = self.TEXT
            text_fg = self.TEXT
            dot_fill = self.ACCENT
            dot_outline = self.ACCENT
        elif state == "hover":
            bg = self.SURFACE_HOVER
            border = self.BORDER_HOVER
            title_fg = self.TEXT
            text_fg = self.TEXT
            dot_fill = bg
            dot_outline = self.TEXT_MUTED
        else:
            bg = self.SURFACE
            border = self.BORDER
            title_fg = self.TEXT
            text_fg = self.TEXT_MUTED
            dot_fill = bg
            dot_outline = self.TEXT_MUTED

        node["frame"].configure(bg=bg, highlightbackground=border)
        for widget in node["widgets"]:
            widget.configure(bg=bg)
        node["title"].configure(fg=title_fg)
        node["subtitle"].configure(fg=text_fg)
        node["bullets"].configure(fg=self.TEXT)
        node["indicator"].itemconfig(node["dot"], fill=dot_fill, outline=dot_outline)

    def start_simulation(self):
        launch_engine(self.engine_path, self.mode.get())
