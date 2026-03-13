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
        self.title("Choose Engine")
        self.configure(bg="#0f172a")
        self.transient(parent)
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
        self._build_logo()
        self._build_engines_section()
        self._build_selected_line()
        self._build_buttons()

        if self.selected_engine:
            _, selected_line = format_engine_display(self.selected_engine)
            self._selected_var.set(f"Selected engine: {selected_line}")

        self.bind("<Escape>", lambda _e: self.destroy())

        if not self.engines:
            messagebox.showwarning(
                "Engine Selection",
                f"No engine definitions (.mr) found in:\n{ENGINES_DIR}",
                parent=self,
            )

    def _s(self, value: int | float, minimum: int = 1) -> int:
        return scale_int(value, self.ui_scale, minimum=minimum)

    def _font(self, family: str, size: int, *styles: str, min_size: int = 8):
        return scale_font(self.ui_scale, family, size, *styles, min_size=min_size)

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

    def _build_logo(self) -> None:
        container = tk.Frame(self, bg="#0f172a")
        container.grid(row=0, column=0, sticky="ew", padx=self._s(16), pady=(self._s(12), self._s(8)))
        container.columnconfigure(0, weight=1)

        logo_loaded = False
        if ASCII_ART_TXT.exists():
            text = ASCII_ART_TXT.read_text(encoding="utf-8").strip()
            tk.Label(
                container,
                text=text,
                font=self._font("Consolas", 4, min_size=4),
                bg="#0f172a",
                fg="#e2e8f0",
                justify="left",
            ).grid(row=0, column=0)
            logo_loaded = True
        elif ASCII_ART_PNG.exists():
            try:
                img = tk.PhotoImage(file=str(ASCII_ART_PNG))
                self._logo_photo = img
                tk.Label(container, image=img, bg="#0f172a").grid(row=0, column=0)
                logo_loaded = True
            except tk.TclError:
                pass

        if not logo_loaded:
            tk.Label(
                container,
                text="V-Twin",
                font=self._font("Segoe UI", 16, "bold"),
                bg="#0f172a",
                fg="#e2e8f0",
            ).grid(row=0, column=0)

    def _build_engines_section(self) -> None:
        section = tk.Frame(self, bg="#0f172a")
        section.grid(row=1, column=0, sticky="nsew", padx=self._s(16), pady=self._s(4))
        section.columnconfigure(0, weight=1)

        top = tk.Frame(section, bg="#0f172a")
        top.grid(row=0, column=0, sticky="ew", pady=(0, self._s(6)))
        top.columnconfigure(0, weight=1)

        tk.Label(
            top,
            text="Available engines",
            font=self._font("Segoe UI", 10, "bold"),
            bg="#0f172a",
            fg="#cbd5e1",
            anchor="w",
        ).grid(row=0, column=0, sticky="w")

        filter_options = ["All"] + sorted(self._engines_by_folder.keys())
        self._filter_combo = ttk.Combobox(
            top,
            textvariable=self._filter_var,
            values=filter_options,
            state="readonly",
            width=max(10, self._s(14)),
        )
        self._filter_combo.grid(row=0, column=1, sticky="e", padx=(self._s(8), 0))
        self._filter_combo.set("All")
        self._filter_combo.bind("<<ComboboxSelected>>", lambda _e: self._refresh_engine_list())

        self._engine_canvas = tk.Canvas(section, bg="#0f172a", highlightthickness=0)
        scrollbar = ttk.Scrollbar(section, orient="vertical", command=self._engine_canvas.yview)
        self._engine_grid_frame = tk.Frame(self._engine_canvas, bg="#0f172a")
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

        sections = list(self._engines_by_folder.items()) if filter_val == "All" else [(filter_val, self._engines_by_folder[filter_val])]

        bg_normal = "#1e293b"
        bg_hover = "#334155"

        for folder_name, engine_list in sections:
            if not engine_list:
                continue

            block = tk.Frame(self._engine_grid_frame, bg="#0f172a")
            block.pack(fill="x", pady=(self._s(12), 0))

            tk.Label(
                block,
                text=f"- {folder_name} -",
                font=self._font("Segoe UI", 10, "bold"),
                bg="#0f172a",
                fg="#94a3b8",
                anchor="w",
            ).pack(fill="x")

            tk.Label(
                block,
                text=f"[{folder_name.replace('-', ' ').title()} engines]",
                font=self._font("Segoe UI", 9),
                bg="#0f172a",
                fg="#64748b",
                anchor="w",
            ).pack(fill="x", pady=(0, self._s(6)))

            cards_frame = tk.Frame(block, bg="#0f172a")
            cards_frame.pack(fill="x")

            for idx, engine in enumerate(engine_list):
                global_idx = len(self._displayed_engines)
                self._displayed_engines.append(engine)
                name, _ = format_engine_display(engine)
                row, col = divmod(idx, self._card_columns)

                frame = tk.Frame(cards_frame, bg=bg_normal, bd=0, highlightthickness=0)
                frame.grid(row=row, column=col, sticky="nsew", padx=self._s(4), pady=self._s(4))
                cards_frame.columnconfigure(col, weight=1)

                label = tk.Label(
                    frame,
                    text=name[:24] + ("..." if len(name) > 24 else ""),
                    font=self._font("Segoe UI", 10),
                    bg=bg_normal,
                    fg="#e2e8f0",
                    anchor="center",
                    wraplength=max(self._s(140), 90),
                )
                label.pack(fill="both", expand=True, padx=self._s(10), pady=self._s(8))

                self._card_widgets[global_idx] = [frame, label]

                for widget in (frame, label):
                    widget.bind("<Button-1>", lambda _e, i=global_idx: self._select_engine(i))
                    widget.bind("<Double-Button-1>", lambda _e, i=global_idx: (self._select_engine(i), self._on_confirm()))
                    widget.bind("<Enter>", lambda _e, i=global_idx: self._on_card_enter(i, bg_hover))
                    widget.bind("<Leave>", lambda _e, i=global_idx: self._on_card_leave(i, bg_normal))

        try:
            idx = self._displayed_engines.index(self.selected_engine)
            self._select_engine(idx)
        except (ValueError, AttributeError):
            pass

    def _on_card_enter(self, index: int, bg_hover: str) -> None:
        selected = self._displayed_engines[index] if index < len(self._displayed_engines) else None
        if self.selected_engine == selected:
            return
        for widget in self._card_widgets.get(index, []):
            widget.configure(bg=bg_hover)

    def _on_card_leave(self, index: int, bg_normal: str) -> None:
        selected = self._displayed_engines[index] if index < len(self._displayed_engines) else None
        if self.selected_engine == selected:
            return
        for widget in self._card_widgets.get(index, []):
            widget.configure(bg=bg_normal)

    def _select_engine(self, index: int) -> None:
        if index < 0 or index >= len(self._displayed_engines):
            return
        self.selected_engine = self._displayed_engines[index]
        bg_normal = "#1e293b"
        bg_selected = "#475569"

        for idx, widgets in self._card_widgets.items():
            for widget in widgets:
                widget.configure(bg=bg_selected if idx == index else bg_normal)

        _, selected_line = format_engine_display(self.selected_engine)
        self._selected_var.set(f"Selected engine: {selected_line}")

    def _build_selected_line(self) -> None:
        container = tk.Frame(self, bg="#0f172a")
        container.grid(row=2, column=0, sticky="ew", padx=self._s(16), pady=self._s(8))
        container.columnconfigure(0, weight=1)

        self._selected_var = tk.StringVar(value="Selected engine: -")
        tk.Label(
            container,
            textvariable=self._selected_var,
            font=self._font("Segoe UI", 9),
            bg="#0f172a",
            fg="#94a3b8",
            anchor="w",
        ).grid(row=0, column=0, sticky="w")

    def _build_buttons(self) -> None:
        footer = tk.Frame(self, bg="#0f172a")
        footer.grid(row=3, column=0, sticky="ew", padx=self._s(16), pady=(0, self._s(16)))
        footer.columnconfigure(0, weight=1)

        ttk.Button(footer, text="Confirm", command=self._on_confirm).grid(row=0, column=0, sticky="w")
        ttk.Button(footer, text="Cancel", command=self.destroy).grid(row=0, column=1, sticky="e")

    def _on_confirm(self) -> None:
        if self.selected_engine is None:
            messagebox.showinfo("Engine Selection", "Select an engine first by clicking a card.", parent=self)
            return

        rel_engine = self.selected_engine.relative_to(ROOT_DIR).as_posix()
        default_engine = self.selected_engine.parent.name
        persist_engine_config(default_engine, rel_engine)

        config.DEFAULT_ENGINE = default_engine
        config.ENGINE_PATH = rel_engine

        self._on_confirm_cb(rel_engine)
        self.destroy()
