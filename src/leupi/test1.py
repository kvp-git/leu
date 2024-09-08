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

def testPulse1():
	lp.motorPulse(0x43, 0, 1000)

def testPulse2():
	lp.motorPulse(0x43, 0, -200)

def testPulse3():
	lp.motorPulse(0x43, 1, 8191)

def testPulse4():
	lp.motorPulse(0x43, 1, -8191)

def testPulse5():
	lp.motorPulse(0x43, 1, 0)

def testPulse6():
	lp.motorPulse(0x43, 0, 0)


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

button4 = tk.Button(master=frame, text ="pulse 1 left 1000", command = testPulse1)
button4.place(x=400, y=10)
button5 = tk.Button(master=frame, text ="pulse 1 right 200", command = testPulse2)
button5.place(x=400, y=40)
button6 = tk.Button(master=frame, text ="pulse 1 stop (reset)", command = testPulse6)
button6.place(x=400, y=70)
button7 = tk.Button(master=frame, text ="pulse 2 left continous", command = testPulse3)
button7.place(x=400, y=100)
button8 = tk.Button(master=frame, text ="pulse 2 right continous", command = testPulse4)
button8.place(x=400, y=130)
button9 = tk.Button(master=frame, text ="pulse 2 stop", command = testPulse5)
button9.place(x=400, y=160)

label1 = tk.Label(master=frame, text="commands/replys", bg="white")
label1.place(x=160, y=10)

label2 = tk.Label(master=frame, text="Sensor 0x41=", bg="white")
label2.place(x=160, y=60)

def testSerialIn(command, reply):
	label1.config(text=(command + "\n" + reply))
	if (command.startswith("SG 41")):
		label2.config(text=("Sensor 0x41=" + reply))

def testQue():
	global window
	msg = lp.serialPortQuePoll()
	if (msg[0] != ""):
		#print("TEST:" + str(msg))
		testSerialIn(msg[0], msg[1])
	window.after(10, testQue) #serialPortCallback, serialCommand, serialString)

# main:
try:
	if len(sys.argv) < 2:
		print("Usage: test1.py <serial_port>");
		print("  ex: python test1.py COM9");
		exit(1)
	lp.serialPortInitFast(sys.argv[1], window, testSerialIn, [[0x41, 3],[0x42, 1],[0x43, 4]])
	window.after(10, testQue)
	window.mainloop()
	lp.serialPortExit()
except Exception as e:
	print("Exception: " + str(e))
	lp.serialPortExit()
	#window.destory()
