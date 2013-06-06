// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "assert.h"
#include "stdlib.h"
#include "math.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zvec.h"
#include "zrand.h"
#include "zgltools.h"

ZVAR( int, Che3_updates, 10000 );
ZVAR( float, Che3_sumX, 0.001 );
ZVAR( float, Che3_sumY, 0.001 );

ZPLUGIN_BEGIN( che3 );

// 1D version of chemotaxis

#define DRAW_LIST_MAX (4096*10)

int drawListCount = 0;
FVec3 drawListVert[DRAW_LIST_MAX];
int drawListColr[DRAW_LIST_MAX];

#define DRAW_LIST_LINE_MAX (4096*4)

int drawListLineCount = 0;
SVec3 drawListLineVert[DRAW_LIST_LINE_MAX];

void drawListRender() {
	glDisable( GL_DEPTH_TEST );

	glPointSize( 3.f );
	
	glVertexPointer( 3, GL_FLOAT, 0, drawListVert );
	glEnableClientState( GL_VERTEX_ARRAY );

	glColorPointer( 4, GL_UNSIGNED_BYTE, 0, drawListColr );
	glEnableClientState( GL_COLOR_ARRAY );

 	glDrawArrays( GL_POINTS, 0, drawListCount );
}

int partColorPalette[] = {
	0xFF0000FF,
	0xFF00FF00,
	0xFFFF0000,
	0xFF00FFFF,
};

struct Part {
	int type;
	float x;
	int isMobile;
};

int partCount = 0;
Part parts[1000];


double signal = 0.0;
int updates = 0;

double outputSum = 0.0;
#define OUTPUT_SUM_SAMPLES_COUNT (50000000)
double *outputSumSamples = 0;
int outputSumSamplesCount = 0;

void update() {
	drawListCount = 0;
	drawListLineCount = 0;
	
	updates++;
	signal = sin( (double)updates / 50000.0 ) / 2.0 + 0.5; 

	for( int i=0; i<partCount; i++ ) {
		Part &p = parts[i];
		if( p.isMobile ) {
			p.x += zrandGaussianF() * 0.01f;

			if( p.type == 1 ) {
				if( zrandD(0.0,1.0) < 0.001 ) {
					p.type = 0;
				}
			}

			if( p.x < 0.f ) {
				p.x = -p.x;
				
				// HIT the left end
				if( p.type == 0 ) {
					if( zrandD(0.0,1.0) < signal ) {
						p.type = 1;
					}
				}
			}

			if( p.x >= 1.f ) {
				p.x = 2.f - 0.00001f - p.x;
				if( p.type == 1 ) {
					outputSum += 1.0;
				}
			}
			
			assert( p.x >= 0.f && p.x < 1.f );
		}

		FVec3 pos( parts[i].x, 0.f, 0.f );
		drawListVert[drawListCount] = pos;
		drawListColr[drawListCount] = partColorPalette[ parts[i].type ];
		drawListCount++;
	}

	assert( drawListCount < DRAW_LIST_MAX );
	assert( drawListLineCount < DRAW_LIST_LINE_MAX );

	if( outputSumSamplesCount < OUTPUT_SUM_SAMPLES_COUNT ) {
		outputSumSamples[outputSumSamplesCount++] = outputSum;
	}
	else {
		FILE *f = fopen( "/transfer/che.txt", "wb" );
		for( int i=0; i<outputSumSamplesCount; i++ ) {
			fprintf( f, "%lf\n", outputSumSamples[i] );
		}
		fclose( f );
		exit( 0 );
	}

}

void render() {
	for( int i=0; i<Che3_updates; i++ ) {
		update();
	}
	printf( "%f\n", 100 * outputSumSamplesCount / OUTPUT_SUM_SAMPLES_COUNT );
	
	glClearColor( 0.f, 0.f, 0.f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	zglMatrixFirstQuadrant();
	glTranslatef( 0.05f, +0.1f, 0.f );
	glScalef( 0.9f, 0.9f, 1.f );

	// HISTOGRAM
	int histogram[50][2] = {0,};
	for( int i=0; i<partCount; i++ ) {
		int bin = min( 49, (int)(parts[i].x*50.f) );
		histogram[ bin ][ parts[i].type ]++;
	}
	
	glLineWidth( 2.f );
	glBegin( GL_LINES );
	for( int t=0; t<2; t++ ) {
		glColor3ubv( (const GLubyte *)&partColorPalette[t] );
		for( int i=0; i<50; i++ ) {
			glVertex2f( (float)i/50.f + 0.01f*t, 0.f );
			glVertex2f( (float)i/50.f + 0.01f*t, (float)histogram[i][t] / 300.f );
		}
	}	

	glVertex2f( -0.05f, 0.f );
	glVertex2f( -0.05f, (float)signal / 2.f );
	
	glEnd();	

	glBegin( GL_LINE_STRIP );
	for( int i=0; i<outputSumSamplesCount; i++ ) {
		glVertex2d( i*Che3_sumX, outputSumSamples[i]*Che3_sumY );
	}
	glEnd();

	drawListRender();

}

void startup() {

	outputSumSamples = new double[ OUTPUT_SUM_SAMPLES_COUNT ];

	for( int i=0; i<1000; i++ ) {
		parts[partCount].type = 0;
		parts[partCount].x = zrandF(0.f,1.f);
		parts[partCount].isMobile = 1;
		partCount++;
	}
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

