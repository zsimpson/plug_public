import ctypes
from time import sleep		# for sleep
import serialport
import sys


#
#  SHUTTER
#==============================================================
class Shutter(object):
	nidaqDll = None

	def isSimulating(self):
		return serialport.SerialPort.simulate;
	
	def __init__(self,channels = None):
		self.report = serialport.Reporter()
		self.nameToChannel = channels if channels != None else { 'blue':1, 'green':3, 'red':6 }
		self.isInit = False
		if not self.isSimulating():
			if self.nidaqDll == None:
				try:
					self.nidaqDll = ctypes.windll.LoadLibrary('nidaq32.dll') # expected in c:\windows\system32
					print "Loaded nidaq32.dll"
				except:
					print "Unable to load the nidaq32.dll to control the shutters."
					print "Check to be sure the file 'nidaq32.dll' is present in c:/windows/system32"
					print "Or perhaps it is being used by another program? Or perhaps you actually intend to be simulating devices?"
					raise Exception("Halting due to missing DLL.")
		
	def getModel(self):
		return "Uniblitz VMM-D1/T1"

	def getManualUrl(self):
		return "http://www.uniblitz.com/resources_filelibrary/vmm-t1_d1_user_manual_1_4.pdf"

	def initialize(self):
		# Initialize
		self.report.status("Initializing shutters\n")
		if self.isSimulating():
			return
		self.nidaqDll.DIG_Prt_Config( 1, 0, 0, 1 )
		self.nidaqDll.DIG_Out_Port( 1, 0, 1 )
		self.nidaqDll.DIG_Out_Line( 1, 0, 3, 1 )
		self.nidaqDll.DIG_Out_Line( 1, 0, 6, 1 )
		self.nidaqDll.DIG_Out_Line( 1, 0, 1, 1 )

	def openPort(self):
		return	# this exists simply for compatibility with the devices that use COM ports
		
	def close(self):
		return	# this exists simply for compatibility with the devices that use COM ports
		
	def set(self,channelName,state):
		if not self.isInit:
			print "Warning: call shutter.initialize() before calling shutter.set() for more efficient operation."
			self.initialize()
		if channelName not in self.nameToChannel:
			return 0
		channel = self.nameToChannel[channelName]
		if state != 1 and state != 0:
			state = 0
		stateName = ['off','on']
		self.report.status( "set {0} shutter {2}\n".format(channelName,channel,stateName[state]) )
		if not self.isSimulating():
			self.nidaqDll.DIG_Out_Line( 1, 0, channel, 1-state )
		return 1

def testShutter():
	print "Testing Shutters."
	
	shutter = Shutter()
	shutter.initialize()
	
	for name,channel in shutter.nameToChannel.iteritems():
		shutter.set(name,0)
		sleep(0.2)
		shutter.set(name,1)
		sleep(0.2)
		shutter.set(name,0)
		sleep(0.2)
		shutter.set(name,1)
		sleep(0.2)
		shutter.set(name,0)
	shutter.close()
	
