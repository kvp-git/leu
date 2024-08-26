import leupi as lp
import tkinter as tk
from PIL import ImageTk, Image, ImageDraw
from enum import Enum



window = tk.Tk()
window.title("This is an application")
window.minsize(640, 480)
window.maxsize(640, 480)

frame = tk.Frame(master=window, width=640, height=480, bg="green")
frame.place(x=0, y=0)

# TODO!!!

lp.serialPortInit(window, "COM9")
window.mainloop()
lp.serialPortExit()
