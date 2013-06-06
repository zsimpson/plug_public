// @ZBS {
//		*SDK_DEPENDS measurecompute
// }

// OPERATING SYSTEM specific includes:
#include "windows.h"
#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glfw.h"
// SDK includes:
#include "cbw.h"
#include "fftgsl.h"
// STDLIB includes:
#include "assert.h"
#include "string.h"
#include "math.h"
#include "limits.h"
#include "stdlib.h"
#ifdef WIN32
#include "malloc.h"
#endif
// MODULE includes:
// ZBSLIB includes:
#include "zplugin.h"
#include "zviewpoint.h"
#include "zmsg.h"
#include "zglfont.h"
#include "zvars.h"

ZPLUGIN_BEGIN( tfunc );

ZVAR( int, Tfunc_sweepSpeed, 8 );

//-----------------------------------------------------------------------------------
// Measurement computing hacked-up interface
//-----------------------------------------------------------------------------------
// For now, hacky global buffer
long lastCurIndex = -1;
short boardStatus;
int boardNum = 0;
short *buffer = 0;
long bufferSize = 0;
float bufferSizeInSecs = 0.f;
int numChannels = 6;
int lowChannel = 0;


void mcBoardStart() {
	// STARTUP the Measurement Copmuting board
	int cbRet = 0;

	// DECLARE Revision level of the Universal Library
	float revision = (float)CURRENTREVNUM;
	cbRet = cbDeclareRevision(&revision);
	assert( cbRet == 0 );

	// INITIATE error handling
	cbRet = cbErrHandling( PRINTALL, DONTSTOP );
	assert( cbRet == 0 );

	int ret = cbSetConfig( BOARDINFO, boardNum, 0, BINUMADCHANS, 8 );
	assert( ret == 0 );
}
void mcBoardAnalogInStart() {
	// Collect the values with cbAInScan() in BACKGROUND mode, CONTINUOUSLY
	//	Parameters:
	//		 boardNum: the number used by CB.CFG to describe this board
	//		 lowChannel: low channel of the scan
	//		 highChannel: high channel of the scan
	//		 bufferSize: the total number of A/D samples to collect
	//		 sampleRatePerChannel: sample rate in samples per second
	//		 gainMode: the gain for the board
	//		 buffer[]: the array for the collected data values
	//		 options: data collection options 
	lowChannel = 0;
	numChannels = 1;
	int highChannel = lowChannel + numChannels - 1;
	long sampleRatePerChannel = 600;

	// COMPUTE the buffer size which must be a multiple of 4096 and of the numChannels.
	long bufferSize = numChannels * 4096 * 100;
	float bufferSizeInSecs = (float)bufferSize / ( (float)sampleRatePerChannel * (float)numChannels );
		// Computed just for reference.  Several seconds seems reasonable
			
	int gainMode = UNI10VOLTS;// BIP20VOLTS;

	buffer = (short*)cbWinBufAlloc(bufferSize);
	assert( buffer );

	// BEGIN acquisition
	int cbRet = cbAInScan( boardNum, lowChannel, highChannel, bufferSize, &sampleRatePerChannel, gainMode, buffer, BACKGROUND | CONTINUOUS );
	assert( cbRet == 0 );
}

void mcBoardAnalogInStop() {
	// The BACKGROUND operation must be explicitly stopped
	int cbRet = cbStopBackground( boardNum, AIFUNCTION );
	assert( cbRet == 0 );

	cbRet = cbWinBufFree( buffer );
	assert( cbRet == 0 );
}

void mcBoardSample( short *&readBuf, int &sampleCountPerChannel ) {
	// CHECK the boardStatus of the current background operation
	// Parameters:
	//   boardNum: the number used by CB.CFG to describe this board
	//   boardStatus: current boardStatus of the operation (IDLE or RUNNING)
	//   curCount: current number of samples collected
	//   curIndex: index to the last data value transferred 
	//   functionType: A/D operation (AIFUNCTIOM)
	long curCount = 0;
	long curIndex = 0;
	int cbRet = cbGetStatus( boardNum, &boardStatus, &curCount, &curIndex, AIFUNCTION );
	assert( cbRet == 0 );

	// COMPUTE the statistics for each channel
	int numPoints;
	int startIndex = lastCurIndex;

	sampleCountPerChannel = 0;
	readBuf = 0;

	if( lastCurIndex != curIndex ) {
		if( lastCurIndex > curIndex ) {
			// We wrapped around the end of the buffer so we analyze only the data at the end of the buffer
			// and we set the lastCurIndex to zero so that on the next sample we will read what we
			// didn't read on this frame plus the new stuff of the next frame
			numPoints = (bufferSize - lastCurIndex - 1) / numChannels;
			lastCurIndex = 0;
		}
		else {
			// Usual case where the curIndex has grown relative to the lastCurIndex
			numPoints = (curIndex - lastCurIndex) / numChannels;
			lastCurIndex = curIndex;
		}

		readBuf = &buffer[ startIndex ];
		sampleCountPerChannel = numPoints;
	}
}

unsigned short digOut = 0;
short displayBuffer[ 0x10000 ];
int displayBufferWrite = 0;
FILE *f = 0;
int fileSize = 0;

void render() {
	int j;

	digOut = (digOut + Tfunc_sweepSpeed) & ((1<<14)-1);
	cbDOut( boardNum, AUXPORT, digOut );

	short *readBuf = 0;
	int sampleCountPerChannel;
	mcBoardSample( readBuf, sampleCountPerChannel );
	if( readBuf && sampleCountPerChannel > 0 ) {
		int remain = 0x10000 - displayBufferWrite;
		int count1 = min( remain, sampleCountPerChannel );
		memcpy( &displayBuffer[displayBufferWrite], readBuf, sizeof(short) * count1 );

		if( f ) {
			for( j=0; j<count1; j++ ) {
				fprintf( f, "%d %d\n", digOut, displayBuffer[displayBufferWrite+j] );
			}
			fileSize += count1;
		}

		sampleCountPerChannel -= count1;
		displayBufferWrite = ( displayBufferWrite + count1 ) % 0x10000;
		memcpy( &displayBuffer[displayBufferWrite], &readBuf[count1], sizeof(unsigned short) * sampleCountPerChannel );

		if( f ) {
			for( j=0; j<sampleCountPerChannel; j++ ) {
				fprintf( f, "%d %d\n", digOut, displayBuffer[displayBufferWrite+j] );
			}
			fileSize += sampleCountPerChannel;
		}

		displayBufferWrite = ( displayBufferWrite + sampleCountPerChannel ) % 0x10000;

	}
	
	if( f ) {
		fflush( f );
	}

	if( f && fileSize > 0x10000 ) {
		fclose( f );
		f = 0;
	}

	glScalef( 1.f/32000.f, 1.f/32000.f, 1.f/32000.f );
	zviewpointSetupView();
	glColor3ub( 0, 0, 0 );
	glBegin( GL_LINES );
	for( int y=0; y<0x8000; y+=0x200 ) {
		glVertex2i( 0, y );
		glVertex2i( 0x10000, y );
	}
	glEnd();

	glColor3ub( 255, 0, 0 );
	glBegin( GL_POINTS );
	for( int i=0; i<0x10000; i++ ) {
		glVertex2i( i, (int)displayBuffer[i] );
	}
	glEnd();
}

void startup() {
	mcBoardAnalogInStop();

	f = fopen( "tfunc_dump2.txt", "wb" );

	zglFontLoad( "mono", "consola.ttf", 10.f );
	mcBoardStart();
	mcBoardAnalogInStart();
	cbDConfigPort( boardNum, AUXPORT, DIGITALOUT );
}

void shutdown() {
	mcBoardAnalogInStop();
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

