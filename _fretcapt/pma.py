#
# This is a stub file that you can use when testing outside of zlab, that is,
# when you are running python from command line.
#

if not 'zlabRunning' in globals():

	if not 'pmaSimulated' in globals():
		print "PMA functions are being simulated in python."
		pmaSimulated = 1

	def setMetaData(s):
		pass
	def getMetaData():
		pass
