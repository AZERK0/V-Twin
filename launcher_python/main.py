import tkinter as tk
from screens.engine_selection import EngineSelectionScreen

def main():
    root = tk.Tk()
    root.title("V-Twin - Engine Launcher")
    root.geometry("400x200")

    EngineSelectionScreen(root)

    root.mainloop()

if __name__ == "__main__":
    main()