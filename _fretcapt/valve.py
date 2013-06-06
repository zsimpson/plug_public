import sys					# for stdout printing
from time import sleep		# for sleep
import ctypes
import inspect
import serialport
from pprint import pprint

valveDefaultPort = "COM5,9600,8,n,1,-,-"


#
#  VALVE
#==============================================================

class Valve(serialport.SerialPort):		

	def __init__(self,_portString=""):
		super(Valve,self).__init__()
		self.standardDelayAfterSet = 1
		self.liquidNameList = {}

		if not self.configureFromString(_portString):
			print "Failed to open valve {0}".format(self.portReason)
			
		if _portString != "":
			self.openPort()
			self.asyncReaderStart()
		
	def getModel(self):
		return "Vici EMH, EMT, ECMH, or ECMT"

	def getManualUrl(self):
		return "http://www.vici.com/support/tn/tn415.pdf"
		
	def setLiquids(self,liquidNameList):
		self.liquidNameList = liquidNameList
		
	def setPos(self,valvePos):	# You may pass either the NAME of the liquid, or the position number
		if isinstance(valvePos, str):
			liquidName = valvePos
			if not liquidName in self.liquidNameList:
				self.report.error( "Unknown liquid '"+liquidName+"'" )
			else:
				liquidIndex = self.liquidNameList[liquidName]
				self.doCommand("setPos({0}/{1})\n".format(liquidName,liquidIndex),"GO{0}".format(liquidIndex),self.standardDelayAfterSet)
		else:
			self.doCommand("setPos({0})\n".format(valvePos),"GO{0}".format(valvePos),self.standardDelayAfterSet)


def testValve(port=valveDefaultPort):
	print "Testing Valve."
	
	valve = Valve(port)

	valve.setLiquids( {'water':1, 'beer':2, 'wine':3, 'milk':4 } )
	
	valve.setPos(1)
	valve.setPos(2)
	valve.setPos(3)
	valve.setPos(4)
	valve.setPos('water')
	valve.setPos('milk')
	valve.setPos('wine')
	valve.setPos('beer')

	valve.close()

