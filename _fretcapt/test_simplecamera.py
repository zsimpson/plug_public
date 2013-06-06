import zlab
import camera
import time

camera.start()
camera.setMetaData("myvar1=22&yourvar=hello")
print "Recording"
camera.record("c:/zlab/camera_test.pma")
print "Sleeping"
time.sleep(4)
print "Stopping"
camera.stop()
print "Done"
