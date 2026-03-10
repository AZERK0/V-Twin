import tkinter as tk
from utils.launcher import launch_engine

class ModeSelectionScreen:

    def __init__(self, root, engine_path):
        self.root = root
        self.engine_path = engine_path

        self.mode = tk.StringVar(value="virtual")

        self.frame = tk.Frame(root)
        self.frame.pack(pady=20)

        label = tk.Label(self.frame, text="Select Mode")
        label.pack()

        tk.Radiobutton(
            self.frame,
            text="Full Virtual",
            variable=self.mode,
            value="virtual"
        ).pack()

        tk.Radiobutton(
            self.frame,
            text="Twin OBD",
            variable=self.mode,
            value="twin"
        ).pack()

        start_button = tk.Button(
            self.frame,
            text="Start Simulation",
            command=self.start_simulation
        )
        start_button.pack(pady=10)

    def start_simulation(self):
        launch_engine(self.engine_path, self.mode.get())