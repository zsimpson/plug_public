// OPERATING SYSTEM specific includes:
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
#include "zrand.h"
#include "zvec.h"
#include "zviewpoint.h"
#include "zgltools.h"

ZPLUGIN_BEGIN( smoke );

ZVAR( double, Smoke_size, 1 );

struct Cell {
	double p;
	DVec3 v;
	DVec3 dvdt;
};

#define DIMX 16
#define DIMY 32
#define DIMZ 16
Cell cells[DIMX][DIMY][DIMZ];

void step() {
	int x, y, z;

	for( x=0; x<DIMX; x++ ) {
		for( y=0; y<DIMY; y++ ) {
			for( z=0; z<DIMZ; z++ ) {
				Cell *c = &cells[x][y][z];
				c->dvdt = DVec3::Origin;				
			}
		}
	}	

	for( x=1; x<DIMX-1; x++ ) {
		for( y=1; y<DIMY-1; y++ ) {
			for( z=1; z<DIMZ-1; z++ ) {
				Cell *xm1 = &cells[x-1][y][z];
				Cell *xp1 = &cells[x+1][y][z];
				Cell *ym1 = &cells[x][y-1][z];
				Cell *yp1 = &cells[x][y+1][z];
				Cell *zm1 = &cells[x][y][z-1];
				Cell *zp1 = &cells[x][y][z+1];
				Cell *c = &cells[x][y][z];
				c->dvdt.x += c->p - xm1->p;
				c->dvdt.x += xp1->p - c->p;
				c->dvdt.y += c->p - ym1->p;
				c->dvdt.y += yp1->p - c->p;
				c->dvdt.z += c->p - zm1->p;
				c->dvdt.z += zp1->p - c->p;
			}
		}
	}	

	for( x=1; x<DIMX-1; x++ ) {
		for( y=1; y<DIMY-1; y++ ) {
			for( z=1; z<DIMZ-1; z++ ) {
				Cell *c = &cells[x][y][z];
				c->v.add( c->dvdt );
			}
		}
	}	

}


void render() {
//	step();

	glScalef( 0.05f, 0.05f, 0.05f );
	glTranslatef( (float)-DIMX, (float)-DIMY, 0.f );
	zviewpointSetupView();

/*
	glBegin( GL_LINES );
	for( int x=0; x<DIMX; x++ ) {
		for( int y=0; y<DIMY; y++ ) {
			for( int z=0; z<DIMZ; z++ ) {
				Cell *c = &cells[x][y][z];
				glColor3ub( 0, 0, 0 );
				glVertex3d( x+0.5, y+0.5, z+0.5 );
				glColor3ub( 255, 255, 255 );
				glVertex3d( x+0.5 + c->v.x, y+0.5 + c->v.y, z+0.5 + c->v.z );
			}
		}
	}
	glEnd();	
*/

	glBegin( GL_QUADS );
	for( int x=0; x<DIMX; x++ ) {
		for( int y=0; y<DIMY; y++ ) {
			for( int z=0; z<DIMZ; z++ ) {
				Cell *c = &cells[x][y][z];
				double p = Smoke_size * c->p;
				glVertex3d( x+0.5 - p, y+0.5 - p, z );
				glVertex3d( x+0.5 + p, y+0.5 - p, z );
				glVertex3d( x+0.5 + p, y+0.5 + p, z );
				glVertex3d( x+0.5 - p, y+0.5 + p, z );
			}
		}
	}
	glEnd();	
	
	zglDrawStandardAxis( 10 );


}

void startup() {
	for( int x=0; x<DIMX; x++ ) {
		for( int y=0; y<DIMY; y++ ) {
			for( int z=0; z<DIMZ; z++ ) {
				cells[x][y][z].v = DVec3::Origin;
				cells[x][y][z].p = x < 2 ? 1.0 : 0.1;
			}
		}
	}
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

