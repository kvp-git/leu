# Lineside Electronic Unit Python Interface by KVP

import serial
import threading
from dataclasses import dataclass

@dataclass
class SerialPortDevice:
	addr    : str
	type    : int
	status  : int
	signals : list
	speeds  : list[int]
	pulses  : list[int]
	sensors : list[int]

# DECODER_SIGNAL = 1,
# DECODER_MOTOR = 2,
# DECODER_SENSOR = 3,
# DECODER_PULSE = 4,

def serialPortWrite(command):
	global serialPort
	print("<" + command)
	cmd = command + "\n"
	serialPort.write(cmd.encode("iso-8859-2"))

def serialPortSendNext():
	global serialPortMap
	global serialPortDevList
	global serialPortDevCnt
	devLen = len(serialPortDevList)
	if (devLen < 1):
		return
	dev = serialPortMap[serialPortDevList[serialPortDevCnt]]
	print("address=" + hex(dev.addr) + " type=" + hex(dev.type))
	#serialPortWrite("PG " + hex(dev.addr)[2:])
	if (dev.type == 1):
		serialPortWrite("SS " + hex(dev.addr)[2:] + " " + hex(dev.signals)[2:])
	else:
		serialPortWrite("PG " + hex(dev.addr)[2:])
	serialPortDevCnt = (serialPortDevCnt + 1) % devLen

def serialPortThread():
	global serialPort
	global serialPortWindow
	global serialPortName
	global serialPortExitFlag
	global serialPortMap
	global serialPortDevList
	global serialPortDevCnt
	serialPortExitFlag = False
	serialPortMap = {}
	serialPort = serial.Serial(port=serialPortName, baudrate=115200, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)
	serialPortWrite("help")
	serialString = ""
	serialPortPnp = True
	serialPortDevList = []
	serialPortDevCnt = 0
	while not serialPortExitFlag:
		serialString = serialPort.readline().decode("iso-8859-2").strip()
		try:
			if (serialString != ""):
				print(">" + serialString)
				if (serialString.startswith("#")):
					pass
				if (serialPortPnp):
					if (serialString.startswith("READY")):
						serialPortPnp = True
						serialPortWrite("PL")
					if (serialString.startswith("info ")):
						cmd = serialString.split(" ")
						if (len(cmd) >= 3):
							if (cmd[2] == "OK"):
								addr = int(cmd[1], 16)
								type = int(cmd[3], 16)
								stat = int(cmd[4], 16)
								print("address=" + hex(addr) + " type=" + hex(type) + " status=" + hex(stat))
								dev = SerialPortDevice(addr, type, stat, 0, [0,0,0,0], [0,0,0,0], [0,0,0,0,0,0,0,0,0,0,0,0])
								serialPortMap[addr] = dev
								serialPortDevList.append(addr)
					if (serialString.startswith("OK")):
						serialPortPnp = False
						print(serialPortDevList)
						print(serialPortMap)
						i = 0
						while (i < len(serialPortDevList)):
							print(serialPortDevList[i])
							print(serialPortMap[serialPortDevList[i]])
							i = i + 1
						serialPortSendNext()
				else:
					if (serialString.startswith("OK") or serialString.startswith("E")): # TODO!!! list error codes here
						serialPortSendNext()
				#serialPortWindow.after(1, serialIn, serialString)
		except Exception as e:
			print("Exception: " + str(e))

def serialPortInit(window, serialPort):
	global serialPortWindow
	global serialPortName
	serialPortWindow = window
	serialPortName = serialPort
	th = threading.Thread(target=serialPortThread)
	th.start()

def serialPortExit():
	global serialPortExitFlag
	serialPortExitFlag = True

def serialPortSendCommand(command):
	pass # TODO!!!

def signalSet(address, aspects):
	global serialPortMap
	if (not (address in serialPortMap)):
		return
	signal = serialPortMap[address]
	signal.signals = aspects
