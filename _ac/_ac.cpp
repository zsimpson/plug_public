// test
// OPERATING SYSTEM specific includes :
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "ztmpstr.h"
#include "zviewpoint.h"
#include "zmat_matlab.h"
#include "zrand.h"
#include "ztime.h"

ZPLUGIN_BEGIN( ac );

#define DIM 128

double grid[2][DIM][DIM];
int read = 0;
int write = 1;

void render() {
	zviewpointSetupView();

	for( int y=0; y<DIM; y++ ) {	
		for( int x=0; x<DIM; x++ ) {	

			double a = grid[read][ (y - 1 + DIM) % DIM ]
			double b = grid[read][  y                  ]
			double c = grid[read][ (y + 1      ) % DIM ]

			delta[x] = dt * diffusion * (a + c - b - b);


			grid[write][y][x] = grid[read][y][x]
	
	
}

void startup() {
}

void shutdown() {
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

