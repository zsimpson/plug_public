// OPERATING SYSTEM specific includes :
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "memory.h"
#include "string.h"
#include "math.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "ztmpstr.h"
#include "zviewpoint.h"
#include "zrand.h"
#include "ztime.h"
#include "zgltools.h"

ZPLUGIN_BEGIN( cellspring );

ZVAR( float, Cellspring_natLen, 0.98f );
ZVAR( float, Cellspring_force, 0.1f );
ZVAR( float, Cellspring_dt, 0.3f );
ZVAR( float, Cellspring_plotLen, 3.5f );
ZVAR( float, Cellspring_reproduceProb, 0.0001f );
ZVAR( float, Cellspring_k0, 2.32e-2f );
ZVAR( float, Cellspring_k1, 2.32e-2f );
ZVAR( float, Cellspring_k2, 2.16e-2f );
ZVAR( float, Cellspring_k3, 1.8e-2f );
ZVAR( float, Cellspring_k4, 0.001f );
ZVAR( float, Cellspring_k5, 0.001f );
ZVAR( float, Cellspring_freq, 0.01f );
ZVAR( float, Cellspring_radius, 0.1f );


#define MAX_NEIGHBORS (16)
#define MAX_CELLS (1024)
GLUquadricObj *sphere = 0;

struct Cell {
	int alive;
	FVec3 pos;
	FVec3 force;
	float s;
	float t;
	Cell *neighbors[MAX_NEIGHBORS];
	
	void addNeighbor( Cell &c ) {
		int free = -1;
		int found = 0;
		for( int i=0; i<MAX_NEIGHBORS; i++ ) {
			if( ! neighbors[i] ) {
				free = i;
			}
			else if( neighbors[i] == &c ) {
				found = 1;
			}
		}
		if( !found && free >= 0 ) {
			neighbors[free] = &c;
		}
	}

	void removeNeighbor( Cell &c ) {
		for( int i=0; i<MAX_NEIGHBORS; i++ ) {
			if( neighbors[i] == &c ) {
				neighbors[i] = 0;
				break;
			}
		}
	}
	
	void clearNeighbors() {
		memset( neighbors, 0, sizeof(neighbors) );
	}
};

Cell cells[MAX_CELLS];
float cellTime = 0.f;

void reproduce( Cell &c ) {
	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &ci = cells[i];
		if( ! ci.alive ) { 
			ci.pos = c.pos;
			//FVec3 d( zrandF(), zrandF(), zrandF() );
			FVec3 d( zrandF(), zrandF(), 0.f );
			d.mul( 0.1f );
			ci.pos.add( d );
			ci.clearNeighbors();
			ci.alive = 1;

//			ci.grow = c.grow / 2.f;
//			c.grow /= 2.f;
			break;
		}
	}
}

void updateReproduce() {
	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &ci = cells[i];
		if( ! ci.alive ) continue;
		
		if( zrandF() < Cellspring_reproduceProb ) {
			reproduce( ci );
		}
	}
}

void updateWaveLogic() {
/*
	float dt = Cellspring_dt;
	float k0 = Cellspring_k0;
	float k1 = Cellspring_k1;
	float k2 = Cellspring_k2;
	float k3 = Cellspring_k3;
	float k4 = Cellspring_k4;
	
	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &ci = cells[i];
		if( ! ci.alive ) continue;

		// SUM neighbors excitement
		float s = 0.f;
		for( int j=0; j<MAX_NEIGHBORS; j++ ) {
			if( ci.neighbors[j] ) {
				s += ci.neighbors[j]->s;
			}
		}

//		float conversionRate = Cellspring_k2 * ci.excited * ci.excitability;
//		ci.excitability += Cellspring_dt * ( + Cellspring_k1 * ( 1.f - ci.excitability ) - conversionRate );
//		ci.excited      += Cellspring_dt * ( + conversionRate - Cellspring_k3 * ci.excited* ci.excited + Cellspring_k4 * neighborExcitement - Cellspring_k5 * ci.excited * ci.excitability  );

		ci.s += dt * ( +k1*ci.s -k2*ci.t*ci.t -k0*ci.s );
		ci.t += dt * ( +k3*ci.s -k4*ci.s );
	}

	cells[0].excited = sinf( Cellspring_freq * cellTime ) / 2.f + 0.5f;
*/
}

void plotLogicDynamics() {
	return;
	
	for( float e=0.f; e<1.f; e+=0.1f ) {
		for( float s=0.f; s<1.f; s+=0.1f ) {
			float conversionRate = Cellspring_k2 * s * e;
//			float de = Cellspring_plotLen * ( + Cellspring_k1 * ( 1.f - e ) - conversionRate );
//			float ds = Cellspring_plotLen * ( + conversionRate - Cellspring_k3 * s * s - Cellspring_k5 * e * s );
//			zglDrawVector( e, s, 0.f, de, ds, 0.f, 0.8f, 30.f );
		}
	}
}

void updateNeighbors() {
	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &ci = cells[i];
		if( ! ci.alive ) continue;
		
		// FIND new neighbors
		for( int j=i+1; j<MAX_CELLS; j++ ) {
			Cell &cj = cells[j];
			if( ! cj.alive ) continue;

			FVec3 d = ci.pos;
			d.sub( cj.pos );
			if( d.mag() < 1.0 ) {
				ci.addNeighbor( cj );
				cj.addNeighbor( ci );
			}
		}

		// REMOVE far neighbors
		for( int j=0; j<MAX_NEIGHBORS; j++ ) {
			if( ci.neighbors[j] ) {
				Cell &cj = *( ci.neighbors[j] );
				FVec3 d = ci.pos;
				d.sub( cj.pos );
				if( d.mag() > 1.1f ) {
					ci.removeNeighbor( cj );
				}
			}
		}
	}
}

void physics() {
	for( int i=0; i<MAX_CELLS; i++ ) {
		cells[i].force.origin();
	}

	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &ci = cells[i];
		if( ! ci.alive ) continue;

		for( int j=0; j<MAX_NEIGHBORS; j++ ) {
			if( ci.neighbors[j] ) {
				Cell &cj = *( ci.neighbors[j] );
				FVec3 d = ci.pos;
				d.sub( cj.pos );
				float m = d.mag();
				d.div( m );
				d.mul( Cellspring_force * (m - Cellspring_natLen) );
				ci.force.sub( d );
				cj.force.add( d );
			}
		}
	}

	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &ci = cells[i];
		if( ! ci.alive ) continue;

		ci.force.mul( Cellspring_dt );		
		ci.pos.add( ci.force );
	}
	
	cellTime += Cellspring_dt;
}

void render() {
	zviewpointSetupView();

	plotLogicDynamics();
	updateWaveLogic();
	updateReproduce();

	if( zTimeFrameCount % 5 == 0 ) {	
		updateNeighbors();
	}
	physics();


	glPointSize( 10.f );
	glBegin( GL_POINTS );
	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &ci = cells[i];
		if( ci.alive ) {
//			glColor3f( 0.f, ci.excitability, 0.f );
			glVertex3fv( ci.pos );

//			glColor3f( ci.excited, 0.f, 0.f );
			FVec3 offset = ci.pos;
			offset.add( FVec3( 0.1f, 0.f, 0.f ) );
			glVertex3fv( offset );
		}
	}
	glEnd();


/*
	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &c = cells[i];
		if( c.alive ) {
			glPushMatrix();
				glTranslatef( c.pos.x, c.pos.y, c.pos.z );
				gluSphere( sphere, Cellspring_radius, 10, 10 );
			glPopMatrix();
		}
	}
*/

	glColor3ub( 0, 0, 255 );
	glBegin( GL_LINES );
	for( int i=0; i<MAX_CELLS; i++ ) {
		Cell &c = cells[i];
		if( c.alive ) {
			for( int j=0; j<MAX_NEIGHBORS; j++ ) {
				if( c.neighbors[j] ) {
					glVertex3fv( c.pos );
					glVertex3fv( c.neighbors[j]->pos );
				}
			}
		}
	}
	glEnd();
}

void reset() {
	memset( cells, 0, sizeof(cells) );
	for( int i=0; i<10; i++ ) {
		cells[i].alive = 1;
		cells[i].pos = FVec3( zrandF(0.f,1.f), zrandF(0.f,1.f), 0.f );
	}
}

void startup() {
	sphere = gluNewQuadric();
	gluQuadricDrawStyle( sphere, GLU_FILL );
	gluQuadricNormals( sphere, GLU_SMOOTH );
    
   	reset();
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

