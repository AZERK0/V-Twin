from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

import tkinter as tk
from tkinter import messagebox, ttk

import config
from screens.ui_scale import compute_ui_scale, scale_font, scale_int


ROOT_DIR = Path(__file__).resolve().parents[2]
ENGINES_DIR = ROOT_DIR / "assets" / "engines"
ASSETS_DIR = Path(__file__).resolve().parent.parent / "assets"
ASCII_ART_TXT = ASSETS_DIR / "ascii-art.txt"
ASCII_ART_PNG = ASSETS_DIR / "ascii-art.png"
CONFIG_PATH = Path(__file__).resolve().parent.parent / "config.py"
CARD_COLUMNS = 3

MONO = "Courier"
BG = "#1a1a1a"
BG_CARD = "#242424"
BG_CARD_HOVER = "#2e2e2e"
BG_CARD_SELECTED = "#3a1a1a"
FG = "#dde2ea"
FG_DIM = "#7a8a99"
BORDER = "#3a3a3a"
BORDER_SELECTED = "#c0392b"
ACCENT = "#c0392b"
BTN_BG = "#2a2a2a"
BTN_HOVER = "#3a3a3a"


def find_engines() -> List[Path]:
    engines: List[Path] = []
    if not ENGINES_DIR.exists():
        return engines
    for path in ENGINES_DIR.rglob("*.mr"):
        if path.is_file():
            engines.append(path)
    engines.sort()
    return engines


def get_engines_by_folder(engines: List[Path]) -> Dict[str, List[Path]]:
    by_folder: Dict[str, List[Path]] = {}
    for path in engines:
        try:
            rel = path.relative_to(ENGINES_DIR)
        except ValueError:
            continue
        folder = rel.parts[0] if len(rel.parts) >= 2 else ""
        by_folder.setdefault(folder, []).append(path)
    for values in by_folder.values():
        values.sort()
    return dict(sorted(by_folder.items()))


def format_engine_display(engine_path: Path) -> Tuple[str, str]:
    rel = engine_path.relative_to(ROOT_DIR).as_posix()
    parts = rel.split("/")
    brand = parts[-2] if len(parts) >= 2 else "engine"
    name = engine_path.stem.replace("_", " ").replace("-", " ")
    return name, f"{brand} . {rel}"


def _replace_line(lines: List[str], prefix: str, replacement: str) -> bool:
    for i, line in enumerate(lines):
        if line.startswith(prefix):
            lines[i] = replacement
            return True
    return False


def persist_engine_config(default_engine: str, engine_rel_path: str) -> None:
    default_line = f'DEFAULT_ENGINE = "{default_engine}"'
    engine_path_line = f'ENGINE_PATH = "{engine_rel_path}"'
    lines = CONFIG_PATH.read_text(encoding="utf-8").splitlines()

    has_default = _replace_line(lines, "DEFAULT_ENGINE = ", default_line)
    has_engine_path = _replace_line(lines, "ENGINE_PATH = ", engine_path_line)

    if not has_default:
        lines.insert(0, default_line)
    if not has_engine_path:
        insert_idx = 1 if lines and lines[0].startswith("DEFAULT_ENGINE = ") else 0
        lines.insert(insert_idx, engine_path_line)

    CONFIG_PATH.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


class EngineFolderSelectionDialog(tk.Toplevel):
    def __init__(
        self,
        parent: tk.Misc,
        current_engine_path: str,
        on_confirm: Callable[[str], None],
    ) -> None:
        super().__init__(parent)
        self.title("V-TWIN // ENGINE SELECTION")
        self.configure(bg=BG)
        self.transient(parent)  # type: ignore[arg-type]
        self.grab_set()

        self._on_confirm_cb = on_confirm
        self.engines = find_engines()
        self._engines_by_folder = get_engines_by_folder(self.engines)
        self._filter_var = tk.StringVar(value="All")
        self.ui_scale = compute_ui_scale(self)
        self._displayed_engines: List[Path] = []
        self._card_widgets: Dict[int, List[tk.Widget]] = {}
        self._card_columns = 3
        self.selected_engine: Optional[Path] = self._resolve_engine_path(current_engine_path)

        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)
        self.rowconfigure(2, weight=0)

        self._set_initial_window_size()
        self._build_header()
        self._build_engines_section()
        self._build_selected_line()
        self._build_buttons()

        if self.selected_engine:
            _, selected_line = format_engine_display(self.selected_engine)
            self._selected_var.set(f"SELECTED : {selected_line.upper()}")

        self.bind("<Escape>", lambda _e: self.destroy())

        if not self.engines:
            messagebox.showwarning(
                "ENGINE SELECTION",
                f"No engine definitions (.mr) found in:\n{ENGINES_DIR}",
                parent=self,
            )

    def _s(self, value: int | float, minimum: int = 1) -> int:
        return scale_int(value, self.ui_scale, minimum=minimum)

    def _font(self, size: int, *styles: str, min_size: int = 8):
        return scale_font(self.ui_scale, MONO, size, *styles, min_size=min_size)

    def _set_initial_window_size(self) -> None:
        screen_w = self.winfo_screenwidth()
        screen_h = self.winfo_screenheight()
        margin_w = self._s(40)
        margin_h = self._s(80)
        target_w = min(max(self._s(1100), int(screen_w * 0.90)), max(self._s(700), screen_w - margin_w))
        target_h = min(max(self._s(760), int(screen_h * 0.88)), max(self._s(520), screen_h - margin_h))
        pos_x = max(0, (screen_w - target_w) // 2)
        pos_y = max(0, (screen_h - target_h) // 2)
        self.geometry(f"{target_w}x{target_h}+{pos_x}+{pos_y}")
        self.minsize(min(self._s(1000), target_w), min(self._s(680), target_h))
        self.resizable(True, True)
        self._card_columns = max(1, min(CARD_COLUMNS, target_w // max(self._s(320), 220)))

    def _resolve_engine_path(self, engine_path: str) -> Optional[Path]:
        if not engine_path:
            return None
        candidate = Path(engine_path)
        absolute = candidate if candidate.is_absolute() else (ROOT_DIR / candidate)
        if absolute.is_dir():
            engine_files = sorted(absolute.rglob("*.mr"))
            return engine_files[0] if engine_files else None
        if absolute.is_file() and absolute.suffix == ".mr":
            return absolute
        return None

    def _build_header(self) -> None:
        container = tk.Frame(self, bg=BG)
        container.grid(row=0, column=0, sticky="ew", padx=self._s(16), pady=(self._s(12), self._s(8)))
        container.columnconfigure(0, weight=1)

        logo_path = ASSETS_DIR / "logo_v_twin.png"
        logo_loaded = False
        if logo_path.exists():
            try:
                import math
                img = tk.PhotoImage(file=str(logo_path))
                max_w, max_h = self._s(280), self._s(168)
                factor = max(img.width() / max_w, img.height() / max_h)
                sample = max(1, math.ceil(factor))
                if sample > 1:
                    img = img.subsample(sample, sample)
                self._logo_photo = img
                tk.Label(container, image=img, bg=BG).grid(row=0, column=0, sticky="w")
                logo_loaded = True
            except tk.TclError:
                pass

        if not logo_loaded:
            tk.Label(
                container,
                text="V-TWIN",
                font=self._font(16, "bold"),
                bg=BG,
                fg=FG,
            ).grid(row=0, column=0, sticky="w")

        tk.Label(
            container,
            text="ENGINE SELECTION",
            font=self._font(11, "bold"),
            bg=BG,
            fg=ACCENT,
        ).grid(row=1, column=0, sticky="w", pady=(self._s(4), 0))

        separator = tk.Frame(container, bg=BORDER, height=1)
        separator.grid(row=2, column=0, sticky="ew", pady=(self._s(8), 0))

    def _build_engines_section(self) -> None:
        section = tk.Frame(self, bg=BG)
        section.grid(row=1, column=0, sticky="nsew", padx=self._s(16), pady=self._s(4))
        section.columnconfigure(0, weight=1)

        top = tk.Frame(section, bg=BG)
        top.grid(row=0, column=0, sticky="ew", pady=(0, self._s(6)))
        top.columnconfigure(0, weight=1)

        tk.Label(
            top,
            text="AVAILABLE ENGINES",
            font=self._font(9, "bold"),
            bg=BG,
            fg=FG_DIM,
            anchor="w",
        ).grid(row=0, column=0, sticky="w")

        filter_frame = tk.Frame(top, bg=BG)
        filter_frame.grid(row=0, column=1, sticky="e", padx=(self._s(8), 0))

        tk.Label(filter_frame, text="FILTER:", font=self._font(9), bg=BG, fg=FG_DIM).pack(side="left", padx=(0, self._s(4)))

        filter_options = ["All"] + sorted(self._engines_by_folder.keys())
        self._filter_combo = ttk.Combobox(
            filter_frame,
            textvariable=self._filter_var,
            values=filter_options,
            state="readonly",
            width=max(10, self._s(14)),
            font=self._font(9),
        )
        self._filter_combo.pack(side="left")
        self._filter_combo.set("All")
        self._filter_combo.bind("<<ComboboxSelected>>", lambda _e: self._refresh_engine_list())

        self._apply_combobox_style()

        self._engine_canvas = tk.Canvas(section, bg=BG, highlightthickness=0)
        scrollbar = ttk.Scrollbar(section, orient="vertical", command=self._engine_canvas.yview)
        self._engine_grid_frame = tk.Frame(self._engine_canvas, bg=BG)
        self._engine_grid_frame.bind(
            "<Configure>",
            lambda _e: self._engine_canvas.configure(scrollregion=self._engine_canvas.bbox("all")),
        )
        self._engine_canvas.create_window((0, 0), window=self._engine_grid_frame, anchor="nw")
        self._engine_canvas.configure(yscrollcommand=scrollbar.set)

        def _on_mousewheel(event: tk.Event) -> None:
            if event.num == 5 or getattr(event, "delta", 0) == -120:
                self._engine_canvas.yview_scroll(1, "units")
            elif event.num == 4 or getattr(event, "delta", 0) == 120:
                self._engine_canvas.yview_scroll(-1, "units")

        self._engine_canvas.bind(
            "<Enter>",
            lambda _e: (
                self._engine_canvas.bind_all("<MouseWheel>", _on_mousewheel),
                self._engine_canvas.bind_all("<Button-4>", _on_mousewheel),
                self._engine_canvas.bind_all("<Button-5>", _on_mousewheel),
            ),
        )
        self._engine_canvas.bind(
            "<Leave>",
            lambda _e: (
                self._engine_canvas.unbind_all("<MouseWheel>"),
                self._engine_canvas.unbind_all("<Button-4>"),
                self._engine_canvas.unbind_all("<Button-5>"),
            ),
        )

        self._engine_canvas.grid(row=1, column=0, sticky="nsew")
        scrollbar.grid(row=1, column=1, sticky="ns")
        section.rowconfigure(1, weight=1)
        self._refresh_engine_list()

    def _apply_combobox_style(self) -> None:
        style = ttk.Style(self)
        style.theme_use("default")
        style.configure(
            "TCombobox",
            fieldbackground=BG_CARD,
            background=BG_CARD,
            foreground=FG,
            selectbackground=BG_CARD_SELECTED,
            selectforeground=FG,
            arrowcolor=FG_DIM,
            bordercolor=BORDER,
            lightcolor=BORDER,
            darkcolor=BORDER,
        )

    def _refresh_engine_list(self) -> None:
        for widget in self._engine_grid_frame.winfo_children():
            widget.destroy()

        self._displayed_engines = []
        self._card_widgets = {}

        filter_val = self._filter_var.get().strip()
        if filter_val not in self._engines_by_folder and filter_val != "All":
            filter_val = "All"
            self._filter_var.set("All")
            try:
                self._filter_combo.set("All")
            except tk.TclError:
                pass

        sections = (
            list(self._engines_by_folder.items())
            if filter_val == "All"
            else [(filter_val, self._engines_by_folder[filter_val])]
        )

        for folder_name, engine_list in sections:
            if not engine_list:
                continue

            block = tk.Frame(self._engine_grid_frame, bg=BG)
            block.pack(fill="x", pady=(self._s(12), 0))

            tk.Frame(block, bg=BORDER, height=1).pack(fill="x", pady=(0, self._s(6)))

            tk.Label(
                block,
                text=f"[ {folder_name.upper()} ]",
                font=self._font(9, "bold"),
                bg=BG,
                fg=ACCENT,
                anchor="w",
            ).pack(fill="x")

            tk.Label(
                block,
                text=f"{len(engine_list)} ENGINE(S)",
                font=self._font(8),
                bg=BG,
                fg=FG_DIM,
                anchor="w",
            ).pack(fill="x", pady=(0, self._s(6)))

            cards_frame = tk.Frame(block, bg=BG)
            cards_frame.pack(fill="x")

            for idx, engine in enumerate(engine_list):
                global_idx = len(self._displayed_engines)
                self._displayed_engines.append(engine)
                name, _ = format_engine_display(engine)
                row, col = divmod(idx, self._card_columns)

                is_selected = engine == self.selected_engine
                card_bg = BG_CARD_SELECTED if is_selected else BG_CARD
                card_border = BORDER_SELECTED if is_selected else BORDER

                frame = tk.Frame(
                    cards_frame,
                    bg=card_bg,
                    bd=0,
                    highlightthickness=1,
                    highlightbackground=card_border,
                )
                frame.grid(row=row, column=col, sticky="nsew", padx=self._s(4), pady=self._s(4))
                cards_frame.columnconfigure(col, weight=1)

                display_name = name[:22].upper() + ("..." if len(name) > 22 else "")
                label = tk.Label(
                    frame,
                    text=display_name,
                    font=self._font(9),
                    bg=card_bg,
                    fg=FG if not is_selected else FG,
                    anchor="center",
                    wraplength=max(self._s(140), 90),
                    pady=self._s(8),
                    padx=self._s(10),
                )
                label.pack(fill="both", expand=True)

                self._card_widgets[global_idx] = [frame, label]

                for widget in (frame, label):
                    widget.bind("<Button-1>", lambda _e, i=global_idx: self._select_engine(i))
                    widget.bind("<Double-Button-1>", lambda _e, i=global_idx: (self._select_engine(i), self._on_confirm()))
                    widget.bind("<Enter>", lambda _e, i=global_idx: self._on_card_enter(i))
                    widget.bind("<Leave>", lambda _e, i=global_idx: self._on_card_leave(i))

        if self.selected_engine and self.selected_engine in self._displayed_engines:
            idx = self._displayed_engines.index(self.selected_engine)
            self._apply_card_highlight(idx, selected=True)

    def _on_card_enter(self, index: int) -> None:
        engine = self._displayed_engines[index] if index < len(self._displayed_engines) else None
        if self.selected_engine == engine:
            return
        widgets = self._card_widgets.get(index, [])
        if widgets:
            widgets[0].configure(bg=BG_CARD_HOVER, highlightbackground=FG_DIM)  # type: ignore[call-arg]
        for widget in widgets[1:]:
            widget.configure(bg=BG_CARD_HOVER)  # type: ignore[call-arg]

    def _on_card_leave(self, index: int) -> None:
        engine = self._displayed_engines[index] if index < len(self._displayed_engines) else None
        if self.selected_engine == engine:
            return
        self._apply_card_highlight(index, selected=False)

    def _apply_card_highlight(self, index: int, selected: bool) -> None:
        bg = BG_CARD_SELECTED if selected else BG_CARD
        border = BORDER_SELECTED if selected else BORDER
        widgets = self._card_widgets.get(index, [])
        if widgets:
            widgets[0].configure(bg=bg, highlightbackground=border)  # type: ignore[call-arg]
        for widget in widgets[1:]:
            widget.configure(bg=bg)  # type: ignore[call-arg]

    def _select_engine(self, index: int) -> None:
        if index < 0 or index >= len(self._displayed_engines):
            return

        prev_engine = self.selected_engine
        self.selected_engine = self._displayed_engines[index]

        if prev_engine and prev_engine in self._displayed_engines:
            prev_idx = self._displayed_engines.index(prev_engine)
            self._apply_card_highlight(prev_idx, selected=False)

        self._apply_card_highlight(index, selected=True)

        _, selected_line = format_engine_display(self.selected_engine)
        self._selected_var.set(f"SELECTED : {selected_line.upper()}")

    def _build_selected_line(self) -> None:
        container = tk.Frame(self, bg=BG)
        container.grid(row=2, column=0, sticky="ew", padx=self._s(16), pady=(self._s(4), self._s(4)))
        container.columnconfigure(0, weight=1)

        tk.Frame(container, bg=BORDER, height=1).grid(row=0, column=0, sticky="ew", pady=(0, self._s(6)))

        self._selected_var = tk.StringVar(value="SELECTED : --")
        tk.Label(
            container,
            textvariable=self._selected_var,
            font=self._font(9),
            bg=BG,
            fg=FG_DIM,
            anchor="w",
        ).grid(row=1, column=0, sticky="w")

    def _build_buttons(self) -> None:
        footer = tk.Frame(self, bg=BG)
        footer.grid(row=3, column=0, sticky="ew", padx=self._s(16), pady=(self._s(6), self._s(16)))
        footer.columnconfigure(0, weight=1)

        confirm_btn = tk.Button(
            footer,
            text="CONFIRM",
            font=self._font(10, "bold"),
            bg=ACCENT,
            activebackground="#a93226",
            fg=FG,
            activeforeground=FG,
            relief="flat",
            bd=0,
            padx=self._s(24),
            pady=self._s(8),
            cursor="hand2",
            command=self._on_confirm,
        )
        confirm_btn.grid(row=0, column=0, sticky="w")

        cancel_btn = tk.Button(
            footer,
            text="CANCEL",
            font=self._font(10),
            bg=BTN_BG,
            activebackground=BTN_HOVER,
            fg=FG_DIM,
            activeforeground=FG,
            relief="flat",
            bd=0,
            padx=self._s(20),
            pady=self._s(8),
            cursor="hand2",
            command=self.destroy,
        )
        cancel_btn.grid(row=0, column=1, sticky="e")

    def _on_confirm(self) -> None:
        if self.selected_engine is None:
            messagebox.showinfo("ENGINE SELECTION", "Select an engine first by clicking a card.", parent=self)
            return

        rel_engine = self.selected_engine.relative_to(ROOT_DIR).as_posix()
        default_engine = self.selected_engine.parent.name
        persist_engine_config(default_engine, rel_engine)

        config.DEFAULT_ENGINE = default_engine
        config.ENGINE_PATH = rel_engine

        self._on_confirm_cb(rel_engine)
        self.destroy()
