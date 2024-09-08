import sys
import leupi as lp
import tkinter as tk
from PIL import ImageTk, Image, ImageDraw
from enum import Enum

def testSetSignalR():
	#lp.signalSet(0x42, 0x02000200)
	lp.signalSetAspect(0x42, 8, 3, [0x02, 0x02])

def testSetSignalYG():
	#lp.signalSet(0x42, 0x05000500)
	lp.signalSetAspect(0x42, 8, 3, [0x05, 0x05])

def testSetSignalYblink():
	#lp.signalSet(0x42, 0x04000000)
	lp.signalSetAspect(0x42, 8, 3, [0x04, 0x00])

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
button3 = tk.Button(master=frame, text ="yellow blinking", command = testSetSignalYblink)
button3.place(x=10, y=70)



if len(sys.argv) < 2:
	print("Usage: test1.py <serial_port>");
	print("  ex: python test1.py COM9");
	exit(1)
lp.serialPortInitFast(window, sys.argv[1], [[0x41, 3],[0x42, 1],[0x43, 4]])
window.mainloop()
lp.serialPortExit()
