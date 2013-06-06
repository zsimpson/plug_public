#
# This is a stub file that you can use when testing outside of zlab, that is,
# when you are running python from command line.
#
# MANUAL:
# ftp://ftp.princetoninstruments.com/public/Manuals/Princeton%20Instruments/Archived%20Manuals/Sun%20Interface%20Library%20Programming%20Manual.pdf
#

if not 'zlabRunning' in globals():

	if not 'cameraSimulated' in globals():
		print "Camera functions are being simulated in python."
		cameraSimulated = 1

	def start():
		pass
	def setMetaData(s):
		pass
	def getMetaData():
		pass
	def record():
		pass
	def stop():
		pass
	def setExposure(n):
		pass
	def setBin(n):
		pass
			
def testCamera():
	print "Load and run test_simplecamera to test the camera."
