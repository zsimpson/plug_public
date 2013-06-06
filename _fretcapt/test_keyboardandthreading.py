import sys					# for stdout printing
from time import sleep		# for sleep
import zlab

# hello there
i = 0
while i < 1000:
	if zlab.kbhit():
		c = zlab.getch()
		print "Got char={0}".format(c)
	print i
	sleep(1)
	i=i+1
	
