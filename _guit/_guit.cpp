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
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
//#include "zsound.h"
#include "ztmpstr.h"
#include "zviewpoint.h"
#include "zglfwtools.h"
#include "zmathtools.h"
#include "zglfont.h"
#include "zgltools.h"
#include "zmousemsg.h"
#include "zui.h"
#include "ztime.h"
#include "zwildcard.h"
#include "zregexp.h"
#include "fft.h"

ZPLUGIN_BEGIN( guit );


ZVAR( int, Guit_windowStart, 0 );
	// Used for dragging the mouse around for debugging.  Where the start of the analysis window is
ZVAR( float, Guit_windowChunkSizeInMS, 0 );
	// Read-only: display WINDOW_CHUNK_SIZE in MS
ZVAR( float, Guit_minVolRatio, 45.0 );
	// How much louder than the quietest chunk something has to be to be detected
ZVARB( int, Guit_viewDrawGraphics, 1, 0, 1 );
	// Draw graphics
ZVARB( int, Guit_viewDrawGlobalGraphics, 1, 0, 1 );
	// Draw the full analysis graphics
ZVAR( float, Guit_viewVol, 1.7e-2 );
	// Scaling of the signal
ZVAR( float, Guit_viewCorrFuncScale, 1.4e-6 );
	// Scaling of the correlation function draw in the corder
ZVAR( float, Guit_viewPsdFuncScale, 1 );
	// Scaling of the psd function draw in the corder
ZVAR( float, Guit_viewPsdFuncX, 1 );
	// Scaling of the psd function draw in the corder
ZVAR( double, Guit_time, 0.0 );
	// Read-only display of the time

ZVAR( float, Guit_highFreqDetectScale, 1.17e8 );
	// When the high freq exceed this we call it an event
	// @TODO make 6 different version of this or make a calibrator
ZVAR( float, Guit_acceptNormalCorr, 0.3 );
	// @TODO: again probasbly want to calibrate?


ZVAR( float, Guit_scaleX, 1e1 );
ZVAR( float, Guit_scaleY, 1.0 );
ZVAR( float, Guit_spacing, 1.2e3 );

ZVAR( int, Guit_viewWhichAC, 0 );

int useBoardSampling = 0;

//-----------------------------------------------------------------------------------
// Measurement computing hacked-up interface
//-----------------------------------------------------------------------------------
// For now, hacky global buffer
long lastCurIndex = -1;
short boardStatus;
int boardNum = 1;
short *buffer = 0;
long bufferSize = 0;
float bufferSizeInSecs = 0.f;
int numChannels = 6;
int lowChannel = 0;

void mcBoardDigialOutStart() {
	// STARTUP the Measurement Copmuting board
	int cbRet = 0;

	// DECLARE Revision level of the Universal Library
	float revision = (float)CURRENTREVNUM;
	cbRet = cbDeclareRevision(&revision);
	assert( cbRet == 0 );

	// INITIATE error handling
	cbRet = cbErrHandling( PRINTALL, DONTSTOP );
	assert( cbRet == 0 );

	cbDConfigPort( 0, AUXPORT, DIGITALOUT );
	cbDOut( 0, AUXPORT, (signed short)(0x7FFF) );

}

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
	numChannels = 6;
	int highChannel = lowChannel + numChannels - 1;
	long sampleRatePerChannel = 44100;

	// COMPUTE the buffer size which must be a multiple of 4096 and of the numChannels.
	long bufferSize = numChannels * 4096 * 100;
	float bufferSizeInSecs = (float)bufferSize / ( (float)sampleRatePerChannel * (float)numChannels );
		// Computed just for reference.  Several seconds seems reasonable
			
	int gainMode = BIP2PT5VOLTS;
	//	int gainMode = UNI10VOLTS;

	buffer = (short*)cbWinBufAlloc(bufferSize);
	assert( buffer );

	// BEGIN acquisition
	cbRet = cbAInScan( boardNum, lowChannel, highChannel, bufferSize, &sampleRatePerChannel, gainMode, buffer, BACKGROUND | CONTINUOUS );
	assert( cbRet == 0 );
}

void mcBoardStop() {
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


//-----------------------------------------------------------------------------------
// Sample globals
//-----------------------------------------------------------------------------------
const int WINDOW_CHUNK_SIZE = 512;
	// Size of an analysis chunk.  512 is 11.59 ms at 44150 Hz
const int WINDOW_CHUNK_COUNT = 4;
	// Maximum number of chunks to search back

int sizeInSamples[6];
	// The total number of samples loaded for each string
signed short *hex[6];
	// The samples off the hex pickup, one for each stinrg. 16-bit 44100 Hz
int smallestSizeInSamples;
	// The smallest of the loaded samples (used as the length for all for simplicity)
signed short *combined = 0;
	// A buffer allocated so that we can hear the combined effect of all of them

float minStdev[6];
float maxStdev[6];
	// The minimum & maximum chunk-sized stdev across the whole sample
	// This simulates some sort of long-term volume tracking or calibration
	
//-----------------------------------------------------------------------------------
// writeFile
//-----------------------------------------------------------------------------------

FILE *writeFile = 0;
void writeFileOpen() {
	ZWildcard w( "guitsample_*" );
	ZRegExp regex( "guitsample_(\\d+)" );
	int largestSample = 0;
	while( w.next() ) {
		if( regex.test(w.getName()) ) {
			largestSample = max( largestSample, regex.getI(1) );
		}
	}

	writeFile = fopen( ZTmpStr("guitsample_%d.dat",largestSample+1), "wb" );
	assert( writeFile );
}

void writeFileClose() {
	if( writeFile ) {
		fclose( writeFile );
		writeFile = 0;
	}
}

void readFile( char *filename ) {
	int i, j;

	FILE *f = fopen( filename, "rb" );
	assert( f );
	fseek( f, 0, SEEK_END );
	int len = ftell( f );
	fseek( f, 0, SEEK_SET );
	int numSamples = len / (6*2);
		// Six channels, 2 bytes per channel
		
	short *loadBuf = (short *)malloc( len );
	memset( loadBuf, 0, len );
		// over 2 because there's two bytes per sample
		
	int read = fread( loadBuf, 1, len, f );
		
	combined = new short[ numSamples ];
	memset( combined, 0, numSamples * 2 );


	int maxPerChannel[6] = { 0, };
	double sum[6] = { 0, };	

	//FILE *out = fopen( "/transfer/recording.txt", "wb" );
	//for( j=0; j<numSamples; j++ ) {
	//	for( i=0; i<6; i++ ) {
	//		fprintf( out, "%d ", loadBuf[ j*6 + i ] );
	//	}
	//	fprintf( out, "\n" );
	//}
	//fclose( out );

	for( i=0; i<6; i++ ) {
		sizeInSamples[i] = numSamples;
		hex[i] = new short[ numSamples ];
		for( j=0; j<numSamples; j++ ) {
			hex[i][j] = loadBuf[ j*6 + i ];

			sum[i] += double( hex[i][j] );
			maxPerChannel[i] = max( maxPerChannel[i], abs(hex[i][j]) );
		}
	}

	// MEAN
	for( i=0; i<6; i++ ) {
		sum[i] /= (double)numSamples;
	}
	
	// ELIMINATE DC bias
	for( i=0; i<6; i++ ) {
		for( j=0; j<numSamples; j++ ) {
			hex[i][j] = (short)max( (double)SHRT_MIN, min( (double)SHRT_MAX, double(hex[i][j]) - sum[i] ) * 10.0 );
			combined[j] += hex[i][j] / 6;
				// @TODO: Probably bias entering here due to rounding cutoff
		}
	}

	smallestSizeInSamples = sizeInSamples[0];
	free( loadBuf );
}


//-----------------------------------------------------------------------------------
// samples
//-----------------------------------------------------------------------------------

float samplesToHertz( float samples ) {
	if( samples > 0.f ) {
		return 44100.f / samples;
	}
	return 0.f;
}

float hertzToSamples( float hertz ) {
	if( hertz > 0.f ) {
		return 44100.f / hertz;
	}
	return 0.f;
}




// CorrStat is a struct that contains the results of analysis of a chunk

struct CorrStat {
	float corr;
		// The peak correlation from the auto-correlation function
	float normalizedCorr;
		// The peak correlation from the auto-correlation function scaled by the zero-shift correlation
	int periodInSamples;
		// The period of the peak correlation point

	int toneAcceptedInChunks;
		// How many chunks back were needed for the analysis
	int globalSampleNumber;
		// Used only for debugging
	int corrFuncCount;
		// The number of samples in the corrFunc
	float corrFunc[4096];
		// The samples auto-correlation function.  This is only stored here for
		// debugging visualization purposes.  It isn't needed for the real system

	int psdSize;
	float psd[512 * 4];
		// Power spectral density worst case is the chunk size (512) times max chunks (4) / 2
		// The samples auto-correlation function.  This is only stored here for
		// debugging visualization purposes.  It isn't needed for the real system
		
	float highFreqMean;
	int foundHighFreqInChunks;

	int toneFound() {
		return normalizedCorr > Guit_acceptNormalCorr;
	}
	int hasHighFreq() {
		return highFreqMean > Guit_highFreqDetectScale;
	}
	int inNote() {
		// This has memory
		// @TODO
	}
};


#define M_PI 3.14159265358979323846f
#define SMB_FFT (-1)
#define SMB_IFFT (+1)

void smbFft(float *fftBuffer, long fftFrameSize, long sign) {
	 // FFT routine, (C)1996 S.M.Bernsee. Sign = -1 is FFT, 1 is iFFT (inverse)
	 // Fills fftBuffer[0...2*fftFrameSize-1] with the Fourier transform of the time 
	 // domain data in fftBuffer[0...2*fftFrameSize-1]. The FFT array takes and returns
	 // the cosine and sine parts in an interleaved manner, ie. 
	 // fftBuffer[0] = cosPart[0], fftBuffer[1] = sinPart[0], asf. fftFrameSize must 
	 // be a power of 2. It expects a complex input signal (see footnote 2), ie. when 
	 // working with 'common' audio signals our input signal has to be passed as 
	 // {in[0],0.,in[1],0.,in[2],0.,...} asf. In that case, the transform of the 
	 // frequencies of interest is in fftBuffer[0...fftFrameSize].
    float wr, wi, arg, *p1, *p2, temp;
    float tr, ti, ur, ui, *p1r, *p1i, *p2r, *p2i;
    long i, bitm, j, le, le2, k, logN;
    logN = (long)(logf(float(fftFrameSize))/logf(2.f)+0.5f);
	
    for (i = 2; i < 2*fftFrameSize-2; i += 2) {
        for (bitm = 2, j = 0; bitm < 2*fftFrameSize; bitm <<= 1) {
            if (i & bitm) j++;
            j <<= 1;
		}
		if (i < j) {
			p1 = fftBuffer+i; p2 = fftBuffer+j;
			temp = *p1; *(p1++) = *p2;
			*(p2++) = temp; temp = *p1;
			*p1 = *p2; *p2 = temp;
		}
	}
	
	for (k = 0, le = 2; k < logN; k++) {
		le <<= 1;
		le2 = le>>1;
		ur = 1.0;
		ui = 0.0;
		arg = M_PI / (le2>>1);
		wr = cosf(arg);
		wi = sign*sin(arg);
		for (j = 0; j < le2; j += 2) {
			p1r = fftBuffer+j; p1i = p1r+1;
			p2r = p1r+le2; p2i = p2r+1;
			for (i = j; i < 2*fftFrameSize; i += le) {
				tr = *p2r * ur - *p2i * ui;
				ti = *p2r * ui + *p2i * ur;
				*p2r = *p1r - tr; *p2i = *p1i - ti;
				*p1r += tr; *p1i += ti;
				p1r += le; p1i += le;
				p2r += le; p2i += le;
			}
			tr = ur*wr - ui*wi;
			ui = ur*wi + ui*wr;
			ur = tr;
		}
	}
}

// The latching algoritm is
// Latch when high (you've detected a rhythm event, show a fat circle)
// If tone && latch then you can show the freq happy colored circle
// Delatch when (!high && tone) || (!tone for 4 chunks)

void windowPeakAnalyze( short *samples, int sampleCount, int minWaveLen, int maxWaveLen, CorrStat &returnCorrStat ) {
	// samples are the 16-bit mono samples to be analyzed of length sampleCount
	// useFullCorrelation is a flag about which mode to use
	// cutoff is the value above which is considered to be a peak.  This is only used if useFullCorrelation==0
	// minWaveLen & maxWaveLen are the min and max sample length for the given string
	// returnCorrStat is where the results are returned

	int i;

	// FIND the maximum point in using the auto-correlation
	float max0Corr = 0.0;
	int max0CorrShift = 0;
	returnCorrStat.corrFuncCount = sampleCount/2-1;
	
	// COMPUTE the Auto-correlation by FFT then Complex Conjugate then IFFT
	assert( sampleCount == 256 || sampleCount == 512 || sampleCount == 1024 || sampleCount == 2048 );
	float *fBuffer = (float *)alloca( sizeof(float) * sampleCount * 2 );
	for( i=0; i<sampleCount; i++ ) {
		float hammingWindow = 0.54f - 0.46f * cosf( 2.f * PIF * (float)i / float(sampleCount-1) );
		fBuffer[i*2+0] = float( samples[i] ) * hammingWindow;
		fBuffer[i*2+1] = 0.f;
	}

	// FORWARD FFT	
	smbFft( fBuffer, sampleCount, SMB_FFT );

	// STORE the psd before we start messing with it
	int psdSize = sampleCount/2;
	returnCorrStat.psdSize = psdSize;
	float psdAccum = 0.f;
	int accumCount = 0;
	for( i=0; i<sampleCount; i++ ) {
		returnCorrStat.psd[i] = fBuffer[i*2+0]*fBuffer[i*2+0] + fBuffer[i*2+1]*fBuffer[i*2+1];
		if( i > 3*psdSize/4 && i < psdSize ) {
			// The psd is symmetric so we only want to look at the
			// top 1/4 of the bottom half.  But, to get the autocorrelation
			// from the IFFT we have to have the entire symmetric FFT
			psdAccum += returnCorrStat.psd[i];
			accumCount++;
		}
	}
	returnCorrStat.highFreqMean = psdAccum / (float)accumCount;

	// COMPLEX CONJUGATE
	for( i=0; i<sampleCount; i++ ) {
		fBuffer[i*2+0] = returnCorrStat.psd[i];
		fBuffer[i*2+1] = 0.f;
	}

	// INVERSE FFT	
	smbFft( fBuffer, sampleCount, SMB_IFFT );
	
	float sampCount2 = ( (float)sampleCount * (float)sampleCount );
	float zeroShiftCorr = fBuffer[0] / sampCount2;
	for( i=0; i<sampleCount; i++ ) {
		// SKIP out of possible string range
		if( i >= minWaveLen && i <= maxWaveLen ) {
			returnCorrStat.corrFunc[i] = fBuffer[i*2] / sampCount2;
		}
		else {
			returnCorrStat.corrFunc[i] = 0.f;
		}
	}

	float maxCorr = 0.f;
	int maxCorrShift = 0;
	for( i=0; i<sampleCount/2-1; i++ ) {
		float corr = returnCorrStat.corrFunc[i];
		if( corr > maxCorr ) {
			maxCorrShift = i;
			maxCorr = corr;
		}
	}

	// COMPUTE the energy (zero-shift correlation) for normalization for the real AC funciton
	returnCorrStat.corr = maxCorr;
	returnCorrStat.normalizedCorr = maxCorr / zeroShiftCorr;
	returnCorrStat.periodInSamples = maxCorrShift;
}

float stdevS16( signed short *data, int count ) {
	// 16-bit signed standard deviation, data assumed to be zero-centered
	__int64 sumSqr = 0;
	for( int i=0; i<count; i++ ) {
		sumSqr += (*data) * (*data);
		data++;
	}
	return sqrtf( (float)sumSqr / (float)count );
}


void analyze( short *endOfSampleBuffers[6], int chunkSizeInSamples, int maxChunkCount, CorrStat *corrStats ) {
	ZTime start = zTimeNow();

	// This assumes that the sample buffers are contiguous and that you are giving it a pointer
	// to the end of that buffer (the current read pos) and that the samples buffer is
	// at least chunkSizeInSample * maxChunkCount big
	// Eventually, this should probably deal with circular buffers

	// MIN and MAX allowable wave length (in samples) for each string:
	// 9/10ths of the lowest possible (tuned) frequency for each string
	// 11/10ths of the highest possible (tuned) frequency for each string
	int minWaveLen[] = {
		9*( 44100 / (329*4) )/10,
		9*( 44100 / (247*4) )/10,
		9*( 44100 / (196*4) )/10,
		9*( 44100 / (164*4) )/10,
		9*( 44100 / (110*4) )/10,
		9*( 44100 / ( 82*4) )/10,
	};

	int maxWaveLen[] = {
		11*( 44100 / (329*1) )/10,
		11*( 44100 / (247*1) )/10,
		11*( 44100 / (196*1) )/10,
		11*( 44100 / (164*1) )/10,
		11*( 44100 / (110*1) )/10,
		11*( 44100 / ( 82*1) )/10,
	};

//	int minChunkAnalyzePerString[6] = { 1, 1, 1, 1, 2, 4 };
	int minChunkAnalyzePerString[6] = { 4, 4, 4, 4, 4, 4 };
		// @TODO: Rethink this optimization


	// latch detection when event is detected (high freq is large)
	// delatch the detection when the freq analysis no longer has signal OR four chuncks has past

	for( int string=0; string<6; string++ ) {
		corrStats[string].foundHighFreqInChunks = 0;

		int size = WINDOW_CHUNK_COUNT;
		int c = 4;
		for( int c=minChunkAnalyzePerString[string]; c<=WINDOW_CHUNK_COUNT; c++ ) {
			// The lower strings always need more analysis than do the higher strings
			// because they can easily be confused.  So the E string requires 3 chunks (45 ms)
			// and the others less.
			
			if( c == 3 ) {
				c=4;
				// FFTs have to be power of 2 so we can't use 3 chunks only 4
				// This is a stupid hack because we're tired.  The right way is
				// to do a series of doublings
			}
			
			corrStats[string].toneAcceptedInChunks = 0;

			windowPeakAnalyze(
				&endOfSampleBuffers[string][ - c*WINDOW_CHUNK_SIZE],
				c*chunkSizeInSamples,
				minWaveLen[string],
				maxWaveLen[string],
				corrStats[string]
			);

			if( corrStats[string].foundHighFreqInChunks==0 && corrStats[string].hasHighFreq() ) {
				corrStats[string].foundHighFreqInChunks = c;
			}

			if( corrStats[string].toneFound() ) {
				corrStats[string].toneAcceptedInChunks = c;
				break;
			}
		}

	}

	ZTime stop = zTimeNow();
	Guit_time = 1000.0 * (stop - start);
}


int globalCorrStatsCount = 0;
CorrStat *globalCorrStats = 0;
	// These are for the global display of the analysis.  This is purely for development
	// and debugging.  Obviously in a real system, you'd only keep around the last
	// 60 ms or so of data.


void printAtWorld( FVec2 worldPos, char *text ) {
	// Debugging routine to print text at a given place in the world
	FVec3 windowPos;
	zglProjectWorldCoordsOnScreenPlane( worldPos, windowPos );
	glPushMatrix();
		zglPixelMatrixFirstQuadrant();
		glColor3ub( 0, 0, 0 );
		zglFontPrint( text, windowPos.x, windowPos.y, "controls" );
		glColor3ub( 255, 255, 255 );
		zglFontPrint( text, windowPos.x-1, windowPos.y+1, "controls" );
	glPopMatrix();
}

void render() {
	// This is called once per frame, so it simulates what the game loop.
	// It currently analyzes based on where the mouse has selected in the
	// data.  Of course, in a real app it would analyze the newly arrived data.

	int string;

	Guit_windowChunkSizeInMS = 1000.f * (float)(WINDOW_CHUNK_SIZE) / 44100.f;
		// For display purposes, 

	glTranslatef( -1.f, -1.f, 0.f );
	glScalef( 0.0001f, 0.0001f, 0.0001f );
	zviewpointSetupView();

	if( useBoardSampling ) {
		// READ data from the device
		short *readBuf = 0;
		int readSampleCountPerChannel = 0;
		mcBoardSample( readBuf, readSampleCountPerChannel );

		if( readSampleCountPerChannel > 0 ) {
			if( writeFile ) {
				fwrite( readBuf, readSampleCountPerChannel * 6 * 2, 1, writeFile );
			}

			for( int ch=0; ch<numChannels; ch++ ) {
				int i, y;

				glColor3ub( 128, 128, 128 );
				glBegin( GL_LINES );
				glVertex2f( 0.f, 0.f );
				glVertex2f( 1000000.f, 0.f );
				glEnd();

				unsigned int sum = 0;
				for( i=0; i<readSampleCountPerChannel; i++ ) {
					y = readBuf[ i*numChannels + ch ];
					sum += y;
				}

				float mean = (float)sum / (float)readSampleCountPerChannel;

				glColor3ub( 255, 255, 255 );
				glBegin( GL_LINE_STRIP );
				for( i=0; i<readSampleCountPerChannel; i++ ) {
					y = readBuf[ i*numChannels + ch ];
					glVertex2f( Guit_scaleX*(float)i, Guit_scaleY*((float)y-mean) );
				}
				glEnd();
				glTranslatef( 0.f, Guit_spacing, 0.f );
			}
		}
		return;
	}

	if( zMouseMsgLState && ZUI::zuiFindByName("controlPanel")->getI("hidden") ) {
		// Move the start to where the mouse is dragging, only if the control panel is not up
		FVec2 mousePos( (float)zMouseMsgX, (float)zMouseMsgY );
		FVec3 mouseWorldPos;
		zglProjectScreenCoordsOnWorldPlane( mousePos, FVec3::Origin, FVec3::ZAxis, mouseWorldPos );
		Guit_windowStart = (int)mouseWorldPos.x;
	}
	
	CorrStat mouseCorrStats[6] = {0,};

	Guit_windowStart = max( WINDOW_CHUNK_SIZE * WINDOW_CHUNK_COUNT, Guit_windowStart );
	short *sampleBuffers[6];
	for( int i=0; i<6; i++ ) {
		sampleBuffers[i] = &hex[i][Guit_windowStart];
	}
	analyze( sampleBuffers, WINDOW_CHUNK_SIZE, WINDOW_CHUNK_COUNT, mouseCorrStats );

	if( Guit_viewDrawGraphics ) {
		unsigned char palette[6][3] = {
			{ 255, 0, 0 },
			{ 0, 255, 0 }, 
			{ 0, 0, 255 }, 
			{ 255, 255, 0 },
			{ 255, 0, 255 },
			{ 0, 255, 255 },
		};

		// PLOT the functions
		const float yDrawOffsetPerString = 2e3f;
		
		glPushMatrix();
			for( string=0; string<6; string++ ) {
				glColor3ubv( palette[string] );
				glBegin( GL_LINE_STRIP );
				for( int t=0; t<sizeInSamples[string]; t++ ) {
					glVertex2f( (float)t, Guit_viewVol * (float)hex[string][t] );
				}
				glEnd();

				glTranslatef( 0, yDrawOffsetPerString, 0 );
			}
		glPopMatrix();
		
		// DRAW the chunk lines
		glBegin( GL_LINES );
			glColor3ub( 255, 255, 255 );
			glVertex2i( Guit_windowStart, 0 );
			glVertex2i( Guit_windowStart, 100000 );
			glColor3ub( 240, 240, 240 );
			for( int i=1; i<=WINDOW_CHUNK_COUNT; i++ ) {
				glVertex2i( Guit_windowStart-WINDOW_CHUNK_SIZE*i, 0 );
				glVertex2i( Guit_windowStart-WINDOW_CHUNK_SIZE*i, 100000 );
			}
		glEnd();
		

		// DRAW the autocorrelation function found from the mouse-selected samples
		glPushMatrix();
		zglPixelMatrixFirstQuadrant();
		for( string=0; string<6; string++ ) {
			glTranslatef( 0.f, 20.f, 0.f );
			glColor3ubv( palette[string] );

			glBegin( GL_LINE_STRIP );
			for( int i=0; i<mouseCorrStats[string].corrFuncCount; i++ ) {
				glVertex2f( (float)i, mouseCorrStats[string].corrFunc[i] * Guit_viewCorrFuncScale );
			}
			glEnd();

			if( mouseCorrStats[string].toneFound() ) {
				glLineWidth( 3.f );
				glBegin( GL_LINES );
					glVertex2i( mouseCorrStats[string].periodInSamples, 0 );
					glVertex2f( (float)mouseCorrStats[string].periodInSamples, mouseCorrStats[string].corr * Guit_viewCorrFuncScale );
				glEnd();
				glLineWidth( 1.f );
			}
		}
	
		glPopMatrix();


		// DRAW the psd function found from the mouse-selected samples
		glPushMatrix();
		zglPixelMatrixFirstQuadrant();
		glTranslatef( 0.f, 7*20.f, 0.f );
		glScalef( Guit_viewPsdFuncX, 1.f, 1.f );
		for( string=0; string<6; string++ ) {
			glTranslatef( 0.f, 50.f, 0.f );
			glColor3ubv( palette[string] );
			
			glBegin( GL_LINE_STRIP );
			for( int i=0; i<mouseCorrStats[string].psdSize; i++ ) {
				glVertex2f( (float)i, logf( mouseCorrStats[string].psd[i]+1.f ) * Guit_viewPsdFuncScale );
			}
			glEnd();

			glBegin( GL_LINES );
				glVertex2i( 0, 0 );
				glVertex2i( mouseCorrStats[string].psdSize, 0 );

			glEnd();

			glEnable( GL_LINE_STIPPLE );
			glLineStipple( 1, 0xF0F0 );
			glBegin( GL_LINES );
				float highFreqDetect = logf(Guit_highFreqDetectScale+1.f);
				glVertex2f( 0.f, highFreqDetect );
				glVertex2f( (float)mouseCorrStats[string].psdSize, highFreqDetect );
			glEnd();
			glDisable( GL_LINE_STIPPLE );

		}
		glPopMatrix();


		// DEBUG trace about
		glPushMatrix();
			glMatrixMode(GL_MODELVIEW);
			zglPixelMatrixInvertedFirstQuadrant();
			
			float y = 6 * 12.f;
			for( string=0; string<6; string++ ) {		
				glColor3ubv( palette[string] );

				zglFontPrintInverted(
					ZTmpStr(
						"%4d %s %d %s %d",
						(int)( 0.5f + samplesToHertz( float(mouseCorrStats[string].periodInSamples) ) ),
						mouseCorrStats[string].toneFound() ? "TONE" : "    ",
						mouseCorrStats[string].toneAcceptedInChunks,
						mouseCorrStats[string].hasHighFreq() ? "HIGH" : "    ",
						mouseCorrStats[string].foundHighFreqInChunks
					),
					0, y, "mono"
				);
				y -= 12.f;
			}
		glPopMatrix();


		if( Guit_viewDrawGlobalGraphics ) {
			// Draw the informatiom from the global analysis
		
			// DRAW the chunk lines
			glColor3ub( 200, 200, 200 );
			glBegin( GL_LINES );
				for( int i=0; i<globalCorrStatsCount; i+=6 ) {
					glVertex2i( globalCorrStats[i].globalSampleNumber, 0 );
					glVertex2i( globalCorrStats[i].globalSampleNumber, 100000 );
				}
			glEnd();

			for( int i=0; i<globalCorrStatsCount; i++ ) {
				int string = i%6;

				if( globalCorrStats[i].hasHighFreq() ) {
					
					glColor3ubv( palette[string] );
					glBegin( GL_QUADS );
						glVertex2f( (float)globalCorrStats[i].globalSampleNumber+000, float(string+0.0f) * yDrawOffsetPerString );
						glVertex2f( (float)globalCorrStats[i].globalSampleNumber+100, float(string+0.0f) * yDrawOffsetPerString );
						glVertex2f( (float)globalCorrStats[i].globalSampleNumber+100, float(string+0.9f) * yDrawOffsetPerString );
						glVertex2f( (float)globalCorrStats[i].globalSampleNumber+000, float(string+0.9f) * yDrawOffsetPerString );
					glEnd();
				}

				if( globalCorrStats[i].toneFound() ) {
					glColor3ub( 0, 0, 0 );
					printAtWorld( FVec2( (float)(globalCorrStats[i].globalSampleNumber), 2e3f*(float)(i%6) ), ZTmpStr("%3.0f",samplesToHertz((float)globalCorrStats[i].periodInSamples)) );
				}
			}
		}

	}
}

void analyzeEntireTrace() {
	// This function scans the entire loaded trace and performs analysis
	// Of course, this is for debugging / development only.  No such function
	// would exist in the real product.


	// SCAN to the whole thing getting statistics to determine maximum volume.
	// In a real system this would be done during a tuning or calibration phase to
	// determine the dynamic range.
	int i;
	for( i=0; i<6; i++ ) {
		minStdev[i] = FLT_MAX;
		maxStdev[i] = 0.f;
	}
	for( int t=WINDOW_CHUNK_SIZE * WINDOW_CHUNK_COUNT; t<smallestSizeInSamples; t+=WINDOW_CHUNK_SIZE ) {
		for( int i=0; i<6; i++ ) {
			float dev = stdevS16( &hex[i][t], WINDOW_CHUNK_SIZE );
			minStdev[i] = min( minStdev[i], dev );
			maxStdev[i] = max( maxStdev[i], dev );
		}
	}

	// ANALYZE the whole trace, 15 ms at a time and store the entire thing into a global buffer for display
	if( globalCorrStats ) {
		delete globalCorrStats;
	}
	globalCorrStatsCount = 0;
	globalCorrStats = new CorrStat[ 6 * (smallestSizeInSamples / WINDOW_CHUNK_SIZE) ];
	memset( globalCorrStats, 0, sizeof(CorrStat) * 6 * (smallestSizeInSamples / WINDOW_CHUNK_SIZE) );

	for( int t=WINDOW_CHUNK_SIZE * WINDOW_CHUNK_COUNT; t<smallestSizeInSamples; t+=WINDOW_CHUNK_SIZE ) {
		short *sampleBuffers[6];
		for( int i=0; i<6; i++ ) {
			sampleBuffers[i] = &hex[i][t];
			globalCorrStats[globalCorrStatsCount+i].globalSampleNumber = t;
		}
		analyze( sampleBuffers, WINDOW_CHUNK_SIZE, WINDOW_CHUNK_COUNT, &globalCorrStats[globalCorrStatsCount] );
		globalCorrStatsCount += 6;
	}
}


void startup() {
	zglFontLoad( "mono", "consola.ttf", 10.f );

mcBoardDigialOutStart();

	if( useBoardSampling ) {
		mcBoardStart();
	}
	else {
		readFile( "c:/transfer/popank/guitsample_5.dat" );
	}


	// INITIALIZE the sound system
//	int frontWindowHWND = glfwZBSExt_GetHWND();
//	zsoundInit( (void *)frontWindowHWND );
}

void shutdown() {
	if( useBoardSampling ) {
		mcBoardStop();
	}
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;
	
	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		// If "q" is pressed, perform entire anaysis
		analyzeEntireTrace();
	}

	if( zmsgIs(type,Key) && zmsgIs(which,w) ) {
		// If "w" is pressed, listen to the combined signal
		//zsoundPlayBuffer( (char *)combined, sizeInSamples[0]*2, 44100, 2, 0, 2.0f, 0.0f, 1.0f );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,e) ) {
		if( writeFile ) {
			writeFileClose();
		}
		else {
			writeFileOpen();
		}
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

