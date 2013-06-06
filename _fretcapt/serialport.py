import sys					# for stdout printing
import traceback			# for exception handling
from time import sleep		# for sleep
import threading
import serial
import inspect
import atexit

# Uses PySerial
# Documented here: http://pyserial.sourceforge.net
# To install it, grab from http://pypi.python.org/pypi/pyserial and unzip just the 'serial' directory
# For useful interactive python sessions, consider using DreamPie

WAIT_WHILE_BUSY = -1

#
# converts strings that might have non-readable values to show the vex halues as well
#
def getHumanReadable(str,spacer=''):
	commandHumanReadable = ""
	for c in str:
		if c <= ' ' or c > '~':
			commandHumanReadable += c.encode('hex').upper()+spacer
		else:
			commandHumanReadable += c+spacer
	return commandHumanReadable

#
# closeAll()
# Keeps track of open ports, especially during interactive debugging sessions, and closes them
# when needed
#
def closeAll(notification=""):
	global openPortList
	if notification:
		print notification
	anyFound = False
	while len(openPortList) > 0:
		anyFound = True
		if openPortList[0].port:
			openPortList[0].close()	# deletes itself from the list
	if not anyFound:
		print "No open serial ports found."
		
if 'openPortList' not in globals():
	print "Registering port closer."
	openPortList = []
	atexit.register(closeAll)
else:
	if len(openPortList) > 0:
		closeAll()	
	# closing hanging ports from a prior run (in DreamPie, for example)
	# ALSO will be called a second time if one called reload(serialport) for debugging purposes

#
# InfoHandler
# Makes reporting more intuitive in partnership with Reporter.  You shouldn't have to make changes here
#
class InfoHandler(object) :
	def __init__(self,name,owner):
		self.name = name
		self.owner = owner
	def __call__(self,str):
		self.owner.report(self.name,str)

#
# Reporter
# Include this in classes to make it easier to direct output
#
class Reporter(object):
	def __init__(self):
		self.seeStatus = False
		self.seeDebug = False
		self.seeSerial = False
		self.status = InfoHandler('status',self)
		self.debug = InfoHandler('debug',self)
		self.error = InfoHandler('error',self)
		self.read = InfoHandler('read',self)
		self.write = InfoHandler('write',self)

	def report(self, name, str):
		try:
			if name == 'error':
				sys.stdout.write( "Error: %s\n" % str )
			elif name == 'write':
				if self.seeDebug or self.seeSerial: sys.stdout.write( "[ %s]\n" % getHumanReadable(str,' ') )
			elif name == 'read':
				if self.seeDebug or self.seeSerial: sys.stdout.write( "< %s>\n" % getHumanReadable(str,' ') )
			elif name == 'debug':
				if self.seeDebug: sys.stdout.write( "%s" % str )
			else:
				if self.seeStatus: sys.stdout.write( "%s" % str )
		except:
			print "Lost the print file descriptor.\n  name={0}\n  string={1}".format(name,str)
#
# SerialPort
# This is the meat and potatos.  It layers over PySerial and makes dealing with serial ports simpler
#
class SerialPort(object):
	registeredPortCloser = False
	simulate = False
	
	def __init__(self):
		self.report = Reporter()
		self.port = serial.Serial()
		self.portValid = 0
		self.portReason = "Uninitialized"
		self.commandTerminator = "\x0D"
		
		self.readAccumulator = ""
		self.readBuffer = []
		
		self.asyncReaderAlive = False
		#print "sim={0}".format(self.simulate)

	def configureFromString(self,serialInfo):
		toBits   = { '7': serial.SEVENBITS, '8': serial.EIGHTBITS }
		toParity = { 'e': serial.PARITY_EVEN, 'o': serial.PARITY_ODD, 'n': serial.PARITY_NONE }
		toStop   = { '1': serial.STOPBITS_ONE, '2': serial.STOPBITS_TWO }
		toXon    = { 'x': True, '-': False }
		toFlow   = { 'R': True, '-': False }
		
		def keyGet(hash,key,text):
			if key not in hash:
				choices = ""
				for k in hash:
					choices += k
				self.portReason = "Error initializing {0} with {1}. The {0} must be one of {2}".format(text,key,choices)
				return None
			return hash[key]
		
		# should be in the form port,baud,databits,parity,stop
		param = serialInfo.split(",")
		
		try:
			self.port.portstr = param[0]
			self.port.baudrate = param[1]
			self.port.bytesize = keyGet(toBits,param[2],'frame bits')
			self.port.parity = keyGet(toParity,param[3],'parity')
			self.port.stopBits = keyGet(toStop,param[4],'stop bits')
			self.port.xonxoff = keyGet(toXon,param[5],'handshake (x-on / x-off)')
			self.port.rtscts = keyGet(toFlow,param[6],'hardware flow control')
			self.port.writeTimeout = 1
			
			self.portValid = 1
			self.portReason = ""
		except:
			self.portValid = 0
			self.portReason = sys.exc_info()
			return False
		return True

	def openPort(self):
		self.report.status( "Opening port {0}\n".format(self.port.portstr) )
		if not self.simulate:
			self.port = serial.Serial(
				port=self.port.portstr,
				baudrate=self.port.baudrate,
				parity=self.port.parity,
				stopbits=self.port.stopbits,
				bytesize=self.port.bytesize,
				xonxoff=self.port.xonxoff,
				rtscts=self.port.rtscts,
				timeout=1
				)
		global openPortList
		#print "Adding to openPortList"
		openPortList.append(self)
		self.port.setInterCharTimeout(5)
		
	def asyncReaderStart(self):
		"""Start reader thread"""
		self.asyncReaderAlive = True
		self.receiver_thread = threading.Thread(target=self.asyncReader)
		self.receiver_thread.setDaemon(True)
		self.receiver_thread.start()

	def asyncReaderStop(self):
		"""Stop reader thread only, wait for clean exit of thread"""
		self.asyncReaderAlive = False
		self.receiver_thread.join()

	def asyncReader(self):
		try:
			while self.asyncReaderAlive:
				if self.simulate:
					data = ""
				else:
					data = self.port.read(1)
				for c in data:
					self.readAccumulator += c
					if c == "\n":
						self.readBuffer.append(self.readAccumulator)
						self.readAccumulator = ""
					#self.report.read( "%s %s" % (c.encode('hex'), c) )
		except serial.SerialException, e:
			self.asyncReaderAlive = False
			self.report.error( "Serial async reader for {0} halted.".format(self.port.portstr) )

	def read(self):
		if self.asyncReaderAlive:
			temp = self.readBuffer
			self.readBuffer = []
			return temp
				
		else:
			lineRetrieved = False
			while not lineRetrieved:
				if self.simulate:
					data = ""
				else:
					data = port.read(port.inWaiting())
				for c in data:
					self.readAccumulator += c
					if c == "\n":
						self.readBuffer.append(self.readAccumulator)
						self.readAccumulator = ""
						lineRetrieved = True
			
	def write(self,command):
		command = command + self.commandTerminator
		self.report.write(command)
		if not self.simulate:
			self.port.write( command )
		# Or if unlucky will have to use: chr(int("0D",16))

	# Returns a list of the responses retrieved from the device.
	# if duration is -1 it waits until self.isBusy() is false - you should overload isBusy()
	def _doCommand(self,commandName,command,duration):
		if commandName != '':
			self.report.status(commandName);
			
		self.write(command)
		
		if duration > 0:
			sleep(duration)
		if duration == WAIT_WHILE_BUSY:
			sleep(0.1)
			
		buf = self.read()
		for line in buf:
			self.report.read(line)

		if len(buf) <= 0:
			return [""]
		return buf

	def waitWhileBusy(self):	# overload me
		return

	# WARNING: This returns a LIST of the strings received from the serial device,
	# not just the last string.
	def doCommand(self,commandName,command,duration=WAIT_WHILE_BUSY):
		result = self._doCommand(commandName,command,duration)
		if duration == WAIT_WHILE_BUSY:
			self.waitWhileBusy();
		return result
		
	def close(self):
		if self.asyncReaderAlive:
			self.report.debug( "Halting reader... " )
			self.asyncReaderStop()
			self.report.debug( "Halted.\n" )
		self.report.status( "Closing port {0}... ".format(self.port.portstr) )
		if not self.simulate:
			self.port.close()
		self.portValid = 0
		self.port = None
		self.report.debug( "Closed.\n" )
		global openPortList
		removeIndex = openPortList.index(self)
		del openPortList[removeIndex]
		
