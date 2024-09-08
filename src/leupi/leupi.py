# Lineside Electronic Unit Python Interface by KVP

import serial
import threading
from dataclasses import dataclass
from collections import deque

@dataclass
class SerialPortDevice:
	addr    : str
	type    : int
	status  : int
	signals : int
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
	global serialCommand
	global serialQueue
	devLen = len(serialPortDevList)
	if (devLen < 1):
		return
	dev = serialPortMap[serialPortDevList[serialPortDevCnt]]
	print("address=" + hex(dev.addr) + " type=" + hex(dev.type))
	if (dev.type == 1):
		serialCommand = "SS " + hex(dev.addr)[2:] + " " + hex(dev.signals)[2:]
	elif (dev.type == 2):
		serialCommand = "PG " + hex(dev.addr)[2:] # TODO!!!
	elif (dev.type == 3):
		serialCommand = "SG " + hex(dev.addr)[2:]
	elif (dev.type == 4):
		serialCommand = "MP " + hex(dev.addr)[2:] + " " + str(dev.pulses[0]) + " " + str(dev.pulses[1]) + " " + str(dev.pulses[2]) + " " + str(dev.pulses[3])
	else:
		serialCommand = "PG " + hex(dev.addr)[2:]
	serialPortWrite(serialCommand)
	serialPortDevCnt = (serialPortDevCnt + 1) % devLen

def serialParseResponses(command, response, queue):
	global serialPortMap
	if (command.startswith("SG ")):
		cl = command.split(" ")
		rl = response.split(" ")
		if ((len(cl) < 2) or (len(rl) < 14)):
			return
		#print (cl, rl)
		address = int(cl[1], 16)
		if (not (address in serialPortMap)):
			return
		sl = [address]
		sensor = serialPortMap[address]
		for t in range (0,11):
			sensor.sensors[t] = int(rl[2 + t], 16)
			sl.append(sensor.sensors[t])
		#print(sl)
		queue.append(["SENSOR", sl])

def serialPortThread():
	global serialPort
	global serialPortWindow
	global serialPortName
	global serialPortExitFlag
	global serialPortMap
	global serialPortDevList
	global serialPortDevCnt
	global serialPortInitFast
	global serialPortInitDevices
	global serialCommand
	global serialQueue
	serialPortExitFlag = False
	serialPortMap = {}
	serialQueue = deque()
	serialPort = serial.Serial(port=serialPortName, baudrate=115200, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)
	serialPortWrite("help")
	serialString = ""
	serialPortPnp = True
	serialPortDevList = []
	serialPortDevCnt = 0
	while (not serialPortExitFlag):
		try:
			serialString = serialPort.readline().decode("iso-8859-2").strip()
			if (serialString != ""):
				print(">" + serialString)
				if (serialString.startswith("#")):
					pass
				if (serialPortPnp):
					if (serialString.startswith("READY")):
						if (serialPortInitFast):
							for dev in serialPortInitDevices:
								addr = dev[0]
								type = dev[1]
								stat = 0
								print("address=" + hex(addr) + " type=" + hex(type) + " status=" + hex(stat))
								dev = SerialPortDevice(addr, type, stat, 0, [0,0,0,0], [0,0,0,0], [0,0,0,0,0,0,0,0,0,0,0,0])
								serialPortMap[addr] = dev
								serialPortDevList.append(addr)
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
					if (not serialString.startswith("#")):
						serialQueue.append([serialCommand, serialString])
						serialParseResponses(serialCommand, serialString, serialQueue)
					if (serialString.startswith("OK") or serialString.startswith("E")): # TODO!!! list error codes here
						serialPortSendNext()
		except Exception as e:
			print("Exception: " + str(e))

def serialPortQuePoll():
	if (len(serialQueue) < 1):
		return ["", ""]
	msg = serialQueue[0]
	serialQueue.popleft()
	return msg

def serialPortInit(serialPort, window, callback):
	global serialPortWindow
	global serialPortCallback
	global serialPortName
	serialPortWindow = window
	serialPortCallback = callback
	serialPortName = serialPort
	th = threading.Thread(target=serialPortThread)
	th.start()

def serialPortInitFast(serialPort, window, callback, devices):
	global serialPortWindow
	global serialPortCallback
	global serialPortName
	global serialPortInitFast
	global serialPortInitDevices
	serialPortWindow = window
	serialPortCallback = callback
	serialPortName = serialPort
	serialPortInitFast = True
	serialPortInitDevices = devices
	th = threading.Thread(target=serialPortThread)
	th.start()

def serialPortExit():
	global serialPortExitFlag
	serialPortExitFlag = True
	try:
		serialPort.close()
	except Exception as e:
		pass

def signalSet(address, aspects):
	global serialPortMap
	if (not (address in serialPortMap)):
		return
	signal = serialPortMap[address]
	signal.signals = aspects

def motorPulse(address, port, value):
	global serialPortMap
	if (not (address in serialPortMap)):
		return
	motor = serialPortMap[address]
	motor.pulses[port] = value

def signalSetAspect(address, offset, size, aspects):
	global serialPortMap
	if (not (address in serialPortMap)):
		return
	mask = ((2 ** size) - 1)
	masks = (mask << offset) | (mask << (offset + 16))
	#print("MASK=" + hex(mask))
	#print("MASKS=" + hex(masks))
	aspect = ((aspects[0] & mask) << offset) | ((aspects[1] & mask) << (offset + 16))
	#print("ASPECTS=" + hex(aspect))
	signal = serialPortMap[address]
	signal.signals = ((signal.signals & ~masks) | aspect)
