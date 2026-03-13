import tkinter as tk
from utils.launcher import launch_engine
from screens.ui_scale import compute_ui_scale, scale_font, scale_int

class ModeSelectionScreen:

    def __init__(self, root, engine_path):
        self.root = root
        self.engine_path = engine_path
        self.ui_scale = compute_ui_scale(self.root)

        self.mode = tk.StringVar(value="virtual")

        self.frame = tk.Frame(root)
        self.frame.pack(pady=self._s(20))

        label = tk.Label(self.frame, text="Select Mode", font=self._font("Segoe UI", 12, "bold"))
        label.pack()

        tk.Radiobutton(
            self.frame,
            text="Full Virtual",
            variable=self.mode,
            value="virtual",
            font=self._font("Segoe UI", 11),
        ).pack()

        tk.Radiobutton(
            self.frame,
            text="Twin OBD",
            variable=self.mode,
            value="twin",
            font=self._font("Segoe UI", 11),
        ).pack()

        start_button = tk.Button(
            self.frame,
            text="Start Simulation",
            font=self._font("Segoe UI", 11, "bold"),
            padx=self._s(14),
            pady=self._s(6),
            command=self.start_simulation
        )
        start_button.pack(pady=self._s(10))

    def _s(self, value, minimum=1):
        return scale_int(value, self.ui_scale, minimum=minimum)

    def _font(self, family, size, *styles):
        return scale_font(self.ui_scale, family, size, *styles)

    def start_simulation(self):
        launch_engine(self.engine_path, self.mode.get())
