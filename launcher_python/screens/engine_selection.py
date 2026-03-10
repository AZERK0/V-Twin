import tkinter as tk
from tkinter import filedialog
import config
from screens.mode_selection import ModeSelectionScreen

class EngineSelectionScreen:

    def __init__(self, root):
        self.root = root
        self.engine_path = config.ENGINE_PATH

        self.frame = tk.Frame(root)
        self.frame.pack(pady=20)

        self.label = tk.Label(self.frame, text="Select Engine")
        self.label.pack()

        self.button = tk.Button(
            self.frame,
            text="Choose Engine File",
            command=self.choose_engine
        )
        self.button.pack(pady=10)

        self.next_button = tk.Button(
            self.frame,
            text="Next",
            command=self.go_to_mode_selection
        )
        self.next_button.pack()

    def choose_engine(self):
        file_path = filedialog.askopenfilename(
            initialdir="assets/engines",
            title="Select Engine File"
        )

        if file_path:
            self.engine_path = file_path

    def go_to_mode_selection(self):
        self.frame.destroy()
        ModeSelectionScreen(self.root, self.engine_path)