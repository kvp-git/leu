import leupi as lp
import tkinter as tk
from PIL import ImageTk, Image, ImageDraw
from enum import Enum

def testSetSignalR():
	lp.signalSet(0x42, 0x55aaaa55)

def testSetSignalYG():
	lp.signalSet(0x42, 0xffff0000)

window = tk.Tk()
window.title("This is an application")
window.minsize(640, 480)
window.maxsize(640, 480)

frame = tk.Frame(master=window, width=640, height=480, bg="green")
frame.place(x=0, y=0)

button1 = tk.Button(master=frame, text ="red", command = testSetSignalR)
button1.place(x=10, y=10)
button2 = tk.Button(master=frame, text ="yellow + green", command = testSetSignalYG)
button2.place(x=10, y=40)



# TODO!!!

lp.serialPortInit(window, "COM9")
window.mainloop()
lp.serialPortExit()
