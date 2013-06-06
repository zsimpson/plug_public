// @ZBS {
//		*SDK_DEPENDS nidaq roper python andor pthread
//		*MODULE_DEPENDS pmafile.cpp colorpalette.cpp zuifilepicker.cpp zuitexteditor.cpp zuivar.cpp
//		*INTERFACE gui
//		*DIRSPEC ./*.py serial/*.py ./readme.txt
// }

// TODO: Temp controller
// TODO: EM Gain controller (with on / off )
	// Need to ask about SetEMGainMode

// TODO: ROI
// TODO: Snapshot

// TODO: Make the record button not call start and stop acquisition
// TODO: Stacked TIFF
// TODO: Exception on shutdown




//#define INCLUDE_SHUTTER_CONTROL

// OPERATING SYSTEM specific includes:
#include "windows.h"

#include "zpython.h"

#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
extern "C" {
	// roper
	#include "platform.h"
	#include "pigenfcn.h"
	#include "pigendef.h"
	#include "piimgfcn.h"
	#include "pimltfcn.h"
	#include "pievtfcn.h"

	// nidaq
	#ifdef INCLUDE_SHUTTER_CONTROL
	#include "nidaq.h"
	#endif
}
#include "atmcd32d.h"

// STDLIB includes:
#include "assert.h"
#include "stdio.h"
#include "time.h"
#include "math.h"
#include "limits.h"
#include "stdlib.h"
#include "float.h"
#include "sys/utime.h"
#include "sys/timeb.h"
#ifdef WIN32
#include "malloc.h"
#endif
// MODULE includes:
#include "colorpalette.h"
#include "pmafile.h"
// ZBSLIB includes:
#include "zchangemon.h"
#include "zviewpoint.h"
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zgltools.h"
#include "ztime.h"
#include "zmousemsg.h"
#include "ztmpstr.h"
#include "zglfont.h"
#include "zui.h"
#include "zidget.h"
#include "zconfig.h"
#include "mainutil.h"


extern int useDirtyRects;

ZPLUGIN_BEGIN( fretcapt );

ZVAR( float, Fretcapt_fps, 10.f );
ZVARB( int, Fretcapt_bin, 1, 1, 16 );
ZVAR( int, Fretcapt_first, 0 );
ZVAR( int, Fretcapt_last, 0 );
ZVAR( int, Fretcapt_lastCapt, 0 );
ZVAR( float, Fretcapt_exposure, 0.1 );
ZVAR( int, Fretcapt_setTemperature, -2 );
ZVAR( int, Fretcapt_emGain, 1 );

VidFile *vidFile = 0;

// National Instruments Interface
//--------------------------------------------------------------------------------------------------------------------

int shutterState[2] = {0,};

void shutterControl( int which, int state ) {
	// We found docs for the breakout board NI CB 68LP which seems to be a straight
	// forward breakout of the signal coming from the 6024E board.
	// Pin out at page 4-2 of 6024E User manual (in the NI/Ni dawq/docs)

#ifdef INCLUDE_SHUTTER_CONTROL
	assert( which >= 1 && which <= sizeof(shutterState)/sizeof(shutterState[0]) );
	shutterState[which-1] = state;

	// In experiemnt 20 Sep we found that 0==open and 1==closed
	// so I revered this logic. Let's see if it stays that way!
	state = !state;

	// SETUP the digital port into output mode
	DIG_Prt_Config( 1, 0, 0, 1 );
		// device number, port, mode, dir
		// mode: 0=no handshaking, 1=handshaking
		// dir: 0=port is input, 1=port is output, 2=bidirectional, 3=output for "wired-OR"

	DIG_Out_Line( 1, 0, which, state );
		// slot, port, linenum, state

#endif
}

float randF() {
	return (float)rand() / (1.f+(float)RAND_MAX);
}

int randI(double max) {
	static double r = 0.0;
	r = (double)rand() / (double)(RAND_MAX+1);
	return (int)( r * max );
}

unsigned short int randSI() {
	return rand() * 65536 / (RAND_MAX+1);
}


// camera controler
//--------------------------------------------------------------------------------------------------------------------

#define CAMERA_BUFFERS (4)
ZBits cameraCaptureBits[CAMERA_BUFFERS];
HANDLE cameraThreadHandle = 0;
int cameraCaptureSerial = 0;
double cameraLastFPS = 0.0;
int cameraStarted = 0;
int cameraBin = 1;

#define CAMERA_DIM (512)

int cameraROIX = 0;
int cameraROIY = 0;
int cameraROIW = CAMERA_DIM;
int cameraROIH = CAMERA_DIM;
	// Note that the ROI is in zero-based coords, not 1 based the way that
	// the camera actually reads takes them

int gCameraSimulate = 0;

void (*cameraFrameCallback)( ZBits &desc );


// Fake camera
//--------------------------------------------------------------------------------------------------------------------

int fakeLockFrame(void **frameBuf, unsigned long *size, unsigned int *error) {
	static unsigned short *fakeFrame = 0;
	static ZTimer timer;
	static const int PT_MAX = 30;
	static const int REGION_MAX = 3;
	static const float DRIFT_PER_SECOND = 1/(15);
	static FVec2 pt[PT_MAX];
	static FVec2 regionOffset[REGION_MAX];
	static float REGION_OFFSET_MAX = 40.0f;
	static float MAX_RADIUS = 10;
	static int regionXEdge[REGION_MAX+1];

	if( timer.isRunning() && !timer.isDone() )
		return 0;

	timer.start(1/Fretcapt_fps);

	unsigned int fakeBytes = cameraROIH*cameraROIW*2 / cameraBin;

	if( !fakeFrame ) {
		fakeFrame = (unsigned short *)new char[fakeBytes];
		srand( 0 );
		for( int r=0 ; r<REGION_MAX+1 ; ++r ) {
			regionXEdge[r] = r*(cameraROIW/REGION_MAX);
		}
		for( int r=0 ; r<REGION_MAX ; ++r ) {
			float xOffset;
			float yOffset;
			do {
				xOffset = (randF()-0.5f)*2.f*REGION_OFFSET_MAX;
				yOffset = (randF()-0.5f)*2.f*REGION_OFFSET_MAX;
				regionOffset[r] = FVec2( (r*(cameraROIW/REGION_MAX))+xOffset, yOffset );
			} while( fabs(xOffset) < REGION_OFFSET_MAX/2 || fabs(yOffset) < REGION_OFFSET_MAX );
		}

		for( int i=0 ; i<PT_MAX ; ++i ) {
			float margin = MAX_RADIUS+10.0f;
			float x = margin+(randF()*((cameraROIW/REGION_MAX)-2*margin)) ;
			float y = margin+(randF()*(cameraROIH-2*margin));
			pt[i] = FVec2( x, y );
		}
	}

	memset(fakeFrame,0,fakeBytes);

	for( int i=0 ; i<10000 ; ++i ) {
		int index = randI(fakeBytes/2);
		fakeFrame[index] = abs(randI(100)+randI(100)+randI(100)-150);
	}

	for( int i=0 ; i<PT_MAX ; ++i ) {
		pt[i].x += (randF()-0.5f) * DRIFT_PER_SECOND;
		pt[i].y += (randF()-0.5f) * DRIFT_PER_SECOND;
	}

	for( int i=0 ; i<PT_MAX ; ++i ) {
		for( int count=0 ; count < 5500 ; ++count ) {
			float xOffset = (float)( ((randF()+randF()+randF())-1.5) / 1.5 * MAX_RADIUS );
			float yOffset = (float)( ((randF()+randF()+randF())-1.5) / 1.5 * MAX_RADIUS );
			for( int r=0 ; r<REGION_MAX ; ++r ) {
				int x = (int)(regionOffset[r].x+pt[i].x+xOffset);
				int y = (int)(regionOffset[r].y+pt[i].y+yOffset);
				if( x < regionXEdge[r] || x > regionXEdge[r+1] )
					continue;
				unsigned int index = ( y*(cameraROIW/cameraBin) + x );
				if( index > 0 && index < fakeBytes )
					fakeFrame[index] += 1;
			}
		}
	}

	*size = fakeBytes;	// KD: actually an unused variable, as far as I can tell
	*error = 0;
	*frameBuf = (void *)fakeFrame;

	return 1;
}



// Andor camera
//--------------------------------------------------------------------------------------------------------------------

int runThread = 1;
int previewMode = 1;
int temperature = 0;
int temperatureStatus = 0;
int shutdownMode = 0;

DWORD WINAPI cameraThread( LPVOID hEventR3 ) {
	unsigned long size = 0;
	unsigned int err = 0;
	long lastFrameCaptured = -1;

	while( runThread ) {
		// The thread doesn't kill itself.  It is terminated by the controlling thread

		if( ! cameraStarted ) {
			Sleep( 100 );
			continue;
		}

		if( previewMode ) {
			// The camera requires that you start and stop acquire every frmae

			err = AbortAcquisition();
			assert( err == DRV_SUCCESS );

			err = SetTemperature( Fretcapt_setTemperature );
			assert( err == DRV_SUCCESS );

			temperatureStatus = GetTemperature( &temperature );

			err = SetEMCCDGain( Fretcapt_emGain );
			assert( err == DRV_SUCCESS );

			err = SetShutter( 1, 1, 0, 0 );
			assert( err == DRV_SUCCESS );

			err = SetImage( 1, 1, 1, CAMERA_DIM, 1, CAMERA_DIM );
			assert( err == DRV_SUCCESS );

			err = SetExposureTime( Fretcapt_exposure );
			assert( err == DRV_SUCCESS );

			err = StartAcquisition();
			assert( err == DRV_SUCCESS );

			lastFrameCaptured = -1;
		}
		
		if( shutdownMode ) {
			previewMode = 0;
			CoolerOFF();
			temperatureStatus = GetTemperature( &temperature );
			if( temperature >= -1 ) {
				ShutDown();
				shutdownMode = 0;
				cameraStarted = 0;
				zMsgQueue( "type=ZUISet key=disabled val=0 toZUI=cameraStopButton" );
				zMsgQueue( "type=ZUISet key=hidden val=1 toZUI=cameraStopButton" );
				zMsgQueue( "type=ZUISet key=hidden val=0 toZUI=cameraStartButton" );
				zMsgQueue( "type=ZUILayout" );
			}
		}
		
		long first, last;
		GetNumberNewImages( &first, &last );

		if( previewMode ) {
			while( 1 ) {
				GetNumberNewImages( &first, &last );
				int newFrameAvail = last != 0;
				if( newFrameAvail ) {
					break;
				}
			}
		}

		int newFrameAvail = last != lastFrameCaptured;
		if( newFrameAvail ) {
			lastFrameCaptured = last;
			cameraCaptureSerial++;
			int which = cameraCaptureSerial % CAMERA_BUFFERS;

			static ZTime cameraLastCaptureTime = 0.0;
			ZTime now = zTimeNow();
			cameraLastFPS = 1.0 / (now - cameraLastCaptureTime);
			cameraLastCaptureTime = now;

			// COPY data from DMA buffer into the cameraCaptureBits bitmap
			//int err = GetMostRecentImage( (long *)cameraCaptureBits[which].bits, cameraROIW * cameraROIH );
			long validFirst=-1, validLast=-1;
			int err = GetImages( last, last, (long *)cameraCaptureBits[which].bits, cameraROIW * cameraROIH, &validFirst, &validLast );

			if( cameraFrameCallback ) {
				(*cameraFrameCallback)( cameraCaptureBits[which] );
			}
		}
	}

	return 0;
}

void cameraBegin() {
	int err;

	// ALLOCATE the bitmap
	for( int i=0; i<CAMERA_BUFFERS; i++ ) {
		cameraCaptureBits[i].createCommon( CAMERA_DIM, CAMERA_DIM, ZBITS_COMMON_LUM_32S );
	}

	char aBuffer[256] = {0,};
    GetCurrentDirectory(256,aBuffer);

	// INITIALIZE driver in current directory
	err = Initialize( aBuffer ); 
	assert( err == DRV_SUCCESS );

	AndorCapabilities caps;
	caps.ulSize = sizeof(AndorCapabilities);
	err = GetCapabilities(&caps);
	assert( err == DRV_SUCCESS );
	assert( caps.ulTriggerModes & AC_TRIGGERMODE_CONTINUOUS );

	int andorW, andorH;
	err = GetDetector( &andorW, &andorH );
	assert( andorW == CAMERA_DIM );
	assert( andorH == CAMERA_DIM );

	err = SetAcquisitionMode( 5 ); // 5 = continuous
	assert( err == DRV_SUCCESS );

	err = SetReadMode( 4 ); // 4 = image
	assert( err == DRV_SUCCESS );

	err = PrepareAcquisition();
	assert( err == DRV_SUCCESS );

	err = SetShutter( 1, 1, 0, 0 );
	assert( err == DRV_SUCCESS );

	err = SetTriggerMode( 0 );	// 0 == internal
	assert( err == DRV_SUCCESS );

	err = SetImage( 1, 1, 1, andorW, 1, andorH );
	assert( err == DRV_SUCCESS );

	err = SetExposureTime( Fretcapt_exposure );
	assert( err == DRV_SUCCESS );

	err = SetKineticCycleTime( 0 );
	assert( err == DRV_SUCCESS );

	err = CoolerON();
	assert( err == DRV_SUCCESS );

	err = StartAcquisition();
	assert( err == DRV_SUCCESS );

	cameraStarted = 1;

	Sleep( 200 );
}


// Recording
//--------------------------------------------------------------------------------------------------------------------

int recordFrameCount = 0;
ZTime recordFrameTimeStart = 0;
char recordFilename[256] = {0,};
int snapshot = 0;

void recordWriteFrameCallback( ZBits &captureDesc ) {
	if( snapshot ) {
		// @TODO: Dump out snapshot
		snapshot = 0;
	}

	if( !vidFile )
		return;

	if( vidFile->isRecording() ) {
		assert( captureDesc.channelDepthInBits == 32 );
		assert( recordFrameCount < 0xFFFF );

		// CONVERT from 32 bit to 16 bit
		static ZBits *dstBits = 0;
		if( ! dstBits ) {
			dstBits = new ZBits();
			dstBits->createCommon( captureDesc.w(), captureDesc.h(), ZBITS_COMMON_LUM_16 );
		}
		for( int y=0; y<captureDesc.h(); y++ ) {
			unsigned int *src = captureDesc.lineU4(y);
			unsigned short *dst = dstBits->lineU2(y);
			for( int x=0; x<captureDesc.w(); x++ ) {
				*dst = *src > 0xFFFF ? (unsigned short)0xFFFF : (unsigned short)*src;
				dst++;
				src++;
			}
		}

		unsigned short *src = &dstBits->lineUshort(cameraROIY)[cameraROIX];
		unsigned int tenthsOfMils = unsigned int( (zTimeNow()-recordFrameTimeStart) * 10000.0 );
		int laserState = shutterState[0] | (shutterState[1]<<1);
		vidFile->writeFrame( tenthsOfMils, src, dstBits->p(), &laserState, sizeof(laserState) );
		recordFrameCount++;
	}
}

void recordStart( char *filename=0 ) {
	if( !filename ) {
		char time[64], date[64];
		_tzset();
		_strtime( time );
		_strdate( date );
		char *c;
		for( c=time; *c; c++ ) {
			if( *c == ':' ) *c = '-';
		}
		for( c=date; *c; c++ ) {
			if( *c == '/' ) *c = '-';
		}

		char dir[256] = {0,};
		strcpy( dir, options.getS("Fretcapt_dir","") );
		if( dir[strlen(dir)-1] == '/' || dir[strlen(dir)-1] == '\\' ) {
			dir[strlen(dir)-1] = 0;
		}

		filename = ZTmpStr("%s/%s_%s.pma",dir,date,time);
	}
	strcpy( recordFilename, filename );

	recordFrameCount = 0;
	recordFrameTimeStart = zTimeNow();
	vidFile->w = cameraROIW;
	vidFile->h = cameraROIH;
	vidFile->setInfoI( "bin", cameraBin );
	vidFile->setInfoI( "x", cameraROIX );
	vidFile->setInfoI( "y", cameraROIY );

	vidFile->openWrite( recordFilename );

	cameraFrameCallback = recordWriteFrameCallback;
	
	previewMode = 0;

	zMsgQueue( "type=ZUISet key=text val='Recording' toZUI=fretcaptCameraStatusText" );
}

ZMSG_HANDLER( Fretcapt_CameraRecord ) {
	char *fileName = 0;
	if( zmsgHas(filename) )
		fileName = zmsgS(filename);
	recordStart(fileName);
}

ZMSG_HANDLER( Fretcapt_CameraMetaDataSet ) {
	if( zmsgHas(metadata) ) {
		vidFile->setMetaData( zmsgS(metadata) );
	}
	else {
		trace("ERROR: message Fretcapt_CameraMetaDataSet needs field 'metadata=...'");
	}
}

ZMSG_HANDLER( Fretcapt_Snapshot ) {
	snapshot = 1;
}


// Rendering
//--------------------------------------------------------------------------------------------------------------------

ZidgetImage imageRaw;
ZidgetHistogram histogramRaw;

ZidgetRectLBWH rgnSelect;
ZidgetPos2 rgnSelectTop;
ZidgetPos2 rgnSelectBot;

void updateStatusText() {
	char status[1024];

	unsigned int lo = (unsigned int)vidFile->getInfoI( "allocatedMemoryLo" );
	unsigned int hi = (unsigned int)vidFile->getInfoI( "allocatedMemoryHi" );
	unsigned int memory = (hi<<16) | (lo);
	unsigned int used = recordFrameCount*cameraROIW*cameraROIH*2;

	float secsRemain = 0.f;
	float secsUsed = 0.f;
	if( Fretcapt_exposure > 0.f ) {
		secsRemain = (float)(memory-used) / ( float(cameraROIW*cameraROIH*2)*(1.f/Fretcapt_exposure) );
		secsUsed = (float)(used) / ( float(cameraROIW*cameraROIH*2)*(1.f/Fretcapt_exposure) );
	}

	sprintf( status, "Camera: %s\nShutdown mode: %s\nPreview mode: %s\nMegs alloc: %d\nSecs used: %d\nSecs remain: %d\nRecording: %s\nCurrent FPS: %4.1f\nTemp: %d\ntemp stable: %d\n", 
		cameraStarted ? "started" : "off",
		shutdownMode ? "yes" : "no",
		previewMode ? "yes" : "no",
		memory / 1000000,
		(int)secsUsed,
		(int)secsRemain,
		(vidFile && vidFile->isRecording()) ? "on" : "off",
		cameraLastFPS,
		temperature,
//		tempStatus == DRV_TEMP_STABILIZED ? "yes" : "no"
		temperatureStatus
	);

	ZUI *z = ZUI::zuiFindByName( "fretcaptStatusText" );
	if( z ) {
		z->putS( "text", status );
	}
}

void viewAutoscale() {
	int mode = 0;
	float maxCnt = 0.f;
	int maxBin = 0;

	int count;
	float *hist;

	count = histogramRaw.histogramCount;
	hist = histogramRaw.histogram;

	float sum = 0.f;
	for( int i=0; i<count; i++ ) {
		if( hist[i] > maxCnt ) {
			maxCnt = hist[i];
			mode = i;
		}
		sum += hist[i];
	}

	// FIND the point where some percent of the mass is on the left side
	float s = 0.f;
	int r = 0;
	for( r=0; r<count && s < 0.9999f * sum; r++ ) {
		s += hist[r];
	}

	histogramRaw.histScale0 = (float)mode;
	histogramRaw.histScale1 = (float)r;
}


ZMSG_HANDLER( Fretcapt_Autoscale ) {
	viewAutoscale();
}

void render() {
	int x, y;

	pythonCheckForUpdatedFile();

	glClearColor( 0.7f, 0.7f, 0.7f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT );

	float viewport[4];
	glGetFloatv( GL_VIEWPORT, viewport );

	zglPixelMatrixInvertedFirstQuadrant();
	glTranslatef( 350.f, 100.f, 0.f );
	zviewpointSetupView();

	// TRACK the selection area
	rgnSelectTop.pos.x = CAMERA_DIM / 2;
	rgnSelectBot.pos.x = CAMERA_DIM / 2;
	rgnSelectBot.pos.y = max( 0.f, min( CAMERA_DIM, rgnSelectBot.pos.y ) );
	rgnSelectTop.pos.y = max( 0.f, min( CAMERA_DIM, rgnSelectTop.pos.y ) );
	rgnSelectBot.pos.y = max( rgnSelectTop.pos.y, rgnSelectBot.pos.y );
	rgnSelectTop.pos.y = float( ((int)rgnSelectTop.pos.y / 4) * 4 );
	rgnSelectBot.pos.y = float( ((int)rgnSelectBot.pos.y / 4) * 4 );
		// Limit the position to multiples of 4
	rgnSelect.bl.x = 0.f;
	rgnSelect.bl.y = rgnSelectTop.pos.y;
	rgnSelect.w = CAMERA_DIM;
	rgnSelect.h = rgnSelectBot.pos.y - rgnSelectTop.pos.y;

	// UPDATE the bits in the view
	static int lastSerial = 0;
	if( lastSerial != cameraCaptureSerial ) {
		lastSerial = cameraCaptureSerial;

		// This is bad because I'm always copying the whole are instead of just the ROI
		// REDO this

		ZBits *srcBits = &cameraCaptureBits[ cameraCaptureSerial % CAMERA_BUFFERS ];
		if( srcBits->bits ) {
			for( y=0; y<cameraROIH; y++ ) {
				unsigned int *src = &srcBits->lineU4( y+cameraROIY )[cameraROIX];
				unsigned short *dst = &imageRaw.srcBits.lineU2( y+cameraROIY )[cameraROIX];
				for( x=0; x<cameraROIW; x++ ) {
					*dst++ = *(unsigned short *)src++;
				}
			}
			imageRaw.dirtyBits();
		}

		// HISTORGRAM
		float histogramData[4096] = { 0, };
		int w = cameraROIW / cameraBin;
		int h = cameraROIH / cameraBin;

		for( y=0; y<h; y+=4 ) {
			unsigned int *src = &srcBits->lineU4( y+cameraROIY )[cameraROIX];
			for( x=0; x<w; x+=4, src += 4 ) {
				int histBucket = max( 0, min( 4096-1, (int)*src ) );
				histogramData[histBucket]++;
			}
		}
		histogramRaw.setHist( histogramData, 4096 );
	}

	updateStatusText();

	histogramRaw.histScale1 = max( histogramRaw.histScale0+1, histogramRaw.histScale1 );
	imageRaw.colorScaleMin = histogramRaw.histScale0;
	imageRaw.colorScaleMax = histogramRaw.histScale1;

	// RENDER zidgets
	glPushMatrix();
	Zidget::zidgetRenderAll();
	glPopMatrix();
}

// Camera setup
//--------------------------------------------------------------------------------------------------------------------

void cameraUpdateSettings() {
/*
	int x = 0;
	int y = (int)rgnSelectTop.pos.y;
	int w = CAMERA_DIM;
	int h = (int)(rgnSelectBot.pos.y - rgnSelectTop.pos.y);
	cameraSetup( cameraExposure, x, y, w, h );
	imageRaw.srcBits.fill( 0 );
*/
}

void cameraStart() {
	previewMode = 1;
	shutdownMode = 0;
	zMsgQueue( "type=ZUISet key=hidden val=1 toZUI=cameraStartButton" );
	zMsgQueue( "type=ZUISet key=hidden val=0 toZUI=cameraStopButton" );
	zMsgQueue( "type=ZUILayout" );
	cameraBegin();
	cameraUpdateSettings();
}

void cameraStop() {
	if( vidFile && vidFile->isRecording() ) {
		vidFile->close();
		recordFrameCount = 0;
		trace( "File written to %s", recordFilename );
		zMsgQueue( "type=ZUISet key=text val='File written to %s' toZUI=fretcaptCameraStatusText", escapeQuotes(recordFilename) );
	}
	previewMode = 1;
}

ZMSG_HANDLER( Fretcapt_CameraSetExposure ) {
	float exposure = 1.f / Fretcapt_fps;
	if( zmsgHas(exposure) ) {
		exposure = zmsgF(exposure);
	}

	Fretcapt_exposure = exposure;
}

ZMSG_HANDLER( Fretcapt_CameraSetBin ) {
	int bin = (int)Fretcapt_bin;
	if( zmsgHas(bin) ) {
		bin = zmsgI(bin);
	}

	if( cameraBin != bin ) {
		cameraBin = bin;
		cameraUpdateSettings();
	}
}

ZMSG_HANDLER( Fretcapt_CameraShutdown ) {
	Fretcapt_setTemperature = -1;
	zMsgQueue( "type=ZUISet key=disabled val=1 toZUI=cameraStopButton" );
	shutdownMode = 1;
}

ZMSG_HANDLER( Fretcapt_CameraStart ) {
	cameraStart();
}

ZMSG_HANDLER( Fretcapt_CameraStop ) {
	cameraStop();
}

ZMSG_HANDLER( Fretcapt_SetROI ) {
	rgnSelect.attach( &imageRaw );
	rgnSelectTop.attach( &imageRaw );
	rgnSelectBot.attach( &imageRaw );
}

ZMSG_HANDLER( Fretcapt_ActivateROI ) {
	cameraUpdateSettings();
	rgnSelect.detach();
	rgnSelectTop.detach();
	rgnSelectBot.detach();
}

static PyObject *CameraModule_Start(PyObject *self, PyObject *args) {
	if( cameraStarted )
		return Py_BuildValue("i",1);

	zMsgQueue( "type=Fretcapt_CameraStart" );
	return Py_BuildValue("i",0);
}
PythonRegister register_CameraStart( "camera", "start", &CameraModule_Start);

static PyObject *CameraModule_Record(PyObject *self, PyObject *args) {
	if( !vidFile )
		return Py_BuildValue("i",1);
	if( vidFile->isRecording() )
		return Py_BuildValue("i",2);

	char *fileName = 0;
	int ok = PyArg_ParseTuple(args,"s",&fileName);
	if( ok == 0 ) {
		traceNoFormat("Error: must pass a file name to camera record");
		return Py_BuildValue("i",3);
	}

	zMsgQueue( "type=Fretcapt_CameraRecord filename=%s", fileName );
	return Py_BuildValue("i",0);
}
PythonRegister register_CameraRecord( "camera", "record", &CameraModule_Record);

static PyObject *CameraModule_Stop(PyObject *self, PyObject *args) {
	if( !vidFile )
		return Py_BuildValue("i",1);
	if( !vidFile->isRecording() )
		return Py_BuildValue("i",2);
	zMsgQueue( "type=Fretcapt_CameraStop" );
	return Py_BuildValue("i",0);
}
PythonRegister register_CameraStop( "camera", "stop", &CameraModule_Stop);

static PyObject *CameraModule_SetExposure(PyObject *self, PyObject *args) {
	float exposure = 0;
	PyArg_ParseTuple(args,"f",&exposure);
	zMsgQueue( ZTmpStr("type=Fretcapt_CameraSetExposure exposure=%5.3f",exposure).s );
	return Py_BuildValue("i",0);
}
PythonRegister register_CameraSetExposure( "camera", "setExposure", &CameraModule_SetExposure);

static PyObject *CameraModule_SetBin(PyObject *self, PyObject *args) {
	int bin = 0;
	PyArg_ParseTuple(args,"i",&bin);
	zMsgQueue( ZTmpStr("type=Fretcapt_CameraSetBin bin=%d",bin).s );
	return Py_BuildValue("i",0);
}
PythonRegister register_CameraSetBin( "camera", "setBin", &CameraModule_SetBin);

static PyObject *CameraModule_SetMetaData(PyObject *self, PyObject *args) {
	char *metaData = 0;
	PyArg_ParseTuple(args,"s",&metaData);
	if( !vidFile )
		return Py_BuildValue("i",1);
	if( vidFile->isRecording() )
		return Py_BuildValue("i",2);

	vidFile->setMetaData(metaData);
	return Py_BuildValue("i",0);
}
PythonRegister register_CameraSetMetaData( "camera", "setMetaData", &CameraModule_SetMetaData);

static PyObject *CameraModule_GetMetaData(PyObject *self, PyObject *args) {
	if( !vidFile )
		return Py_BuildValue("i",1);
	return Py_BuildValue("s",vidFile->getMetaData());
}
PythonRegister register_CameraGetMetaData( "camera", "getMetaData", &CameraModule_GetMetaData);


/*

Shutter handling is done through Python now. No point in doing it in C++ if not necessary.

static PyObject *ZLabModule_ShutterOpen(PyObject *self, PyObject *args) {
	int shutterIndex = 0;
	PyArg_ParseTuple(args,"i",&shutterIndex);
	if( shutterIndex >=1 && shutterIndex <= 5 ) {
		shutterControl( shutterIndex, 1 );
		return Py_BuildValue("i",1);
	}
	return Py_BuildValue("i",0);
}
PythonRegister register_ShutterOpen( "shutter", "open", &ZLabModule_ShutterOpen);

static PyObject *ZLabModule_ShutterClose(PyObject *self, PyObject *args) {
	int shutterIndex = 0;
	PyArg_ParseTuple(args,"i",&shutterIndex);
	if( shutterIndex >=1 && shutterIndex <= 5 ) {
		shutterControl( shutterIndex, 0 );
		return Py_BuildValue("i",1);
	}
	return Py_BuildValue("i",0);
}
PythonRegister register_ShutterClose( "shutter", "close", &ZLabModule_ShutterClose);
*/



// Startup
//--------------------------------------------------------------------------------------------------------------------

ZHashTable fretcaptConfig;

void saveConfig() {
	fretcaptConfig.putF( "zviewpointTrans.x", zviewpointTrans.x );
	fretcaptConfig.putF( "zviewpointTrans.y", zviewpointTrans.y );
	fretcaptConfig.putF( "zviewpointScale", zviewpointScale );

	fretcaptConfig.putF( "rgnSelectTop.pos.y", rgnSelectTop.pos.y );
	fretcaptConfig.putF( "rgnSelectBot.pos.y", rgnSelectBot.pos.y );

	fretcaptConfig.putF( "histogramRaw.histScale0", histogramRaw.histScale0 );
	fretcaptConfig.putF( "histogramRaw.histScale1", histogramRaw.histScale1 );

	fretcaptConfig.putF( "cameraExposure", Fretcapt_exposure );
	fretcaptConfig.putI( "cameraBin", cameraBin );

	zConfigSaveFile( pluginPath("fretcapt.cfg"), fretcaptConfig );
}

void loadConfig(ZHashTable &fretcaptConfig) {
	zConfigLoadFile( pluginPath("fretcapt.cfg"), fretcaptConfig );

	zviewpointTrans.x = fretcaptConfig.getF( "zviewpointTrans.x", 0.f );
	zviewpointTrans.y = fretcaptConfig.getF( "zviewpointTrans.y", 0.f );
	zviewpointScale = fretcaptConfig.getF( "zviewpointScale", 1.f );

	rgnSelectTop.pos.y = fretcaptConfig.getF( "rgnSelectTop.pos.y" );
	rgnSelectBot.pos.y = fretcaptConfig.getF( "rgnSelectBot.pos.y" );

	histogramRaw.histScale0 = fretcaptConfig.getF( "histogramRaw.histScale0" );
	histogramRaw.histScale1 = fretcaptConfig.getF( "histogramRaw.histScale1" );

	Fretcapt_exposure = fretcaptConfig.getF( "Fretcapt_exposure" );
	Fretcapt_exposure = max( 0.1f, Fretcapt_exposure );
	cameraBin = fretcaptConfig.getI( "cameraBin", 1 );
	Fretcapt_bin = cameraBin;
}

void stdoutHook( char *text ) {
	traceNoFormat(text);
}

void startup() {
	DWORD threadID = 0;
	cameraThreadHandle = CreateThread( 0, 0x1000, cameraThread, 0, 0, &threadID );
	SetThreadPriority( cameraThreadHandle, THREAD_PRIORITY_ABOVE_NORMAL );
	assert( cameraThreadHandle );

	pythonInterpreter.startup(stdoutHook);

	loadConfig(fretcaptConfig);

	pythonConsoleStart(&fretcaptConfig,options.getS("Fretcapt_pythonDir",""));
	gCameraSimulate = options.getI("simulateSerialDevices",0);

	colorPaletteInit();

	// SETUP control panel
	ZUI::zuiExecuteFile( pluginPath("_fretcapt.zui") );

	imageRaw.attach();
	imageRaw.srcBits.createCommon( CAMERA_DIM, CAMERA_DIM, ZBITS_COMMON_LUM_16 );
	imageRaw.srcBits.fill( 0 );
	imageRaw.colorScaleMin = 0.f;
	imageRaw.colorScaleMax = 100.f;
	imageRaw.pos.x = 0.f;
	imageRaw.pos.y = 0.f;

	histogramRaw.attach( &imageRaw );
	histogramRaw.pos.x = 0.f;
	histogramRaw.pos.y = -30.f;
	histogramRaw.xScale = 0.3f;
	histogramRaw.yScale = 4.00f;
	histogramRaw.logScale = 1;
	histogramRaw.histScale0 = max( 0.f, histogramRaw.histScale0 );
	histogramRaw.histScale1 = max( histogramRaw.histScale0+1.f, histogramRaw.histScale1 );

	rgnSelect.color = FVec4( 0.f, 0.f, 1.f, 0.5f );
	rgnSelect.handles[ZidgetLine2_bl].disabled = 1;
	rgnSelect.handles[ZidgetLine2_wh].disabled = 1;
	rgnSelectTop.pos.y = 0;
	rgnSelectBot.pos.y = CAMERA_DIM;

	// PREPARE the vid file for writing
	vidFile = new PMAVidFile;

	unsigned int memory = options.getI( "Fretcapt_recordMemInMB" ) * 1024 * 1024;
	if( !memory ) {
		memory = 500 * 1024 * 1024;
	}
	vidFile->preallocWrite( memory, 30000 );

	zMsgQueue( "type=ZUISet key=text val='%s' toZUI=fretcaptDeviceInfo", gCameraSimulate ? "SIMULATED" : "Live" );

}

void shutdown() {
	// Stop recording, if recording
	if( vidFile && vidFile->isRecording() ) {
		vidFile->close();
		recordFrameCount = 0;
	}

	if( vidFile ) {
		delete vidFile;
		vidFile = 0;
	}

	saveConfig();

	runThread = 0;
	Sleep( 300 );

	pythonInterpreter.shutdown();

	imageRaw.detach();
	histogramRaw.detach();
	rgnSelect.detach();
	rgnSelectTop.detach();
	rgnSelectBot.detach();

	zMsgQueue( "type=ZUIDie toZUI=styleFretcaptSectionHeader" );
	zMsgQueue( "type=ZUIDie toZUI=styleFretcaptTwiddler" );
	zMsgQueue( "type=ZUIDie toZUI=fretcaptZUI" );

	ZUI *z = ZUI::zuiFindByName( "pythonPanel" );
	if( z )
		z->die();
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && pythonInterpreter.isBusy() ) {
		int keystroke = *zmsgS(which);
		pythonSetKeystroke(keystroke);
	}
	else if( zmsgIs(type,Key) && zmsgIs(which,2) ) {
		shutterControl( 1, !shutterState[0] );
	}
	else if( zmsgIs(type,Key) && zmsgIs(which,1) ) {
		shutterControl( 2, !shutterState[1] );
	}
	else if( zmsgIs(type,Key) && zmsgIs(which,r) ) {
		if( vidFile->isRecording() ) {
			zMsgQueue( "type=Fretcapt_CameraStop" );
		}
		else {
			zMsgQueue( "type=Fretcapt_CameraRecord" );
		}
	}

	Zidget::zidgetHandleMsg( msg );
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;




// Pentamax camera
//--------------------------------------------------------------------------------------------------------------------

/*

DWORD WINAPI cameraThread( LPVOID hEventR3 ) {
	unsigned long size = 0;
	unsigned int error = 0;

	while( 1 ) {
		// The thread doesn't kill itself.  It is terminated by the controlling thread
		long waitResult = gCameraSimulate ? WAIT_OBJECT_0 : WaitForSingleObject((HANDLE)hEventR3, 100);
		if( waitResult == WAIT_OBJECT_0 ) {
			void *frameBuf = 0;

			int locked = gCameraSimulate ? fakeLockFrame(&frameBuf, &size, &error) : PICM_LockCurrentFrame( &frameBuf, &size, &error );

			if( locked ){
				cameraCaptureSerial++;
				int which = cameraCaptureSerial % CAMERA_BUFFERS;
				if( frameBuf ) {
					static ZTime pentamaxLastCaptureTime = 0.0;
					ZTime now = zTimeNow();
					cameraLastFPS = 1.0 / (now - pentamaxLastCaptureTime);
					pentamaxLastCaptureTime = now;

					// COPY data from DMA buffer into the cameraCaptureBits bitmap
					for( int y=0; y<cameraROIH/cameraBin; y++ ){
						unsigned short *src = &((unsigned short *)frameBuf)[y*cameraROIW/cameraBin];
						unsigned short *dst = &cameraCaptureBits[which].lineUshort( y+cameraROIY )[cameraROIX];
						memcpy( dst, src, cameraROIW*2/cameraBin );
					}

					if( cameraFrameCallback ) {
						(*cameraFrameCallback)( cameraCaptureBits[which] );
					}
				}

				assert( !(error & DATAOVERRUN) );
				assert( !(error & VIOLATION) );

				if( !gCameraSimulate ) {
					PICM_UnlockCurrentFrame();
				}
			}
		}
	}

	return 0;
}

void cameraSetup( float exposure, int x, int y, int w, int h ) {
	// This takes x and y in zero-based coords, not one-based the 
	// way that pentamax actually wants them for some odd reason
	if( ! cameraStarted ) {
		return;
	}

	cameraExposure = exposure;

	cameraROIX = max( 0, min( cameraCaptureBits[0].w(), x ) );
	cameraROIY = max( 0, min( cameraCaptureBits[0].h(), y ) );
	cameraROIW = max( 0, min( w, cameraCaptureBits[0].w()-x ) );
	cameraROIH = max( 0, min( h, cameraCaptureBits[0].h()-y ) );

	for( int i=0; i<CAMERA_BUFFERS; i++ ) {
		memset( cameraCaptureBits[i].bits, 0, cameraCaptureBits[i].lenInBytes() );
	}

	if( !gCameraSimulate ) {
		unsigned int error_code;

		PICM_Stop_controller();

		// SET exposure time to 0.1 seconds
		int ok = PICM_SetExposure( exposure, &error_code );
		assert( ok );

		// SET region of interest to be the whole screen
		ok = PICM_SetROI( cameraROIX+1, cameraROIY+1, cameraROIX+cameraROIW, cameraROIY+cameraROIH, Fretcapt_bin, Fretcapt_bin, &error_code );
			// Note that the camera wants the offset in 1 based coords
		assert( ok );

		// SET for synchronous mode acquisition
		ok = PICM_Set_AutoStop(0);
		assert( ok );

		// SET to fast ADC
		ok = PICM_Set_Fast_ADC();
		assert( ok );

		// SET for focus mode, this means that we might miss frames,
		// but won't overwrite DMA buffer (hopefully).
		// PICM_Set_EasyDLL_DC(EASYDLL_NFRAME_MODE);
		ok = PICM_Set_EasyDLL_DC(EASYDLL_FOCUS_MODE);
		assert( ok );

		// WARNING: This returns OK with no error code even if there is NO CAMERA attached to the
		// computer.
		ok = PICM_Initialize_System( NULL, &error_code );
		assert( ok );

		PICM_Start_controller();
	}
}

void cameraBegin() {
	unsigned int error_code = 0;

	if( cameraStarted )
		return;

	// ALLOCATE the bitmap
	for( int i=0; i<CAMERA_BUFFERS; i++ ) {
		cameraCaptureBits[i].createCommon( CAMERA_DIM, CAMERA_DIM, ZBITS_COMMON_LUM_16 );
	}

	// CREATE controller
	int ok = gCameraSimulate || PICM_CreateController( DC131, EEV_1024x512_FT, ROM_FRAME_TRANSFER, APP_NORMAL_IMAGING, &error_code );
	assert( ok );

	ok = gCameraSimulate || PICM_SetInterfaceCard( PCI_COMPLEX_PC_Interface, 0x00, CHANNEL_9, &error_code );
	assert( ok );

	// SET controller type to version 5
	ok = gCameraSimulate || PICM_Set_controller_version(5);
	assert( ok );

	cameraStarted = 1;

	cameraSetup( cameraExposure, 0, 0, CAMERA_DIM, CAMERA_DIM );

	// SET to notify thread after only 1 image
	HANDLE cameraEventInterrupt = gCameraSimulate ? (HANDLE)1 : PICM_SetUserEvent( 0, 0, 0, 0, 1 ); 
	assert( cameraEventInterrupt );

	DWORD threadID = 0;
	cameraThreadHandle = CreateThread( 0, 0x1000, cameraThread, cameraEventInterrupt, 0, &threadID );
	SetThreadPriority( cameraThreadHandle, THREAD_PRIORITY_ABOVE_NORMAL );
	assert( cameraThreadHandle );
}

void cameraShutdown() {
	TerminateThread( cameraThreadHandle, 0 );
	cameraThreadHandle = 0;

	// CLEANUP memory occupied by controller
	if( !gCameraSimulate ) {
		PICM_CleanUp();
	}
}

*/
