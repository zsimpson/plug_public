import sys					# for stdout printing
from time import sleep		# for sleep

import serialport
import shutter
import pump
import valve
import stage

reload(serialport)
reload(shutter)
reload(pump)
reload(valve)
reload(stage)

try:
	serialport.SerialPort.simulate = ( int(optionGet('simulateSerialDevices')) == 1 );
	if serialport.SerialPort.simulate:
		print "SIMULATING SERIAL DEVICES (see options.cfg or local.cfg and change simulateSerialDevices=1 to change this)"
except: pass
		
def testAll():
	shutter.testShutter()
	pump.testPump()
	valve.testValve()
	stage.testStage()

#shutter.testShutter()
#pump.testPump()
#valve.testValve()
stage.testStage()
#testAll()
