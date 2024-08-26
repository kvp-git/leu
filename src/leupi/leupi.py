import serial
import threading
from dataclasses import dataclass

@dataclass
class SerialPortDevice:
    addr   : str
    type   : int
    status : int

def serialPortWrite(command):
	global serialPort
	cmd = command + "\n"
	serialPort.write(cmd.encode("iso-8859-2"))

def serialPortSendNext():
	global serialPortMap
	global serialPortDevList
	i = 0
	while (i < len(serialPortDevList)):
		print(serialPortDevList[i])
		print(serialPortMap[serialPortDevList[i]])
		i = i + 1

def serialPortThread():
	global serialPort
	global serialPortWindow
	global serialPortName
	global serialPortExitFlag
	global serialPortMap
	global serialPortDevList
	serialPortExitFlag = False
	serialPortMap = {}
	serialPort = serial.Serial(port=serialPortName, baudrate=115200, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)
	serialPortWrite("help")
	serialString = ""
	serialPortPnp = False
	serialPortDevList = []
	while not serialPortExitFlag:
		serialString = serialPort.readline().decode("iso-8859-2").strip()
		try:
			if (serialString != ""):
				print(">" + serialString)
				if (serialString.startswith("#")):
					pass
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
							print("address=0x" + cmd[2] + " type=0x" + cmd[3] + " status=0x" + cmd[4])
							print("    " + str(type) + " " + str(stat))
							dev = SerialPortDevice(addr, type, stat)
							serialPortMap[addr] = dev
							serialPortDevList.append(addr)
				if (serialString.startswith("OK") and serialPortPnp):
					serialPortPnp = False
					print(serialPortDevList)
					print(serialPortMap)
					serialPortSendNext()
				else:
					pass
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
