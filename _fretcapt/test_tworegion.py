import sys					# for stdout printing
from time import sleep		# for sleep
from pprint import pprint
import math
import serialport
reload(serialport)

import zlab
import camera
import pump
import shutter
import valve
import stage

#rollbackimporter.rbi.mark()


try:
	serialport.SerialPort.simulate = ( int(zlab.optionGet('simulateSerialDevices')) == 1 );
	if serialport.SerialPort.simulate:
		print "SIMULATING SERIAL DEVICES (see options.cfg or local.cfg and change simulateSerialDevices=1 to change this)"
except: pass

class Config:
	pumpPort = "COM4,9600,8,n,1,-,-"
	valvePort = "COM5,9600,8,n,1,-,-"
	stagePort = "COM3,9600,8,n,1,-,-"
	valveLiquidNameList = { 'drain': 1, 'water': 1, 'beer': 2, 'wine': 3 }


class Grid:
	def __init__(self):
		self.calibrated = False
		self.head = stage.Vec2(0.0,0.0)
		self.tail = stage.Vec2(1200.0,1200.0)
		self.first = stage.Vec2(200.0,200.0)
		self.overlap = stage.Vec2(0.10,0.10)
		self.step = stage.Vec2(0.0,0.0)
		self.count = stage.Vec2(0.0,0.0)

	def xSpan(self):
		return (self.first.x - self.head.x) * 2.0
		
	def ySpan(self):
		return (self.first.y - self.head.y) * 2.0
		
	def calculate(self):
		xHalfSpan = self.first.x - self.head.x;
		yHalfSpan = self.first.y - self.head.y;
		
		last = stage.Vec2(self.tail.x-xHalfSpan,self.tail.y-yHalfSpan)
	
		# first calculate how many rough steps would be needed
		step  = stage.Vec2( xHalfSpan * 2.0 * (1.0-self.overlap.x), yHalfSpan * 2.0 * (1.0-self.overlap.y) )
		count = stage.Vec2( 1.0 + (last.x-self.first.x) / step.x, 1.0 + (last.y-self.first.y) / step.y )
		#print "c.x={0}   c.y={1}".format(count.x,count.y)
		
		# now figure out how many even steps there should be
		self.count.x = int(math.ceil(count.x))
		self.count.y = int(math.ceil(count.y))
		
		# now how many steps to perfectly encompass the slide?
		xActualDist = (last.x - self.first.x)
		yActualDist = (last.y - self.first.y)
		self.step  = stage.Vec2( xActualDist / self.count.x, yActualDist / self.count.y )
		
		# and determine the actual overlap this represents...
		self.overlap.x = 1 - (self.step.x / (xHalfSpan*2.0))
		self.overlap.y = 1 - (self.step.y / (yHalfSpan*2.0))

		#print "self.overlap.x = {0} / {1} = {2}".format(self.step.x,(xHalfSpan*2.0),self.overlap.x)
		#print "self.overlap.y = {0} / {1} = {2}".format(self.step.y,(yHalfSpan*2.0),self.overlap.y)
		
		self.calibrated = True
	
	def tellCalibration(self):
		if not self.calibrated:
			s = "Stage is not yet calibrated."
		else:
			s = "TopLeft =     ({0},{1})\nBottomRight = ({2},{3})\nGrid =        {4} x {5}\nSteps =       {6} x {7} with {8}% x {9}% overlap".format(self.head.x,self.head.y,self.tail.x,self.tail.y,self.count.x,self.count.y,self.step.x,self.step.y,int(100.0*self.overlap.x),int(100.0*self.overlap.y))
		return s
		
class Controller:

	def __init__(self):
		self.config = Config()
		self.running = False
		self.grid = Grid()
		self.quadrantIndex = -1
		self.pump = None
		self.shutter = None
		self.valve = None
		self.stage = None
		self.camera = None

	def ask(self,message,legalResponses):
		print message
		while True:
			c = zlab.getch()
			if ord(c) == 27:
				print "Halting the program."
				sys.exit()
			if c in legalResponses:
				return c
		
	def startDevices(self):
		print "Connecting to shutters"
		self.shutter = shutter.Shutter()
		self.shutter.initialize()
		print "Connecting to pump"
		self.pump = pump.Pump(self.config.pumpPort)
		self.pump.initialize()
		print "Connecting to valve"
		self.valve = valve.Valve(self.config.valvePort)
		self.valve.setLiquids( self.config.valveLiquidNameList )
		print "Connecting to stage"
		self.stage = stage.Stage(self.config.stagePort)
		print "Starting camera"
		self.camera = camera
		self.camera.start()

	def wash(self,liquidName,amount):
		self.pump.openInput()
		self.valve.setPosition(liquidName)
		self.pump.aspirate(amount)
		self.pump.openOutput()
		self.pump.dispense(amount)

	def checkStartConditions(self):
		pos = self.pump.getPos()
		if pos != 0:
			c = self.ask("WARNING: the pump is not in the right position.\nD)ispense liquid now, or C)ontinue?",'dc')
			if c == 'D':
				self.valve.setPos('drain')
				self.pump.openOutput()
				self.pump.dispense(pos)
	
	def calibGather(self):
		minMargin = 10	# make sure the user move the stage at least a LITTLE BIT to define the origin frame
		
		print "\n"
		
		if False:
			self.grid.head = stage.Vec2(-259835.0,-13671.0)
			self.grid.first = stage.Vec2(-212780.0,32560.0)
			self.grid.tail = stage.Vec2(14558.0,94110.0)
		else:
			while self.ask( "TOP LEFT\n   Use the joystick to move the stage so that the TOP LEFT CORNER\n   of the slide is CENTERED in the camera, then hit 'C'.",'c') != 'c':
				pass
			self.grid.head = self.stage.getPos()

			while True:
				while self.ask( "ORIGIN\n   Now move the stage so that the camera frames the top left\n   part of the slide, then hit 'C'.",'c') != 'c':
					pass
				self.grid.first = self.stage.getPos()
				if self.grid.first.x <= self.grid.head.x+minMargin or self.grid.first.y <= self.grid.head.y+minMargin:
					print "ERROR: Please move the stage appropriately."
				else:
					break
			while True:
				while self.ask( "BOTTOM LEFT\n   Move the stage so the BOTTOM LEFT CORNER of\n   the slide in centered in the camera, then hit 'C'.",'c') != 'c':
					pass
				self.grid.tail = self.stage.getPos()
				xSpan = self.grid.first.x - self.grid.head.x
				ySpan = self.grid.first.y - self.grid.head.y
				if self.grid.tail.x-self.grid.head.x <= xSpan*2 or self.grid.tail.y-self.grid.head.y <= ySpan*2:
					print "ERROR: Please move the stage appropriately."
				else:
					break
		
		self.grid.calculate()
		print "\n"
		self.grid.tellCalibration()

	def calibSetup(self):
		c = ''
		while c != 'c':
			print self.grid.tellCalibration()
			if self.grid.calibrated:
				c = self.ask("S)etup Stage or C)ontinue?",'sc')
			if c != 'c':
				self.calibGather()

	def wash(self,liquidName,amount,askAfter):
		while True:
			print "\nWASH: {0} {1}".format(amount,liquidName)
			self.pump.openInput()
			self.pump.aspirate(amount)
			self.valve.setPos(liquidName)
			self.pump.openOutput()
			self.pump.dispense(amount)
			if not askAfter:
				break
			c = self.ask( "What next?  C)ontinue   R)ewash   P)ick new liquid", 'crp' )
			if c == 'c':
				break
			if c == 'r':
				continue
			print "Pick new liquid not yet implemented.  Continuing..."
			break;

	def moveToNext(self):
		self.quadrantIndex += 1
		xq = int(self.quadrantIndex % self.grid.count.x)
		yq = int(self.quadrantIndex / self.grid.count.y)
		print "\nQUADRANT {0} x {1}".format(xq,yq)
		x = self.grid.first.x + xq * self.grid.xSpan()
		y = self.grid.first.y + yq * self.grid.ySpan()
		self.stage.setPos(x,y)
		return self.stage.getPos()

	def moveReset(self):
		self.quadrantIndex = -1
		
	def record(self):
		exposureTime = 1.0
		print "\nRECORDING exposure={0}".format(exposureTime)
		self.camera.record()
		self.shutter.set('green',1)
		sleep(exposureTime)
		self.shutter.set('green',0)
		self.shutter.set('red',1)
		sleep(exposureTime)
		self.shutter.set('red',0)
		self.camera.stop()

	def writeFile(self):
		print "Imagine the file was written."
		
	def run(self):
		self.checkStartConditions()
	
		self.calibSetup()
		
		quadrantCount = self.grid.count.x * self.grid.count.y

		print "PHASE 1 - recording baseline\n"
		while self.quadrantIndex < quadrantCount:
			stagePos = self.moveToNext()
			self.record()
			self.writeFile(stagePos)

		print "PHASE 2 - washing\n"
		self.wash('beer',400,True)
		self.wash('wine',400,True)
		
		print "PHASE 3 - recording post-wash\n"
		self.moveReset()
		while self.quadrantIndex < quadrantCount:
			stagePos = self.moveToNext()
			self.record()
			self.writeFile(stagePos)
		
			
		
print "Test of a two-region scan."
c = Controller()
c.startDevices()
#print "\n--------------------------------------\n"
#c.run()
print "PHASE 2 - washing\n"
c.wash('beer',400,True)

