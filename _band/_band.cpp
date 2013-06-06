// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "assert.h"
#include "memory.h"
#include "math.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "float.h"
// MODULE includes:
// ZBSLIB includes:
#include "zmathtools.h"
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zvec.h"
#include "zrand.h"
#include "zviewpoint.h"
#include "zchangemon.h"
#include "fft.h"
#include "zprof.h"
#include "ztmpstr.h"

ZPLUGIN_BEGIN( band );

ZVAR( int, Band_dimX, 32 );
ZVAR( int, Band_dimY, 32 );
ZVAR( int, Band_a, 10 );
ZVAR( int, Band_f, 10 );
ZVAR( int, Band_y, 100 );
ZVAR( int, Band_z, 10 );
ZVARB( int, Band_useSinSignal, 1, 0, 1 );
ZVARB( float, Band_signal, 1.11e-001, 0.00e+000, 1.00e+000 );
ZVARB( float, Band_signalTop, 1.79e-001, 0.00e+000, 1.00e+000 );
ZVAR( float, Band_signalOmega, 9.70e-005 );
ZVAR( int, Band_updates, 1573 );
ZVAR( float, Band_plotScaleX, 3.85e-003 );
ZVAR( float, Band_plotScaleY0, 5.54e+001 );
ZVAR( float, Band_plotScaleY1, 1.02e1 );
ZVAR( float, Band_plotFFTScaleX, 7.58e-002 );
ZVAR( float, Band_plotFFTScaleY, 1.96e+000 );
ZVAR( float, Band_histogramScale, 1 );
ZVARB( float, Band_cheZReactProb, 3.75e-002, 0.00e+000, 1.00e+000 );
ZVAR( int, Band_plotWindowRadius, 1681 );
ZVAR( float, Band_pointSize, 3.00e+000 );
ZVARB( int, Band_cheZDiffuses, 1, 0, 1 );
ZVAR( float, Band_avgZHits, 0 );


struct Part {
	int type;
	IVec2 pos;
	unsigned char color[3];
	int diffuses;
};
const int PART_MAX = 4096;
int partCount = 0;
Part part[PART_MAX];

int DIMX = 0;
int DIMY = 0;
Part **partGrid;

int simulationFrameNumber = 0;

void partReset( int dimx, int dimy ) {
	partCount = 0;

	if( partGrid ) {
		free( partGrid );
	}
	partGrid = (Part **)malloc( sizeof(Part*) * dimx * dimy );
	DIMX = dimx;
	DIMY = dimy;
	memset( partGrid, 0, sizeof(Part *) * DIMY * DIMX );
}

Part *partAddAtPos( IVec2 pos ) {
	assert( pos.x >= 0 && pos.x < DIMX );
	assert( pos.y >= 0 && pos.y < DIMY );
	if( partCount < PART_MAX-1 ) {
		part[partCount].type = 0;
		part[partCount].color[0] = 0;
		part[partCount].color[1] = 0;
		part[partCount].color[2] = 0;

		if( ! partGrid[pos.y*DIMX + pos.x] ) {
			partGrid[pos.y*DIMX + pos.x] = &part[partCount];
			part[partCount].pos = pos;
			partCount++;
			return &part[partCount-1];
		}
		else {
			return 0;
		}
	}
	else {
		return 0;
	}
}

Part *partAdd() {
	int i;
	if( partCount < PART_MAX-1 ) {
		part[partCount].type = 0;
		part[partCount].color[0] = 0;
		part[partCount].color[1] = 0;
		part[partCount].color[2] = 0;

		// FIND a free place
		for( i=0; i<1000000; i++ ) {
			IVec2 p( (int)zrandI(0,DIMX), (int)zrandI(0,DIMY) );
			if( ! partGrid[p.y*DIMX + p.x] ) {
				part[partCount].pos = p;
				break;
			}
		}
		assert( i < 1000000 );
		partCount++;
		return &part[partCount-1];
	}
	else {
		return 0;
	}
}

Part *partAt( IVec2 p ) {
	return partGrid[p.y*DIMX + p.x];
}

int partInBounds( IVec2 p ) {
	return( p.x >= 0 && p.x < DIMX && p.y >=0 && p.y < DIMY );
}

void partMove( Part *p, IVec2 newPos ) {
	partGrid[p->pos.y*DIMX + p->pos.x] = 0;
	partGrid[newPos.y*DIMX + newPos.x] = p;
	p->pos = newPos;
}

typedef int (*PartReactCallback)( Part *p0, Part *p1 );
PartReactCallback partReactCallback;

void partUpdate() {
	simulationFrameNumber++;

	// DIFFUSE (@TODO: replace w more realisitic simulation, see CheY paper)
	for( int i=0; i<partCount; i++ ) {
		IVec2 p = part[i].pos;
		static IVec2 dirs[4] = { IVec2(1,0), IVec2(-1,0), IVec2(0,1), IVec2(0,-1) };
		p.add( dirs[zrandI(0,4)] );
		if( partInBounds(p) ) {
			Part *react = partAt(p);
			if( react ) {
				int hit = (*partReactCallback)( react, &part[i] );
				if( !hit ) {
					(*partReactCallback)( &part[i], react );
				}
			}
			else if( part[i].diffuses ) {
				partMove( &part[i], p );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

enum {
	TYPE_CHEY  =1,
	TYPE_CHEYP,
	TYPE_CHEA,
	TYPE_CHEZ,
	TYPE_FLIM,
	TYPE_COUNT,
};

void cheySetColor( Part *p ) {
	static unsigned char cheyColorFromType[TYPE_COUNT][3] = {
		{ 255, 255, 255 },
		{ 0, 128, 0 },
		{ 128, 255, 128 },
		{ 0, 255, 0 },
		{ 255, 0, 0 },
		{ 0, 0, 255 },
	};

	memcpy( p->color, cheyColorFromType[p->type], sizeof(p->color) );
}

struct History {
	int size;
	float *data;
	float *avgData;
	int write;

	int avgSize;
	float avgSum;

	History( int _size ) {
		size = _size;
		data = new float[size];
		avgData = new float[size];
		memset( data, 0, size * sizeof(float) );
		memset( avgData, 0, size * sizeof(float) );
		write = 0;
	}

	void add( float x ) {
		data[write] = x;

		avgSum -= data[ (write - avgSize + size) % size ];
		avgSum += x;
		avgData[write] = avgSum / ((float)avgSize + FLT_EPSILON);

		write = ( write + 1 ) % size;
	}

	void setAvgWindow( int _avgSize ) {
		if( avgSize == _avgSize ) return;
		avgSize = _avgSize;
		if( avgSize > size-1 ) {
			avgSize = size-1;
		}

		float sum = 0.f;
		for( int i=1; i<=avgSize; i++ ) {
			sum += data[ (write - i + size) % size ];
		}

		avgSum = sum;
	}

	float getVal( int i ) {
		return data[ (write - i - 1 + size) % size ];
	}

	float getAvg( int i ) {
		return avgData[ (write - i - 1 + size) % size ];
	}

};

History outputHistory( 4096*4 );
History inputHistory( 4096*4 );
float signal = 0.f;
int outputHits = 0;
int zReactHits = 0;
int framesSinceLastReset = 0;

int cheReactionCallback( Part *p0, Part *p1 ) {
	if( p0->type == TYPE_CHEY && p1->type == TYPE_CHEA ) {
		if( zrandF(0.f,1.f) <= signal ) {
			p0->type = TYPE_CHEYP;
			cheySetColor( p0 );
			p0->diffuses = 1;
			return 1;
		}
	}
	else if( p0->type == TYPE_CHEZ && p1->type == TYPE_CHEYP ) {
		if( zrandF(0.f,1.f) < Band_cheZReactProb ) {
			zReactHits++;
			p1->type = TYPE_CHEY;
			cheySetColor( p1 );
			return 1;
		}
	}
	else if( p0->type == TYPE_FLIM ) {
		if( p1->type == TYPE_CHEYP ) {
			outputHits++;
			return 1;
		}
//		else if( p1->type == TYPE_CHEY ) {
//			outputHits--;
//			return 1;
//		}
	}
	return 0;
}

float oldOmega = 0.f;

int avgBufSize = 0;
int avgBufCount = 0;
float *avgBuf;
const int cyclesToSample = 10;

const int fftBufSize = 4096*2;
int fftBufCount = 0;
float fftBuf[fftBufSize];

int cumPowerSpectrumCount = 0;
float cumPowerSpectrum[fftBufSize/2] = {0,};
float cumPowerSpectrumLog[fftBufSize/2] = {0,};

int spatialHistogram[2048] = { 0, };
int spatialHistogramCount = 0;

void cheReactionReset() {
	partReset( Band_dimX, Band_dimY );

	int i;
	Part *p;

	partReactCallback = cheReactionCallback;

	for( i=0; i<Band_a; i++ ) {
		p = partAddAtPos( IVec2( 0, Band_dimY*i/Band_a ) );
		assert( p );
		p->type = TYPE_CHEA;
		p->diffuses = 0;
		cheySetColor( p );
	}

	for( i=0; i<Band_f; i++ ) {
		p = partAddAtPos( IVec2( DIMX-1, Band_dimY*i/Band_f ) );
		assert( p );
		p->type = TYPE_FLIM;
		p->diffuses = 0;
		cheySetColor( p );
	}

	for( i=0; i<Band_y; i++ ) {
		Part *p = partAdd();
		if( p ) {
			p->type = TYPE_CHEY;
			p->diffuses = 1;
			cheySetColor( p );
		}
	}

	for( i=0; i<Band_z/2; i++ ) {
		//Part *p = partAdd();
		Part *p = partAddAtPos( IVec2( 1, Band_dimY*i/(Band_z/2) ) );
		if( p ) {
			p->type = TYPE_CHEZ;
			p->diffuses = Band_cheZDiffuses;
			cheySetColor( p );
		}
	}

//	for( i=0; i<Band_z/2; i++ ) {
//		//Part *p = partAdd();
//		Part *p = partAddAtPos( IVec2( Band_dimX-2, Band_dimY*i/(Band_z/2) ) );
//		if( p ) {
//			p->type = TYPE_CHEZ;
//			p->diffuses = Band_cheZDiffuses;
//			cheySetColor( p );
//		}
//	}

	// ALLOCATE fft buffers, etc
	avgBufSize = int( (float)cyclesToSample * PI2F / (float)Band_signalOmega / (float)fftBufSize );
	assert( avgBufSize >= 1 );
		// This is the number of samples that have to be averaged for each position in the box
	if( avgBuf ) {
		free( avgBuf );
	}
	avgBuf = (float *)malloc( sizeof(float) * avgBufSize );
	memset( avgBuf, 0, sizeof(float) * avgBufSize );
	avgBufCount = 0;
	oldOmega = Band_signalOmega;
	fftBufCount = 0;
	memset( cumPowerSpectrum, 0, sizeof(float) * (fftBufSize/2) );
	memset( cumPowerSpectrumLog, 0, sizeof(float) * (fftBufSize/2) );
	memset( spatialHistogram, 0, sizeof(spatialHistogram) );
	spatialHistogramCount = 0;
	cumPowerSpectrumCount = 0;
	zReactHits = 0;
	framesSinceLastReset = 0;
}

void cheReactionUpdate() {
	framesSinceLastReset++;

	int i;

	if( Band_signalOmega != oldOmega ) {
		cheReactionReset();
	}

	avgBuf[avgBufCount++] = (float)outputHits;
	if( avgBufCount == avgBufSize ) {
		float sum = 0.f;
		for( i=0; i<avgBufCount; i++ ) {
			sum += avgBuf[i];
		}
		fftBuf[fftBufCount] = sum / (float)avgBufCount;
		fftBufCount++;
		avgBufCount = 0;
		if( fftBufCount == fftBufSize ) {
			FFT fft( fftBuf, fftBufSize );
			fft.fft();
			float *p = fft.computePowerSpectrum();
			for( i=0; i<fftBufCount/2-1; i++ ) {
				cumPowerSpectrum[i] += p[i];
				cumPowerSpectrumLog[i] = logf( 1.f + cumPowerSpectrum[i] );
			}
			fftBufCount = 0;
			cumPowerSpectrumCount++;
			FILE *file = fopen( ZTmpStr("/transfer/fft/fft-diff-%d.txt",cumPowerSpectrumCount), "wt" );
			for( i=1; i<100; i++ ) {
				fprintf( file, "%f\n", cumPowerSpectrum[i] / (float)cumPowerSpectrumCount );
			}
			fclose( file );
		}
	}

	if( !(simulationFrameNumber % 100 ) ) {
		spatialHistogramCount++;
		for( int x=0; x<DIMX; x++ ) {
			for( int y=0; y<DIMY; y++ ) {
				Part *p = partAt( IVec2(x,y) );
				if( p && p->type == TYPE_CHEYP ) {
					spatialHistogram[x]++;
				}
			}
		}
	}

	outputHistory.setAvgWindow( Band_plotWindowRadius );
	outputHistory.add( (float)outputHits );

	// COMPUTE input signal
	signal = Band_signal;
	if( Band_useSinSignal ) {
		signal = (float)( Band_signalTop * 0.5*( 1.0+sin( Band_signalOmega * (double)simulationFrameNumber ) ) );
	}
	inputHistory.add( signal );

	// SETUP for next computation
	outputHits = 0;
}

//////////////////////////////////////////////////////////////////////////

ZChangeMon changeMon;

void render() {
	int i;

	if( changeMon.hasChanged() ) {
		cheReactionReset();
	}

	glScalef( 1.f/DIMX, 1.f/DIMX, 1.f );
	glTranslatef( -DIMX/2.f, -DIMY/2.f, 0.f );
	zviewpointSetupView();

	zprofBeg( band_update );
	for( i=0; i<Band_updates; i++ ) {
		partUpdate();
		cheReactionUpdate();
	}
	zprofEnd();

	Band_avgZHits = (float)zReactHits / (float)framesSinceLastReset;

	zprofBeg( band_render );
	glPointSize( Band_pointSize );
	glVertexPointer( 2, GL_INT, sizeof(Part), &part[0].pos );
	glEnableClientState( GL_VERTEX_ARRAY );
	glColorPointer( 3, GL_UNSIGNED_BYTE, sizeof(Part), &part[0].color );
	glEnableClientState( GL_COLOR_ARRAY );
 	glDrawArrays( GL_POINTS, 0, partCount );

	glColor3ub( 100, 100, 100 );
	glBegin( GL_LINES );
	float cntf = (float)spatialHistogramCount;
	for( int x=0; x<DIMX; x++ ) {
		glVertex2f( (float)x, 0.f );
		glVertex2f( (float)x, Band_histogramScale * (float)spatialHistogram[x] / cntf );
	}
	glEnd();

	glTranslatef( 0.f, -22, 0.f );
	glBegin( GL_LINES );
	glColor3ub( 255, 255, 255 );
	glVertex2f( 0.f, 0.f );
	glVertex2f( Band_plotScaleX * (float)inputHistory.size, 0.f );

	glColor3ub( 50, 255, 50 );
	glVertex2f( 0.f, Band_plotScaleY0 * Band_signalTop );
	glVertex2f( Band_plotScaleX * (float)inputHistory.size, Band_plotScaleY0 * Band_signalTop );
	glEnd();

	glColor3ub( 100, 255, 100 );
	glBegin( GL_LINE_STRIP );
	for( i=0; i<inputHistory.size-1; i++ ) {
		glVertex2f( Band_plotScaleX * (float)i, Band_plotScaleY0 * inputHistory.getVal(i) );
	}
	glEnd();

	glColor3ub( 100, 100, 255 );
	glBegin( GL_LINE_STRIP );
	for( i=0; i<inputHistory.size-1; i++ ) {
		glVertex2f( Band_plotScaleX * (float)i, Band_plotScaleY1 * outputHistory.getAvg(i) );
	}
	glEnd();

	// DRAW the power spectrum
	glTranslatef( 0.f, -22, 0.f );
	glBegin( GL_LINES );
	glColor3ub( 255, 255, 255 );
	glVertex2f( 0.f, 0.f );
	glVertex2f( Band_plotFFTScaleX * (float)fftBufSize/2.f, 0.f );
	glEnd();
	glColor3ub( 200, 200, 50 );
	glBegin( GL_LINE_STRIP );
	for( i=1; i<fftBufSize/2-1; i++ ) {
		glVertex2f( Band_plotFFTScaleX * (float)i, Band_plotFFTScaleY * cumPowerSpectrumLog[i] );
	}
	glEnd();
	zprofEnd();
}

void startup() {
	cheReactionReset();

	changeMon.add( &Band_a, sizeof(Band_a) );
	changeMon.add( &Band_f, sizeof(Band_f) );
	changeMon.add( &Band_y, sizeof(Band_y) );
	changeMon.add( &Band_z, sizeof(Band_z) );
	changeMon.add( &Band_dimX, sizeof(Band_dimX) );
	changeMon.add( &Band_dimY, sizeof(Band_dimY) );
	changeMon.add( &Band_cheZDiffuses, sizeof(Band_cheZDiffuses) );
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		cheReactionReset();
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

