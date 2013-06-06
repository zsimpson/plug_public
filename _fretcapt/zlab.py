#
# This is a stub file that you can use when testing outside of zlab, that is,
# when you are running python from command line.
#

if not 'zlabRunning' in globals():

	if not 'zlabSimulated' in globals():
		print "ZLabfunctions are being simulated in python."
		zlabSimulated = 1

	def printConsole(message):
		print "emulated: {0}".format(message)
		pass
	def message(message):
		pass
	def getch():
		import msvcrt
		return msvcrt.getch()
	def kbhit():
		import msvcrt
		return msvcrt.kbhit()

	def optionGet(k):
		return ""
	def configGet(k):
		return ""
	def configPut(k,v):
		pass
