from time import sleep		# for sleep
import serialport
from random import random


pumpDefaultPort = "COM4,9600,8,n,1,-,-"


def bound(value,_min,_max):
	if value < _min:
		value = _min
	if value > _max:
		value = _max
	return value

#
#  PUMP
#==============================================================

FLOW_UNKNOWN, FLOW_INPUT, FLOW_OUTPUT = (0,1,2)

class Pump(serialport.SerialPort):
	FLOWDIR = (FLOW_UNKNOWN, FLOW_INPUT, FLOW_OUTPUT)

	def __init__(self,_portString=""):
		super(Pump,self).__init__()
		self.pumpSpeed = 500
		self.flowDir = FLOW_UNKNOWN
		
		if not self.configureFromString(_portString):
			print "Failed to open pump {0}".format(self.portReason)

		if _portString != "":
			self.openPort()
			self.asyncReaderStart()
			
	def getModel(self):
		return "Cavro XP3000"

	def getManualUrl(self):
		return "http://www.flowinjection.com/Brochures/X_ 3000_ Manual.pdf" # Yes, there are spaces after the underscores!

	def isBusy(self):
		oldSeeSerial = self.report.seeSerial
		self.report.seeSerial = False 
		if self.simulate:
			result = random() < 0.5
		else:
			result = self.doCommand("","/1QR",0.1)[-1][2] != '`';
		self.report.seeSerial = oldSeeSerial
		return result
	
	def waitWhileBusy(self):	# Automatically called from doCommand()
		reps = 0
		while self.isBusy():
			if reps > 2:
				self.report.status(".")
			reps += 1
			sleep(0.9)
		return
		
	def initialize(self):
		self.doCommand("initialize()\n","/1ZR")
		
	def aspirate(self,amount):
		# moves the plunger down the number of steps commanded. The new absolute position is the previous position + <n>, where <n> = 0..3000.
		# will return error 3 (invalid operand) if the final plunger position would be greater than 3000.
		if self.flowDir == FLOW_UNKNOWN:
			self.report.error("Warning: Aspirating with pump valve in unknown state.")
		#if self.flowDir == FLOW_OUTPUT:
		#	self.report.error("Warning: Aspirating from the output line.")
		self.doCommand("aspirate({0})\n".format(amount),"/1P{0}R".format(amount))
		self.waitWhileBusy()

	def dispense(self,steps):
		# moves the plunger UPWARD the number of steps commanded. The new absolute position is the previous position - <n>; range is 0..3000.
		# will return error 3 (invalid operand) if the final plunger position would be less than 0.
		if self.flowDir == FLOW_UNKNOWN:
			self.report.error("Warning: Dispensing with pump valve in unknown state.")
		#if self.flowDir == FLOW_INPUT:
		#	self.report.error("Warning: Dispensing into the input line.")
		steps = bound(steps,0,3000)
		self.doCommand("dispense({0})\n".format(steps),"/1D{0}R".format(steps))

	def setPos(self,pos):
		# moves the plunger to the absolute position <n>, where <n> = 0..3000.
		pos = bound(pos,0,3000)
		self.doCommand("setPosition({0})\n".format(pos),"/1A{0}R".format(pos))

	def setSpeed(self,speedHz):
		# sets the velocity at which the plunger begins its movement. The plunger will then ramp up (slope) to the top velocity. 
		# The start velocity should always be less than the top velocity.  <n> = 50..1000 Hz
		# (900 is the default; 901 is the default on the 3-port distribution valve)
		speedHz = bound(speedHz,50,1000)
		self.pumpSpeed = speedHz
		self.doCommand("setSpeed({0}hz)\n".format(speedHz),"/1V{0}R".format(speedHz),0.1)
		# It is important to send the velocity commands in the proper order to insure that all
		# parameters are read. The XP 3000 queries for the input of the velocity commands
		# in the following order: backlash [K], slope [L], start velocity [v], top velocity [V],
		# and cutoff velocity [c]. Not inputting these values in the above order causes the
		# XP 3000 to rely on the default values for these commands.

	def setValve(self,side,force):
		# side is 'left' or 'right'
		# force is 'half' or 'full'
		# initializes the plunger drive and sets valve output to the right (as viewed from the front of the pump).
		if side=='left':
			sideChar = "Y"
		else:
			sizeChar = "Z"
		forceChar = "1" if force=='half' else "0"
		self.doCommand("setValve()\n","/1"+sideChar+forceChar+"R");

	def openInput(self):
		self.flowDir = FLOW_INPUT
		self.doCommand("openInput()\n","/1IR");

	def openOutput(self):
		self.flowDir = FLOW_OUTPUT
		self.doCommand("openOutput()\n","/1OR");

	def getPos(self):
		bufList = self.doCommand("getPos()\n","/1?R");
		if self.simulate:
			bufList = ["xxx999\x03\0x0D"]
		buf = bufList[-1]
		try:
			start = 3
			end = buf.index('\x03')
		except:
			self.report.error( "Failed to parse position from {0}".format(buf) )
			return -1
		try:
			return int(buf[start:end])
		except:
			self.report.error( "Failed to parse position from {0} start={1} end={2}".format(buf,start,end) )
			return -1

	def query(self):
		return self.doCommand("query()","/1QR",1)[-1];
		
	def test(self,str):
		self.doCommand("test command","/"+str+"R",1);

	def raw(self,str):
		self.doCommand("raw command",str,1);

def testPump(port=pumpDefaultPort):
	print "Testing Pump."
	
	pump = Pump(port)

	pump.initialize()
	print "Pump is at position %d" % pump.getPos()
	pump.setSpeed(500)	# this is default
	pump.openInput()
	pump.setPos(0)
	pump.aspirate(2000)
	print "Pump is at position %d" % pump.getPos()
	pump.openOutput()
	pump.dispense(1000)
	print "Pump is at position %d" % pump.getPos()
	pump.setSpeed(50)
	pump.openInput()
	pump.aspirate(100)
	pos = pump.getPos()
	print "Pump is at position %d" % pos
	pump.openOutput()
	pump.dispense(100)
	print "Pump is at position %d" % pump.getPos()
	pump.close()
