// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "stdio.h"
#include "string.h"
#include "assert.h"
#include "stdlib.h"
#ifdef WIN32
#include "malloc.h"
#endif
// MODULE includes:
#include "pmafile.h"
// ZBSLIB includes:
#include "fft.h"
#include "colorpalette.h"
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zgraphfile.h"
#include "ztmpstr.h"
#include "zgltools.h"
#include "zbittexcache.h"
#include "zmathtools.h"
#include "zviewpoint.h"
#include "zrand.h"

extern ZHashTable options;

ZPLUGIN_BEGIN( fretspec );



ZVARB( int, Fretspec_viewMode, 0, 0, 2 );
ZVAR( int, Fretspec_viewTime, 0 );
ZVAR( int, Fretspec_viewFreq, 393 );
ZVAR( int, Fretspec_markStart, 2 );
ZVAR( int, Fretspec_markCount, 256 );
ZVAR( int, Fretspec_radius, 3 );
ZVAR( float, Fretspec_recoveredHertz, 1.00e+000 );
ZVAR( float, Fretspec_colorMTime, 1.00e+000 );
ZVAR( float, Fretspec_colorBTime, 1.00e+000 );

ZVAR( float, Fretspec_colorMFreq, 4.54e-005 );
ZVAR( float, Fretspec_colorBFreq, 5.44e+002 );

ZVAR( float, Fretspec_colorMIntr, 4.54e-005 );
ZVAR( float, Fretspec_colorBIntr, 5.44e+002 );

ZVAR( float, Fretspec_plotMTime, 9.61e-001 );
ZVAR( float, Fretspec_plotBTime, -9.53e+002 );
ZVAR( float, Fretspec_plotMFreq, 6.02e-006 );
ZVAR( float, Fretspec_plotBFreq, -1.17e+002 );
ZVAR( float, Fretspec_plotLinTheoryScale, 1 );
ZVAR( float, Fretspec_plotNonlinTheoryScale, 1 );
ZVAR( float, Fretspec_plotMarkovScale, 1 );


ZVARB( int, Fretspec_markov0, 1, 1, 100000 );
ZVARB( int, Fretspec_markov1, 1, 1, 100000 );

ZVAR( int, Fretspec_freqM0, 157 );
ZVAR( int, Fretspec_freqM1, 157 );


//freqCount = 598
//left peak at 293
//right peak at 157
//10 sec at 60 fps is 600 frames
//shutter change every 10 frames, green and red opposite
//is 6 times per second


int timeCount = 0;
ZBits *timeBits = 0;

int freqCount = 0;
ZBits *freqBits = 0;
	// Stores the freq bits of the CURRENTLY selected time slice

ZBits intermodFilterBits;
	// This is the result of filtering for only the intersting intermod products

FVec2 selecF;
IVec2 selecI;

#define CALIB_MAX (128)
IVec2 calibPts[CALIB_MAX];
FMat3 calibMat;
int calibCount = 0;

void calibSolveMatrix() {
	float lxlx=0.f, lyly=0.f, lxly=0.f, lx=0.f, ly=0.f, lxrx=0.f, lyrx=0.f, rx=0.f, lxry=0.f, lyry=0.f, ry=0.f;
	for( int i=0; i<calibCount; i+=2 ) {
		lxlx += calibPts[i].x * calibPts[i].x;
		lyly += calibPts[i].y * calibPts[i].y;
		lxly += calibPts[i].x * calibPts[i].y;
		lx   += calibPts[i].x;
		ly   += calibPts[i].y;
		lxrx += calibPts[i].x * calibPts[i+1].x;
		lyrx += calibPts[i].y * calibPts[i+1].x;
		rx   += calibPts[i+1].x;
		lxry += calibPts[i].x * calibPts[i+1].y;
		lyry += calibPts[i].y * calibPts[i+1].y;
		ry   += calibPts[i+1].y;
	}

	FMat3 m;
	m.m[0][0] = lxlx;
	m.m[0][1] = lxly;
	m.m[0][2] = lx;

	m.m[1][0] = lxly;
	m.m[1][1] = lyly;
	m.m[1][2] = ly;

	m.m[2][0] = lx;
	m.m[2][1] = ly;
	m.m[2][2] = (float)calibCount / 2.f;

	FVec3 b;
	b.x = lxrx;
	b.y = lyrx;
	b.z = rx;

	m.inverse();
	FVec3 x0 = m.mul( b );

	b.x = lxry;
	b.y = lyry;
	b.z = ry;
	FVec3 x1 = m.mul( b );

	calibMat.m[0][0] = x0.x;
	calibMat.m[1][0] = x0.y;
	calibMat.m[2][0] = x0.z;

	calibMat.m[0][1] = x1.x;
	calibMat.m[1][1] = x1.y;
	calibMat.m[2][1] = x1.z;

	calibMat.m[0][2] = 0.f;
	calibMat.m[1][2] = 0.f;
	calibMat.m[2][2] = 1.f;
}

FVec2 calibMatePos( FVec2 p ) {
	// @TODO: Don't assume 512 w
	if( p.x < 512/2 ) {
		FVec3 left( p.x, p.y, 1.f );
		FVec3 right = calibMat.mul( left );
		if( right.x >= 512/2 && right.x < 512 && right.y >= 0 && right.y < 512 ) {
			return FVec2( right.x, right.y );
		}
	}
	else {
		FMat3 inverse = calibMat;
		inverse.inverse();
		FVec3 right( p.x, p.y, 1.f );
		FVec3 left = inverse.mul( right );
		if( left.x <= 512/2 && left.x > 0 && left.y >= 0 && left.y < 512 ) {
			return FVec2( left.x, left.y );
		}
	}
	return FVec2( -1.f, -1.f );
}

void calibLoad() {
	FILE *f = fopen( ZTmpStr("%s/calibpts.txt",options.getS("Fretspec_dir")), "rt" );
	if( f ) {
		calibCount = 0;
		while( fscanf( f, "%d %d\n", &calibPts[calibCount].x, &calibPts[calibCount].y ) > 0 ) {
			calibCount++;
			assert( calibCount < CALIB_MAX );
		}
		fclose( f );
		calibSolveMatrix();
	}
}

void render() {
	glClearColor( 0.2f, 0.2f, 0.2f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT );

	int x, y, i;

	int w = timeBits[0].w();
	int h = timeBits[0].h();

	ZBits viewBits;
	viewBits.createCommon( w, h, ZBITS_COMMON_RGB_8 );

	// CREATE a psuedo-color display and histogram
	if( Fretspec_viewMode==0 ) {
		Fretspec_viewTime = max( 0, min( timeCount-1, Fretspec_viewTime ) );
		for( y=0; y<h; y++ ) {
			unsigned short *src = timeBits[Fretspec_viewTime].lineU2( y );
			unsigned char *dst = viewBits.lineUchar( y );
			for( x=0; x<w; x++ ) {
				// SCALE the bit depth
				float d = Fretspec_colorMTime * ( (float)*src - Fretspec_colorBTime );
				int di = max( 0, min( 255, (int)d ) );
				*dst++ = colorPaletteBlueToRedUB256[di][0];
				*dst++ = colorPaletteBlueToRedUB256[di][1];
				*dst++ = colorPaletteBlueToRedUB256[di][2];
				src++;
			}
		}
	}
	else if( Fretspec_viewMode==1 ) {
		if( freqBits ) {
			Fretspec_viewFreq = max( 0, min( freqCount-1, Fretspec_viewFreq ) );
			for( y=0; y<h; y++ ) {
				float *src = freqBits[Fretspec_viewFreq].lineFloat( y );
				unsigned char *dst = viewBits.lineUchar( y );
				for( x=0; x<w; x++ ) {
					// SCALE the bit depth
					float d = Fretspec_colorMFreq * (*src - Fretspec_colorBFreq);
					int di = max( 0, min( 255, (int)d ) );
					*dst++ = colorPaletteBlueToRedUB256[di][0];
					*dst++ = colorPaletteBlueToRedUB256[di][1];
					*dst++ = colorPaletteBlueToRedUB256[di][2];
					src++;
				}
			}
		}
	}
	else if( Fretspec_viewMode==2 ) {
		if( intermodFilterBits.bits ) {
			for( y=0; y<h; y++ ) {
				float *src = intermodFilterBits.lineFloat( y );
				unsigned char *dst = viewBits.lineUchar( y );
				for( x=0; x<w; x++ ) {
					// SCALE the bit depth
					float d = Fretspec_colorMIntr * (*src - Fretspec_colorBIntr);
					int di = max( 0, min( 255, (int)d ) );
					*dst++ = colorPaletteBlueToRedUB256[di][0];
					*dst++ = colorPaletteBlueToRedUB256[di][1];
					*dst++ = colorPaletteBlueToRedUB256[di][2];
					src++;
				}
			}
		}
	}

	zglPixelMatrixFirstQuadrant();
	zviewpointSetupView();

	int *tex = zbitTexCacheLoad( &viewBits, 1 );
	zbitTexCacheRender( tex );
	zbitTexCacheFree( tex );

	selecI.x = max( Fretspec_radius, selecI.x );
	selecI.x = min( w - Fretspec_radius, selecI.x );
	selecI.y = max( Fretspec_radius, selecI.y );
	selecI.y = min( h - Fretspec_radius, selecI.y );
	glColor3ub( 0, 0, 0 );
	glBegin( GL_LINE_LOOP );
	for( i=0; i<16; i++ ) {
		glVertex2f( selecI.x + Fretspec_radius*cos64F[i<<2], selecI.y + Fretspec_radius*sin64F[i<<2] );
	}
	glEnd();

	// PLOT the calib mate pos
	FVec2 mouse = zviewpointProjMouseOnZ0Plane();
	int mx = (int)floorf( 0.5f + mouse.x );
	int my = (int)floorf( 0.5f + mouse.y );
	if( mx >= 0 && mx < w && my >= 0 && my < h ) {
		glBegin( GL_LINES );
			glVertex2i( mx-5, my-5 );
			glVertex2i( mx+5, my+5 );
			glVertex2i( mx+5, my-5 );
			glVertex2i( mx-5, my+5 );
		glEnd();

		// PLOT the complement on the right
		FVec2 mate = calibMatePos( mouse );
		if( mate.x >= 0 && mate.y >= 0 ) {
			glBegin( GL_LINES );
				glVertex2f( mate.x-5, mate.y-5 );
				glVertex2f( mate.x+5, mate.y+5 );
				glVertex2f( mate.x+5, mate.y-5 );
				glVertex2f( mate.x-5, mate.y+5 );
			glEnd();
		}
	}

	if( Fretspec_viewMode==0 && timeCount!=0 ) {
		// PLOT the time trace
		glColor3ub( 200, 50, 0 );
		glTranslatef( 0.f, Fretspec_plotBTime, 0.f );
		glScalef( 1.f, Fretspec_plotMTime, 1.f );
		glBegin( GL_LINE_STRIP );
		int r2 = Fretspec_radius * Fretspec_radius;
		for( i=0; i<timeCount; i++ ) {
			int val = 0;
			for( y=-Fretspec_radius; y<=Fretspec_radius; y++ ) {
				for( x=-Fretspec_radius; x<=Fretspec_radius; x++ ) {
					if( x*x + y*y < r2 ) {
						val += timeBits[i].getU2(selecI.x+x,selecI.y+y);
					}
				}
			}
			glVertex2i( i, val );
		}
		glEnd();

		// PLOT the markers
		glBegin( GL_LINES );
			glColor3ub( 0, 0, 0 );
			glVertex2f( (float)Fretspec_viewTime, -10e6f );
			glVertex2f( (float)Fretspec_viewTime, +10e6f );

			glColor3ub( 0, 255, 0 );
			glVertex2f( (float)Fretspec_markStart, -10e6f );
			glVertex2f( (float)Fretspec_markStart, +10e6f );

			glColor3ub( 0, 0, 255 );
			glVertex2f( (float)Fretspec_markStart+(float)Fretspec_markCount, -10e6f );
			glVertex2f( (float)Fretspec_markStart+(float)Fretspec_markCount, +10e6f );
		glEnd();
	}
	else if( Fretspec_viewMode==1 || Fretspec_viewMode==2 ) {
		// PLOT the freq trace
		glColor3ub( 200, 50, 0 );
		glTranslatef( 0.f, Fretspec_plotBFreq, 0.f );
		glScalef( 1.f, Fretspec_plotMFreq, 1.f );
		glBegin( GL_LINE_STRIP );
		int r2 = Fretspec_radius * Fretspec_radius;
		for( i=0; i<freqCount; i++ ) {
			float val = 0.f;
			for( y=-Fretspec_radius; y<=Fretspec_radius; y++ ) {
				for( x=-Fretspec_radius; x<=Fretspec_radius; x++ ) {
					if( x*x + y*y < r2 ) {
						val += freqBits[i].getFloat(selecI.x+x,selecI.y+y);
					}
				}
			}
			glVertex2f( (float)i, val );
		}
		glEnd();

		// PLOT a theoretical results
		for( int j=0; j<2; j++ ) {
			float *tempTime = (float *)alloca( Fretspec_markCount * sizeof(float) );
			for( i=0; i<Fretspec_markCount; i++ ) {
				if( j==0 ) {
					tempTime[i] = (float)( ((i/17) & 1) + ((i/11) & 1) );
				}
				else {
					tempTime[i] = (float)( ((i/17) & 1) | ((i/11) & 1) );
				}
			}
			FFT theoryFFT( tempTime, Fretspec_markCount );
			theoryFFT.fft();
			float *theoryPower = theoryFFT.computePowerSpectrum();
			assert( theoryFFT.getNumFreqCoefs() == freqCount );
			if( j == 0 ) {
				glColor3ub( 200, 255, 0 );
			}
			else {
				glColor3ub( 200, 255, 255 );
			}
			glBegin( GL_LINE_STRIP );
			for( i=0; i<freqCount; i++ ) {
				glVertex2f( (float)i, (j==0 ? Fretspec_plotLinTheoryScale : Fretspec_plotNonlinTheoryScale) * theoryPower[i] );
			}
			glEnd();
		}

		// PLOT the markers
		glBegin( GL_LINES );
			glColor3ub( 0, 0, 0 );
			glVertex2f( (float)Fretspec_viewFreq, -10e6f );
			glVertex2f( (float)Fretspec_viewFreq, +10e6f );
		glEnd();
	}
}

void freqCompute() {
	int x, y, i;

	// CLEAR existing bits
	if( freqBits ) {
		for( i=0; i<freqCount; i++ ) {
			freqBits[i].clear();
		}
		delete [] freqBits;
		freqBits = 0;
	}

	{
		float floatBuffer[4096];
		FFT fft( floatBuffer, Fretspec_markCount );
		freqCount = fft.getNumFreqCoefs();
		freqBits = new ZBits[freqCount];
	}

	int w = timeBits[0].w();
	int h = timeBits[0].h();

	for( i=0; i<freqCount; i++ ) {
		freqBits[i].createCommon( w, h, ZBITS_COMMON_LUM_FLOAT );
	}

	for( y=0; y<h; y++ ) {
		printf( "FFT %0.2f%%       \r", 100.f * (float)y / (float)h );
		for( x=0; x<w; x++ ) {
			// @TODO: This couple be optimized by making a version of FFt which didn't realloc
			float floatBuffer[4096];
			for( i=0; i<Fretspec_markCount; i++ ) {
				floatBuffer[i] = (float)timeBits[i+Fretspec_markStart].lineU2(y)[x];
			}
			FFT fft( floatBuffer, Fretspec_markCount );
			fft.fft();
			float *powerSpec = fft.computePowerSpectrum();
			for( i=0; i<freqCount; i++ ) {
				freqBits[i].setFloat( x, y, powerSpec[i] );
			}
		}
	}

	char *filename = ZTmpStr( "%s/%s.cache", options.getS( "Fretspec_dir" ), options.getS( "Fretspec_filename" ) );
	FILE *cache = fopen( filename, "wb" );
	if( cache ) {
		fwrite( &freqCount, sizeof(freqCount), 1, cache );
		fwrite( &w, sizeof(w), 1, cache );
		fwrite( &h, sizeof(h), 1, cache );
		for( i=0; i<freqCount; i++ ) {
			fwrite( freqBits[i].bits, freqBits[i].lenInBytes(), 1, cache );
		}
		fclose( cache );
	}
}

void load() {
	char *filename = ZTmpStr( "%s/%s", options.getS( "Fretspec_dir" ), options.getS( "Fretspec_filename" ) );

	// LOAD and time domain images
	VidFile *dataFile = new PMAVidFile;
	int ret = dataFile->openRead( filename, 0 );

	int w = dataFile->w;
	int h = dataFile->h;
	int lft = dataFile->getInfoI( "x" );
	int top = dataFile->getInfoI( "y" );

	timeCount = dataFile->numFrames;
	timeBits = new ZBits[timeCount];
	for( int i=0; i<timeCount; i++ ) {
		timeBits[i].createCommon( w, h, ZBITS_COMMON_LUM_16 );
		for( int y=0; y<h; y++ ) {
			unsigned short *src = dataFile->getFrameU2( i, y );
			unsigned short *dst = timeBits[i].lineU2(y);
			memcpy( dst, src, w * 2 );
		}
	}
	dataFile->close();

	// LOAD freq cache if it exists
	filename = ZTmpStr( "%s/%s.cache", options.getS( "Fretspec_dir" ), options.getS( "Fretspec_filename" ) );
	FILE *cache = fopen( filename, "rb" );
	if( cache ) {
		int _w, _h;
		fread( &freqCount, sizeof(freqCount), 1, cache );
		fread( &_w, sizeof(_w), 1, cache );
		fread( &_h, sizeof(_h), 1, cache );
		freqBits = new ZBits[freqCount];
		for( int i=0; i<freqCount; i++ ) {
			printf( "Cache %0.2f%%     \r", 100.f * (float)i / (float)freqCount );
			freqBits[i].createCommon( _w, _h, ZBITS_COMMON_LUM_FLOAT );
			fread( freqBits[i].bits, freqBits[i].lenInBytes(), 1, cache );
		}
		fclose( cache );
	}
}

void intermodFilter() {
	int w = timeBits[0].w();
	int h = timeBits[0].h();
	intermodFilterBits.createCommon( w, h, ZBITS_COMMON_LUM_FLOAT );

	float *spectrum = (float *)alloca( freqCount * sizeof(float) );

	int x, y, i;
	for( y=0; y<h; y++ ) {
		printf( "Intermod %0.2f%%     \r", 100.f * (float)y / (float)h );
		for( x=0; x<w; x++ ) {

			float sum = 0.f;
			for( i=0; i<5; i++ ) {
				sum += freqBits[65+i].getFloat( x, y );
			}

			for( i=0; i<5; i++ ) {
				sum += freqBits[222+i].getFloat( x, y );
			}

			for( i=0; i<freqCount; i++ ) {
				spectrum[i] = freqBits[i].getFloat( x, y );
			}

			float median;
			zmathStatsF( spectrum, 0, freqCount, 0, &median, 0 );

			intermodFilterBits.setFloat( x, y, sum - median*10.f );
		}
	}
}

void startup() {
	colorPaletteInit();
	calibLoad();
	load();
//	intermodFilter();
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
		selecF = zviewpointProjMouseOnZ0Plane();
		selecI = IVec2( (int)selecF.x, (int)selecF.y );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,]) ) {
		if( Fretspec_viewMode==0 ) Fretspec_viewTime+=1; else Fretspec_viewFreq+=1;
	}
	else if( zmsgIs(type,Key) && zmsgIs(which,}) ) {
		if( Fretspec_viewMode==0 ) Fretspec_viewTime+=4; else Fretspec_viewFreq+=4;
	}
	else if( zmsgIs(type,Key) && zmsgIs(which,[) ) {
		if( Fretspec_viewMode==0 ) Fretspec_viewTime-=1; else Fretspec_viewFreq-=1;
	}
	else if( zmsgIs(type,Key) && zmsgIs(which,{) ) {
		if( Fretspec_viewMode==0 ) Fretspec_viewTime-=4; else Fretspec_viewFreq-=4;
	}
	else if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		freqCompute();
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;



/*

// Test code for making sure the FFT smoothing works

ZVAR( int, Fretspec_testLen, 594 );
ZVAR( float, Fretspec_omega0, 7.56e-001 );
ZVAR( float, Fretspec_omega1, 2.36e+000 );
ZVAR( float, Fretspec_recoveredOmega, 1 );
ZVAR( int, Fretspec_testI, 1 );

void render() {
	glClearColor( 0.2f, 0.2f, 0.2f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT );

	// Write a test of the FFT system.  Put known tones into the FFT and
	// run the simulation for non-power of 2 lenght and write a func
	// in FFT which converts an FFT bin into an omega or into a Hertz

	int i;
	float test[1024*64];
	for( i=0; i<Fretspec_testLen; i++ ) {
		test[i] = sinf( Fretspec_omega0 * (float)i ) + 0.5f * sinf( Fretspec_omega1 * (float)i );
	}

	FFT fft( test, Fretspec_testLen );
	fft.fft();

	zglPixelMatrixFirstQuadrant();
	zviewpointSetupView();

	glColor3ub( 0, 255, 0 );
	glBegin( GL_LINE_STRIP );
		for( i=0; i<Fretspec_testLen; i++ ) {
			glVertex2f( (float)i, test[i] );
		}
	glEnd();

	glColor3ub( 255, 0, 0 );
	glBegin( GL_LINE_STRIP );
		for( i=0; i<fft.getNumCoefs(); i++ ) {
			glVertex2f( (float)i, fft.getPower(i) );
		}
	glEnd();

	glBegin( GL_LINES );
		glVertex2f( (float)Fretspec_testI, -10e5f );
		glVertex2f( (float)Fretspec_testI, +10e5f );
	glEnd();

	// SUppose that there are 60 samples per second.
	// Then we want to know the frequency of a sample in Hertz

	Fretspec_recoveredOmega = fft.getOmegaFromPow2Offset( Fretspec_testI );
	Fretspec_recoveredHertz = fft.getHertzFromPow2Offset( Fretspec_testI, 60.f );
}
*/
