// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "memory.h"
#include "string.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zbits.h"
#include "zgltools.h"
#include "zviewpoint.h"
#include "ztmpstr.h"
#include "zgraphfile.h"

ZPLUGIN_BEGIN( diffus );

ZVAR( int, Diffus_updatesPerFrame, 1 );

ZVAR( double, Diffus_rate, 0.01 );
ZVAR( double, Diffus_vizScale, 1 );
ZVAR( double, Diffus_puke, 1 );

ZVAR( double, Diffus_pop0, 0 );
ZVAR( double, Diffus_pop1, 0 );

ZVAR( double, Diffus_popGrow0, 0.1 );
ZVAR( double, Diffus_popGrow1, 0.05 );


const int X_MAX = 64;
const int Y_MAX = 64;
const int TYPES = 2;
double grid[2][TYPES][Y_MAX][X_MAX];

double population[2];

unsigned char wall[Y_MAX][X_MAX];

int curr = 0;
int last = 1;

int updateCount = 0;

IVec2 colonyPos[2];

void update() {
	// DIFFUSION
	for( int t=0; t<TYPES; t++ ) {
		for( int y=1; y<Y_MAX-1; y++ ) {

			for( int x=1; x<X_MAX-1; x++ ) {

				if( ! wall[y][x] ) {

					double diff = 0.0;
					double count = 0.0;
				
					if( ! wall[y][x-1] ) {
						diff += grid[last][t][y][x-1];
						count += 1.0;
					}
				
					if( ! wall[y][x+1] ) {
						diff += grid[last][t][y][x+1];
						count += 1.0;
					}
				
					if( ! wall[y-1][x] ) {
						diff += grid[last][t][y-1][x];
						count += 1.0;
					}
				
					if( ! wall[y+1][x] ) {
						diff += grid[last][t][y+1][x];
						count += 1.0;
					}

					grid[curr][t][y][x] =
						+ grid[last][t][y][x]
						+ Diffus_rate * ( diff - count * grid[last][t][y][x] )
					;
				}
			}
		}
	}

	// PUKE
	grid[curr][0][colonyPos[0].y][colonyPos[0].x] += Diffus_puke * population[0];
	grid[curr][1][colonyPos[1].y][colonyPos[1].x] += Diffus_puke * population[1];

	// EAT
	double local0 = grid[curr][1][colonyPos[0].y][colonyPos[0].x];
	double local1 = grid[curr][0][colonyPos[1].y][colonyPos[1].y];
	grid[curr][1][colonyPos[0].y][colonyPos[0].x] -= local0;
	grid[curr][0][colonyPos[1].y][colonyPos[1].x] -= local1;

	// GROW
	population[0] += Diffus_popGrow0 * local0;
	population[1] += Diffus_popGrow1 * local1;
	
	updateCount++;

	// HACK display	
	Diffus_pop0 = population[0];
	Diffus_pop1 = population[1];

	// SWAP	
	last = !last;
	curr = !curr;
}

ZBits bits;
int tex;

void render() {
	for( int i=0; i<Diffus_updatesPerFrame; i++ ) {
		update();
	}
	
	zviewpointSetupView();
	
	// RENDER to texture
	for( int t=0; t<TYPES; t++ ) {
		for( int y=0; y<Y_MAX; y++ ) {
			double *src = grid[curr][t][y];
			unsigned char *dst = bits.lineU1( y ) + t;
			for( int x=0; x<X_MAX; x++ ) {
				double c = max( 0.0, min( 255.0, *src * Diffus_vizScale ) );
				*dst = (unsigned char)c;
				dst += 4;
				src++;
			}
		}
	}

	for( int y=0; y<Y_MAX; y++ ) {
		unsigned char *dst = bits.lineU1( y ) + 2;
		unsigned char *w = wall[y];
		for( int x=0; x<X_MAX; x++ ) {
			*dst = *w;
			dst += 4;
			w++;
		}
	}

	zglLoadTextureFromZBits( &bits, tex );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, tex );
	zglTexRect( 0.f, 0.f, 1.f, 1.f );
}

void reset() {
	curr = 0;
	last = 1;
	memset( grid, 0, sizeof(grid) );

	population[0] = 1.0;
	population[1] = 1.0;

	memset( wall, 0, sizeof(wall) );	
	
	colonyPos[0] = IVec2( 1*X_MAX/4, Y_MAX/2 );
	colonyPos[1] = IVec2( 3*X_MAX/4, Y_MAX/2 );


	ZBits wallBits;	
	int a = zGraphFileLoad( "/zlab/plug_public/_diffus/wall1.png", &wallBits );
	for( int y=0; y<Y_MAX; y++ ) {
		for( int x=0; x<X_MAX; x++ ) {
			wall[y][x] = wallBits.getChar( x, y ) ? 255 : 0;
		}
	}
	
/*
	for( int y=0; y<Y_MAX; y++ ) {
		for( int x=0; x<X_MAX; x++ ) {
			//if( y < Y_MAX/2-2 || y > Y_MAX/2+2 ) {
			//	wall[y][x] = 255;
			//}
			
			if( y > 1*Y_MAX/4 && y < 3*Y_MAX/4 && x == X_MAX/2 ) {
				wall[y][x] = 255;
			}
		}
	}
*/
}

void tests() {
	
	for( int distance = 30; distance < 60; distance+=5 ) {
		reset();

		FILE *file = fopen( ZTmpStr("growth-%d.txt",distance), "wb" );

		colonyPos[0] = IVec2( X_MAX/2-distance/2, Y_MAX/2 );
		colonyPos[1] = IVec2( X_MAX/2+distance/2, Y_MAX/2 );

		updateCount = 0;
		
		while( updateCount < 80000 ) {
			update();
			if( !(updateCount % 1000) ) {
				fprintf( file, "%lf %lf\n", population[0], population[1] );
			}
		}
		
		fclose( file );
	}
}


void startup() {
	tex = zglGenTexture();
	bits.createCommon( X_MAX, Y_MAX, ZBITS_COMMON_RGBA_8 );
	bits.fill( 0 );
	reset();

//	tests();
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		reset();
	}

}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

