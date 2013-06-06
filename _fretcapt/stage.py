from time import sleep		# for sleep
import serialport
from random import random

stageDefaultPort = "COM3,9600,8,n,1,-,-"

class Vec2:
	def __init__(self,_x,_y):
		self.x = float(_x);
		self.y = float(_y);
		
#
#  STAGE
#==============================================================
class Stage(serialport.SerialPort):
	xReverse = True

	def __init__(self,_portString=stageDefaultPort):
		super(Stage,self).__init__()
		self.standardDelayAfterCommand = 0.1

		if not self.configureFromString(_portString):
			print "Failed to open stage {0}".format(self.portReason)
			
		if _portString != "":
			self.openPort()
			self.asyncReaderStart()
		
	def getModel(self):
		return "Applied Scientific Instruments MS2000 XY Inverted Stage"

	def getManualUrl(self):
		return "http://www.asiimaging.com/ftp2asi/Manuals/MS2000%20Programming.pdf"

	def isBusy(self):
		oldSeeSerial = self.report.seeSerial
		self.report.seeSerial = False
		if self.simulate:
			result = random() < 0.5
		else:
			result = self.doCommand("","STATUS",0.1)[-1][0] != 'N'
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
		
	def getPos(self):
		buf = self.doCommand("stage.getPos() = ","WHERE X Y",self.standardDelayAfterCommand)
		if self.simulate:
			buf = ["/:A 299 399 499"]
		result = buf[-1]
		try:
			xIndex = result.index(' ')		# first coordinate
			yIndex = result.index(' ',xIndex+1)	# second coord
			endIndex = result.index(' ',yIndex+1)	# second coord
		except:
			return (-9999999,-9999999)
		pos = Vec2( int(result[xIndex+1:yIndex]), int(result[yIndex+1:endIndex]) )
		if self.xReverse:
			pos.x = -pos.x
		self.report.status( "(%d,%d)\n" % (pos.x,pos.y) )
		return pos

	def setPos(self,x,y):
		if self.xReverse:
			x = -x
		buf = self.doCommand("setPos({0},{1})\n".format(x,y),"MOVE X=%d Y=%d" % (x,y))
		
def testStage(port=stageDefaultPort):
	print "Testing Stage."
	stage = Stage(port)
	#stage.report.seeDebug = True
	stage.setPos(0,0)
	stage.setPos(250,175)
	print "The stage is now at (%d,%d)" % (stage.getPos().x,stage.getPos().y)
	stage.setPos(-100000,-100000)
	print "The stage is now at (%d,%d)" % (stage.getPos().x,stage.getPos().y)
	stage.setPos(-100000,100000)
	print "The stage is now at (%d,%d)" % (stage.getPos().x,stage.getPos().y)
	stage.setPos(100000,-100000)
	print "The stage is now at (%d,%d)" % (stage.getPos().x,stage.getPos().y)
	stage.setPos(0,0)
	print "The stage is now at (%d,%d)" % (stage.getPos().x,stage.getPos().y)
	stage.setPos(0,0)
	print "The stage is now at (%d,%d)" % (stage.getPos().x,stage.getPos().y)
	stage.close()
	
