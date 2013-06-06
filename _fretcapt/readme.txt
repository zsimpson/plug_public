To debug this more easily, run DreamPie and type in the command execfile('myfile.py')

DreamPie's default directory can be set as follows:
   1. Click the Start button
   2. Right-click the DreamPie entry in the list and pick Properties
   3. Under 'Shortcut' change the "Start In" entry to the directory you're working in

To find the list of all serial devices in the registry:
   1. Start / Run
   2. Regedit
   3. Visit the key: HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM

To determine whether one is open, or close it:
   1. Start Process Explorer (replaces your task manager)
   2. Click 'Find' and pick 'Find Handle or DLL'
   3. In the registry under SERIALCOMM look at the NAME column.
   4. Search for the part of the name thaqt follows \Device\ - for example VCP0 or Silabser0
   5. When you have found the name, click it
   6. To close the dangling serial port, close that window and look in the DLL pane (lower pane) of ProcExp
   7. Highlight the name (again) and hit delete

To close all open ports with script
   import serialport
   serialport.closeAll()

To test a device:
   1. simply import the appropriate .py file, and call test{Devicename}() where Devicename is that name of the device.
      Also, you can use the test_any.py file as a template.
      Here is a script snippet showing a test:
        import pump
        pump.testPump()

To halt a script:
   * You might be tempted to use sys.exit(), but that will exit zlab entirely.  the correct way is to use:
     raise Exception("My error message here")

      
Option Variables
================
You may create a 'local.cfg' file to control certain settings:

startupPlugin = "_fretview"		// The module to start in.  Either _fretview or _fretcapt
simulateSerialDevices = 0		// Set to 1 to simulate devices for debugging
pythonDebug = 0					// Set to 1 to get details about what Python is doing
Fretview_dir = c:\zlab			// Set this to the directory where you want your .pma files to be saved
Fretview_pythonDir = c:\zlab	// Set this to the directory where you keep your fretview python files
Fretcapt_pythonDir = c:\zlab	// Set this to the directory where you keep your fretCAPT python files
Fretcapt_recordMemInMB = ...	// Experts only!


ZLab Python Modules
====================

ALL MODES - ZLAB - (zlab.py)
-------------------------------------------------------
zlab.printConsole(m)			// prints to zlab's graphic console
zlab.message(m)					// puts a message 'm' in zlab's UI message queue. Experts only.
zlab.getch()					// waits for the user to type a key on the keyboard and returns it. Blocking.
zlab.kbhit()					// returns 1 when the user has hit a key on the keyboard.  Non-blocking. Unbuffered.

zlab.optionGet(k)				// returns the value of the key 'k' from zlab's option file(s)
zlab.configGet(k)				// returns the value of the key 'k' from the module's config file
zlab.configPut(k,v)				// In the current module's config file sets the key 'k' to the value 'v'

CAPTURE MODE - CAMERA - (camera.py)
-------------------------------------------------------
NOTE: All camera functions return 0 on success and non-zero on faiure.
camera.start()				// turn on the camera
camera.setMetaData(s)		// sets the metadata for the .pma file to a string 's' BEFORE you start recording ONLY
camera.getMetaData()		// returns the metadata for the current .pma file
camera.record(filename)		// begin recording to file 'filename'
camera.stop()				// stop recording
camera.setExposure(n.n)		// set the camera's exposure to n.n 
camera.setBin(n)			// set the camera bin to n.  Experts only.
testCamera()				// DOES NOT EXIST - run the file test_simplecamera.py instead

CAPTURE MODE - SHUTTER - (shutter.py)
-------------------------------------------------------
shutter.Shutter(channels)	// You may pass the channel identities when making a shutter, eg { 'blue':1, 'green':3, 'red':6 }
shutter.open(n)				// open shutter #n
shutter.close(n)			// close shutter #n
testShutter()				// tests the shutter system

CAPTURE MODE - PUMP - (pump.py)
-------------------------------------------------------
pump.waitWhileBusy()		// Pauses execution until the pump has completed it's current operation
pump.initialize()			// Resets the pump to a starting state
pump.aspirate(amount)		// Pulls liquid into the pump.  amount is 0 to 3000
pump.dispense(amount)		// Pushes liquid from the pump.  amount is 0 to 3000
pump.setPos(pos)			// Moves the plunger to an absolute position 0 to 3000
pump.setSpeed(speedHz)		// Tells the pump how fast to move - 50hz to 1000hz
pump.setValve(side,force)	// Sets the PUMP's valve.  side can be 'left' or 'right' and force can be 'half' or 'full'
pump.openInput()			// Opens the 'input' valve on the pump
pump.openOutput()			// Opens the 'output' valve on the pump
pump.getPos()				// Returns and integer plunger positon, 0 to 3000
pump.query()				// Asks the pump it's status
testPump()					// Tests the pump

CAPTURE MODE - VALVE - (valve.py)
-------------------------------------------------------
valve.setLiquids(nameList)	// Give names to a valve's liquids. For example { 'water': 1, 'beer': 2, 'wine': 3 }
valve.setPos(p)				// Sets the valve to position 'p'.  p can be a number or a liquid name
testValve()					// Tests the value

CAPTURE MODE - STAGE - (stage.py)
-------------------------------------------------------
stage.Vec2					// This is a data structure that contains an x,y representing the stage's location
stage.waitWhileBusy()		// Pauses execution until the stage completes it's current operation
stage.getPos()				// Returns a stage.Vec2 with x and y set for the stage's location
stage.setPos(x,y)			// Moves the stage to coordinate x by y
testStage()					// Tests the stage

VIEW MODE - PMA - (pma.py)
-------------------------------------------------------
pma.setMetaData(s)		// sets the metadata for the current .pma file to a string 's' BEFORE you start recording ONLY
pma.getMetaData()		// returns the metadata for the current .pma file


KNOWN ISSUES
==================================
* Switching from _fretcapt to _fretview sometimes crashes
* Occasionally an attempt to close a COM port (serial port) will appear to succeed, but the COM port will not open
  the next time.  To work around this unplug the USB that connects the COM port and then plug it back in.
* The NI camera driver does NOT correctly report when there is no camera. Instead it displays "Irq Conflict, Try PCI(Timer) Interface"

