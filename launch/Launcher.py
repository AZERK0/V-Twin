#!/usr/bin/env python3
"""
V-Twin Launcher — GUI to pick an engine config and run engine-sim-app.
Select an engine by clicking a card, then click "Run Engine". Selections
and executed commands are logged to launch/launcher.log.
"""

import os
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import tkinter as tk
from tkinter import messagebox, ttk


# --- Paths and config (all relative to repo root) ---
ROOT_DIR = Path(__file__).resolve().parent.parent
LAUNCH_DIR = ROOT_DIR / "launch"
ENGINES_DIR = ROOT_DIR / "assets" / "engines"
APP_PATH = ROOT_DIR / "build" / "linux-release" / "engine-sim-app"
LOG_PATH = LAUNCH_DIR / "launcher.log"
ASCII_ART_TXT = LAUNCH_DIR / "ascii-art.txt"
ASCII_ART_PNG = LAUNCH_DIR / "ascii-art.png"

CARD_COLUMNS = 3  # Number of engine cards per row in the grid


def find_engines() -> List[Path]:
    """Discover all .mr engine files under assets/engines (recursive)."""
    engines: List[Path] = []
    if not ENGINES_DIR.exists():
        return engines
    for path in ENGINES_DIR.rglob("*.mr"):
        if path.is_file():
            engines.append(path)
    engines.sort()
    return engines


def get_engines_by_folder(engines: List[Path]) -> Dict[str, List[Path]]:
    """Group engine paths by immediate folder under assets/engines (e.g. chevrolet, audi)."""
    by_folder: Dict[str, List[Path]] = {}
    for p in engines:
        try:
            rel = p.relative_to(ENGINES_DIR)
        except ValueError:
            continue
        parts = rel.parts
        folder = parts[0] if len(parts) >= 2 else ""
        by_folder.setdefault(folder, []).append(p)
    for paths in by_folder.values():
        paths.sort()
    return dict(sorted(by_folder.items()))


def format_engine_display(engine_path: Path) -> Tuple[str, str]:
    """Return (name for card label, full line for "Selected engine: ...")."""
    rel = engine_path.relative_to(ROOT_DIR).as_posix()
    parts = rel.split("/")
    brand = parts[-2] if len(parts) >= 2 else "engine"
    name = engine_path.stem.replace("_", " ").replace("-", " ")
    return name, f"{brand} . {rel}"


def log_selection(engine_rel_path: str, command_str: str) -> None:
    """Append timestamp, selected path, and [SYSTEM] running command to launcher.log."""
    ts = datetime.now().isoformat(timespec="seconds")
    lines = [
        f"{ts} selected {engine_rel_path}",
        f"[SYSTEM] running: {command_str}",
        "",
    ]
    LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
    with LOG_PATH.open("a", encoding="utf-8") as f:
        f.write("\n".join(lines))


def launch_engine(engine_path: Path) -> None:
    """Run engine-sim-app with --scripts <engine_path>; log to launcher.log; set ENGINE_SIM_DATA_ROOT."""
    if not APP_PATH.is_file():
        messagebox.showerror(
            "V-Twin Launcher",
            f"Binary not found:\n{APP_PATH}\n\nRun ./scripts/build.sh first.",
        )
        return

    rel_engine = engine_path.relative_to(ROOT_DIR).as_posix()
    cmd = [str(APP_PATH), "--scripts", rel_engine]
    cmd_str = f"{APP_PATH} --scripts {rel_engine}"

    log_selection(rel_engine, cmd_str)

    # Required so the app finds assets relative to repo root
    env = os.environ.copy()
    env["ENGINE_SIM_DATA_ROOT"] = str(ROOT_DIR)

    try:
        subprocess.Popen(cmd, cwd=str(ROOT_DIR), env=env)  # Non-blocking; launcher stays open
    except Exception as exc:  # pylint: disable=broad-except
        messagebox.showerror("V-Twin Launcher", f"Failed to launch:\n{exc}")


class LauncherApp(tk.Tk):
    """Main window: logo, engine grid, selected-engine line, Run Engine / Exit."""

    def __init__(self, engines: List[Path]) -> None:
        super().__init__()
        self.title("V-Twin Launcher")
        self.engines = engines
        self._engines_by_folder = get_engines_by_folder(engines)
        self._filter_var = tk.StringVar(value="All")  # "All" or folder name
        self.selected_engine: Optional[Path] = None  # Set when user clicks a card
        self._displayed_engines: List[Path] = []  # Flat list of engines currently shown (for selection index)
        self._card_widgets: dict = {}  # index -> [frame, label] for selection/hover highlight

        # Window: compact, floating/dialog style
        self.geometry("520x520")
        self.minsize(420, 420)
        self.resizable(True, True)
        self.configure(bg="#0f172a")

        try:
            self.attributes("-type", "dialog")  # Dialog-style window on X11
        except tk.TclError:
            pass

        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)   # Engine list gets extra vertical space
        self.rowconfigure(2, weight=0)

        self._build_logo()
        self._build_engines_section()
        self._build_selected_line()
        self._build_buttons()

        if not self.engines:
            messagebox.showwarning(
                "V-Twin Launcher",
                f"No engine definitions (.mr) found in:\n{ENGINES_DIR}",
            )

    def _build_logo(self) -> None:
        """Show V-Twin logo: prefer ascii-art.txt, else ascii-art.png, else plain "V-Twin" text."""
        container = tk.Frame(self, bg="#0f172a")
        container.grid(row=0, column=0, sticky="ew", padx=16, pady=(12, 8))
        container.columnconfigure(0, weight=1)

        logo_loaded = False
        if ASCII_ART_TXT.exists():
            text = ASCII_ART_TXT.read_text(encoding="utf-8").strip()
            lbl = tk.Label(
                container,
                text=text,
                font=("Consolas", 4),
                bg="#0f172a",
                fg="#e2e8f0",
                justify="left",
            )
            lbl.grid(row=0, column=0)
            logo_loaded = True
        elif ASCII_ART_PNG.exists():
            try:
                img = tk.PhotoImage(file=str(ASCII_ART_PNG))
                self._logo_photo = img  # Keep reference so image is not garbage-collected
                lbl = tk.Label(container, image=img, bg="#0f172a")
                lbl.grid(row=0, column=0)
                logo_loaded = True
            except tk.TclError:
                pass

        if not logo_loaded:
            # Fallback when no ascii-art file or PNG
            lbl = tk.Label(
                container,
                text="V-Twin",
                font=("Segoe UI", 16, "bold"),
                bg="#0f172a",
                fg="#e2e8f0",
            )
            lbl.grid(row=0, column=0)

    def _build_engines_section(self) -> None:
        """Build filter dropdown and scrollable area with sectioned engine cards."""
        section = tk.Frame(self, bg="#0f172a")
        section.grid(row=1, column=0, sticky="nsew", padx=16, pady=4)
        section.columnconfigure(0, weight=1)

        # Top row: "Available engines" label + filter dropdown
        top = tk.Frame(section, bg="#0f172a")
        top.grid(row=0, column=0, sticky="ew", pady=(0, 6))
        top.columnconfigure(0, weight=1)

        tk.Label(
            top,
            text="Available engines",
            font=("Segoe UI", 10, "bold"),
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
            width=14,
        )
        self._filter_combo.grid(row=0, column=1, sticky="e", padx=(8, 0))
        self._filter_combo.set("All")
        self._filter_combo.bind("<<ComboboxSelected>>", lambda e: self._refresh_engine_list())

        # Scrollable area: canvas + scrollbar, inner frame for section blocks
        self._engine_canvas = tk.Canvas(section, bg="#0f172a", highlightthickness=0)
        scrollbar = ttk.Scrollbar(section, orient="vertical", command=self._engine_canvas.yview)
        self._engine_grid_frame = tk.Frame(self._engine_canvas, bg="#0f172a")

        self._engine_grid_frame.bind(
            "<Configure>",
            lambda e: self._engine_canvas.configure(scrollregion=self._engine_canvas.bbox("all")),
        )
        self._engine_canvas.create_window((0, 0), window=self._engine_grid_frame, anchor="nw")
        self._engine_canvas.configure(yscrollcommand=scrollbar.set)

        def _on_mousewheel(event: tk.Event) -> None:
            if event.num == 5 or getattr(event, "delta", 0) == -120:
                self._engine_canvas.yview_scroll(1, "units")
            elif event.num == 4 or getattr(event, "delta", 0) == 120:
                self._engine_canvas.yview_scroll(-1, "units")

        self._engine_canvas.bind("<Enter>", lambda e: (
            self._engine_canvas.bind_all("<MouseWheel>", _on_mousewheel),
            self._engine_canvas.bind_all("<Button-4>", _on_mousewheel),
            self._engine_canvas.bind_all("<Button-5>", _on_mousewheel),
        ))
        self._engine_canvas.bind("<Leave>", lambda e: (
            self._engine_canvas.unbind_all("<MouseWheel>"),
            self._engine_canvas.unbind_all("<Button-4>"),
            self._engine_canvas.unbind_all("<Button-5>"),
        ))

        self._engine_canvas.grid(row=1, column=0, sticky="nsew")
        scrollbar.grid(row=1, column=1, sticky="ns")
        section.rowconfigure(1, weight=1)

        self._refresh_engine_list()

    def _refresh_engine_list(self) -> None:
        """Rebuild section headers and cards from current filter; update _displayed_engines and _card_widgets."""
        for w in self._engine_grid_frame.winfo_children():
            w.destroy()

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

        # Sections to show: all folders or single folder
        if filter_val == "All":
            sections = list(self._engines_by_folder.items())
        else:
            sections = [(filter_val, self._engines_by_folder[filter_val])]

        bg_normal = "#1e293b"
        bg_hover = "#334155"

        for folder_name, engine_list in sections:
            if not engine_list:
                continue
            # Section block: header, subheader, then grid of cards
            block = tk.Frame(self._engine_grid_frame, bg="#0f172a")
            block.pack(fill="x", pady=(12, 0))

            tk.Label(
                block,
                text=f"- {folder_name} -",
                font=("Segoe UI", 10, "bold"),
                bg="#0f172a",
                fg="#94a3b8",
                anchor="w",
            ).pack(fill="x")

            tk.Label(
                block,
                text=f"[{folder_name.replace('-', ' ').title()} engines]",
                font=("Segoe UI", 9),
                bg="#0f172a",
                fg="#64748b",
                anchor="w",
            ).pack(fill="x", pady=(0, 6))

            cards_frame = tk.Frame(block, bg="#0f172a")
            cards_frame.pack(fill="x")

            for idx, engine in enumerate(engine_list):
                global_idx = len(self._displayed_engines)
                self._displayed_engines.append(engine)
                name, _ = format_engine_display(engine)
                row, col = divmod(idx, CARD_COLUMNS)

                frame = tk.Frame(cards_frame, bg=bg_normal, bd=0, highlightthickness=0)
                frame.grid(row=row, column=col, sticky="nsew", padx=4, pady=4)
                cards_frame.columnconfigure(col, weight=1)

                label = tk.Label(
                    frame,
                    text=name[:24] + ("…" if len(name) > 24 else ""),
                    font=("Segoe UI", 10),
                    bg=bg_normal,
                    fg="#e2e8f0",
                    anchor="center",
                    wraplength=120,
                )
                label.pack(fill="both", expand=True, padx=10, pady=8)

                self._card_widgets[global_idx] = [frame, label]

                def on_click(e: tk.Event | None = None, i: int = global_idx) -> None:
                    self._select_engine(i)

                def on_double_click(e: tk.Event | None = None, i: int = global_idx) -> None:
                    self._select_engine(i)
                    self._on_run()

                def on_enter(e: tk.Event, i: int = global_idx) -> None:
                    if self.selected_engine != (self._displayed_engines[i] if i < len(self._displayed_engines) else None):
                        if i in self._card_widgets:
                            for w in self._card_widgets[i]:
                                w.configure(bg=bg_hover)

                def on_leave(e: tk.Event, i: int = global_idx) -> None:
                    if self.selected_engine != (self._displayed_engines[i] if i < len(self._displayed_engines) else None):
                        if i in self._card_widgets:
                            for w in self._card_widgets[i]:
                                w.configure(bg=bg_normal)

                for w in (frame, label):
                    w.bind("<Button-1>", lambda ev, i=global_idx: on_click(ev, i))
                    w.bind("<Double-Button-1>", lambda ev, i=global_idx: on_double_click(ev, i))
                    w.bind("<Enter>", lambda ev, i=global_idx: on_enter(ev, i))
                    w.bind("<Leave>", lambda ev, i=global_idx: on_leave(ev, i))

        # Re-apply selection highlight if selected engine is still in displayed list
        try:
            idx = self._displayed_engines.index(self.selected_engine)
            self._select_engine(idx)
        except (ValueError, AttributeError):
            pass

    def _select_engine(self, index: int) -> None:
        """Set selected engine, update card highlights and "Selected engine: ..." line."""
        if index < 0 or index >= len(self._displayed_engines):
            return
        engine = self._displayed_engines[index]
        self.selected_engine = engine
        bg_normal = "#1e293b"
        bg_selected = "#475569"

        for i, widgets in self._card_widgets.items():
            for w in widgets:
                w.configure(bg=bg_selected if i == index else bg_normal)

        _, sel_line = format_engine_display(engine)
        self._selected_var.set(f"Selected engine: {sel_line}")

    def _build_selected_line(self) -> None:
        """Single line showing "Selected engine: -" or "Selected engine: brand . path"."""
        container = tk.Frame(self, bg="#0f172a")
        container.grid(row=2, column=0, sticky="ew", padx=16, pady=8)
        container.columnconfigure(0, weight=1)

        self._selected_var = tk.StringVar(value="Selected engine: -")
        tk.Label(
            container,
            textvariable=self._selected_var,
            font=("Segoe UI", 9),
            bg="#0f172a",
            fg="#94a3b8",
            anchor="w",
        ).grid(row=0, column=0, sticky="w")

    def _build_buttons(self) -> None:
        """Run Engine (launches selected) and Exit (quit launcher)."""
        footer = tk.Frame(self, bg="#0f172a")
        footer.grid(row=3, column=0, sticky="ew", padx=16, pady=(0, 16))
        footer.columnconfigure(0, weight=1)

        ttk.Button(
            footer,
            text="Run Engine",
            command=self._on_run,
        ).grid(row=0, column=0, sticky="w")

        ttk.Button(footer, text="Exit", command=self.destroy).grid(row=0, column=1, sticky="e")

    def _on_run(self) -> None:
        """Launch selected engine (if any), then minimize launcher window."""
        if self.selected_engine is None:
            messagebox.showinfo(
                "V-Twin Launcher",
                "Select an engine first by clicking a card.",
            )
            return
        launch_engine(self.selected_engine)
        # Minimize so user can focus on the running engine window
        try:
            self.iconify()
        except tk.TclError:
            pass


def main() -> int:
    """Discover engines, show launcher GUI, run event loop."""
    engines = find_engines()
    app = LauncherApp(engines)
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())  # Run from repo root: ./launch/Launcher.py or python3 launch/Launcher.py
