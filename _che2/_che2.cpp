// @ZBS {
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "memory.h"
#include "assert.h"
#include "math.h"
#include "string.h"
#include "float.h"
#include "stdio.h"
#include "stdlib.h"
// MODULE includes:
// ZBSLIB includes:
#include "zrand.h"
#include "zvars.h"
#include "zvec.h"
#include "zmathtools.h"
#include "ztime.h"
#include "zplugin.h"
#include "zgltools.h"
#include "zviewpoint.h"

ZVAR( int, Cell_________RENDER________, 0.f );
ZVAR( int, Cell_update, 200 );
ZVAR( float, Cell_pointSize, 3.f );
ZVAR( float, Cell_near, 1 );
ZVAR( float, Cell_far, 2000.0 );
ZVARB( int, Cell_freezeMembrane, 0, 0, 1  );

ZVAR( int, Cell_________PARTS_________, 0.f );
ZVAR( int, Cell_membraneViscoscity, 1 );
ZVAR( int, Cell_ab, 10000 );
ZVAR( int, Cell_bb, 0 );
ZVAR( int, Cell_bc, 10000 );
ZVAR( int, Cell_aParts, 400 );
ZVAR( int, Cell_bParts, 400 );
ZVAR( int, Cell_cParts, 400 );
ZVAR( int, Cell_updateCount, 0 );


ZPLUGIN_BEGIN( cell );
    
int worldUpdateCount = 0;



int partColorPalette[] = {
	0xFF0000FF,
	0xFF00FF00,
	0xFFFF0000,
	0xFF00FFFF,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mole 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Mole {
	static Mole *moleHead;

	int type;

	SVec3 pos;
		// The position in world coords (world array).  Note that only one
		// molecule can exist at a given world coord at a time

	int isMobile;
	
	short membraneBound;
		// True is some part of this molecule is membrane bound (a flag)

	int nextDisassociate;
		// When the next disassociation will occur among any of these particles

	Mole *next;
	Mole *prev;
		// Double link pointers

	Mole() {
		pos.origin();
		nextDisassociate = 0;
		next = prev = 0;
		membraneBound = 0;
		isMobile = 1;
	}

	void unlink() {
		if( prev ) {
			prev->next = next;
		}
		if( next ) {
			next->prev = prev;
		}
		if( moleHead == this ) {
			moleHead = next;
		}
		prev = next = 0;
	}

	void destroy() {
		unlink();
		delete this;
	}

	void linkToHead() {
		next = Mole::moleHead;
		prev = 0;
		if( Mole::moleHead ) {
			Mole::moleHead->prev = this;
		}
		Mole::moleHead = this;
	}

	int hasMembraneBoundPart() {
		return membraneBound;
	}

};

Mole *Mole::moleHead = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DrawList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DRAW_LIST_MAX (4096*10)

int drawListCount = 0;
SVec3 drawListVert[DRAW_LIST_MAX];
int drawListColr[DRAW_LIST_MAX];

#define DRAW_LIST_LINE_MAX (4096*4)

int drawListLineCount = 0;
SVec3 drawListLineVert[DRAW_LIST_LINE_MAX];

void drawListRender() {
	glEnable( GL_DEPTH_TEST );

	glPointSize( Cell_pointSize );
	
	glVertexPointer( 3, GL_SHORT, 0, drawListVert );
	glEnableClientState( GL_VERTEX_ARRAY );

	glColorPointer( 4, GL_UNSIGNED_BYTE, 0, drawListColr );
	glEnableClientState( GL_COLOR_ARRAY );

 	glDrawArrays( GL_POINTS, 0, drawListCount );

	glVertexPointer( 3, GL_SHORT, 0, drawListLineVert );
	glDisableClientState( GL_COLOR_ARRAY );
	//glColor3ub( 80, 80, 80 );
	glColor3ub( 255, 255, 255 );
 	glDrawArrays( GL_LINES, 0, drawListLineCount );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// World and Boundary hashes
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DIMX (512)
#define DIMY (128)
#define DIMZ (128)

Mole **worldGridPtr = 0;
char *worldInBoundsPtr = 0;


inline Mole **worldGrid( int z, int y, int x ) {
	return &worldGridPtr[ z*DIMY*DIMX + y*DIMX + x ];
}

inline char *worldInBounds( int z, int y, int x ) {
	return &worldInBoundsPtr[ sizeof(char) * z*DIMY*DIMX + y*DIMX + x ];
}

inline int worldIsOpen( SVec3 pos ) {
	return *worldInBounds(pos.z,pos.y,pos.x) != 0 && *worldGrid(pos.z,pos.y,pos.x) == 0;
}

void worldInBoundsClear() {
	memset( worldInBoundsPtr, 0, sizeof(char) * DIMX * DIMY * DIMZ );
}

void worldAlloc() {
	// ALLOC world grid and bound flags
	worldGridPtr = (Mole **)malloc( sizeof(Mole *) * DIMX * DIMY * DIMZ );
	memset( worldGridPtr, 0, sizeof(Mole *) * DIMX * DIMY * DIMZ );

	worldInBoundsPtr = (char *)malloc( sizeof(char) * DIMX * DIMY * DIMZ );
	memset( worldInBoundsPtr, 0, sizeof(char) * DIMX * DIMY * DIMZ );
}

void worldFree() {
	if( worldGridPtr ) {
		free( worldGridPtr );
		worldGridPtr = 0;
	}
	if( worldInBoundsPtr ) {
		free( worldInBoundsPtr );
		worldInBoundsPtr = 0;
	}
}

Mole *worldNewAt( int type, SVec3 pos ) {
	Mole *m = new Mole;
	m->type = type;
	m->pos = pos;
	m->linkToHead();

	assert( *worldGrid( pos.z, pos.y, pos.x ) == 0 );
	*worldGrid( pos.z, pos.y, pos.x ) = m;
	
	return m;
}

void worldSphereBoundary() {
	int dim = min( DIMX, min( DIMY, DIMZ ) );

	worldInBoundsClear();

	int radius2 = (dim/2-1) * (dim/2-1);
	IVec3 center( dim/2, dim/2, dim/2 );
	int x, y, z;
	for( z=0; z<dim; z++ ) {
		for( y=0; y<dim; y++ ) {
			for( x=0; x<dim; x++ ) {
				IVec3 here( x, y, z );
				here.sub( center );
				if( here.mag2() <= radius2 ) {
					*worldInBounds(z,y,x) = 1;
				}
			}
		}
	}
}

void worldCylinderBoundary() {
	worldInBoundsClear();

	int x, y, z;

	assert( DIMY == DIMZ );
	IVec3 center( DIMX/2, DIMY/2, DIMZ/2 );

	int radius2 = DIMY/2 - 1;
	radius2 *= radius2;
		// This is the radius squared
	 
	int barrelLen = DIMX/2 - DIMY/2 - 1;
		// This is half the length of the barrel

	IVec3 capACenter( center.x - barrelLen, center.y, center.z );
	IVec3 capBCenter( center.x + barrelLen, center.y, center.z );
		// These are the centers of the dome caps
	
	for( z=0; z<DIMZ; z++ ) {
		for( y=0; y<DIMY; y++ ) {
			for( x=0; x<DIMX; x++ ) {
				
				// CENTER
				if( abs(x-center.x) < barrelLen && (y-center.y)*(y-center.y) + (z-center.z)*(z-center.z) <= radius2 ) {
					*worldInBounds(z,y,x) = 1;
				}

				// CAPS
				if( (x-capACenter.x)*(x-capACenter.x) + (y-capACenter.y)*(y-capACenter.y) + (z-capACenter.z)*(z-capACenter.z) <= radius2 ) {
					*worldInBounds(z,y,x) = 1;
				}

				if( (x-capBCenter.x)*(x-capBCenter.x) + (y-capBCenter.y)*(y-capBCenter.y) + (z-capBCenter.z)*(z-capBCenter.z) <= radius2 ) {
					*worldInBounds(z,y,x) = 1;
				}

			}
		}
	}
}

Mole *worldAddAtEndLeft( int type ) {
	SVec3 center( DIMX/2, DIMY/2, DIMZ/2 );
	int barrelLen = DIMX/2 - DIMY/2 - 1;
	SVec3 capACenter( center.x - barrelLen, center.y, center.z );
	SVec3 endPoint( 0, DIMY/2, DIMZ/2 );

	// SEARCH the zy plane looking for the cloest open space on the bundary
	int closest = 100000000;
	SVec3 closestPos(-1,-1,-1);
	for( int z=0; z<DIMZ; z++ ) {
		for( int y=0; y<DIMY; y++ ) {

			for( int x=capACenter.x; x>=3; x-- ) {
				SVec3 test( x, y, z );
				SVec3 testxm1( x-1, y, z );
				if( worldIsOpen(test) && !worldIsOpen(testxm1) ) {
					test.sub( endPoint );
					if( test.mag2() < closest ) {
						closest = test.mag2();
						closestPos = SVec3( x, y, z );
					}
				}
			}
		}
	}

	assert( closestPos.x >= 0 );
	return worldNewAt( type, closestPos );			
}

Mole *worldAddSinglePartMoleAnywhere( int type ) {
	// FIND a place for the new particle
	int i;
	SVec3 pos;
	int membraneIndex = 0;
	
	for( i=0; i<10000; i++ ) {
		pos.x = zrandI(0,DIMX);
		pos.y = zrandI(0,DIMY);
		pos.z = zrandI(0,DIMZ);
		if( worldIsOpen(pos) ) {
			break;
		}
	}
	assert( i < 10000 );

	return worldNewAt( type, pos );
}

Mole *worldFindMoleOfType( int type ) {
	for( Mole *m = Mole::moleHead; m; m=m->next ) {
		if( m->type == type ) {
			return m;
		}
	}
	return 0;
}

SVec3 worldFindOpenNear( SVec3 pos, int membrane ) {
	// 3D SPRIAL search looking for a place nearby to place the new mole
	// This is not a very efficient algorithm since it checks some
	// of the same places more than once but it is good enough for now
	// since this isn't in the inner loop core.
	// @TODO: Optimize with a lookup table
	int minDim = min( DIMX, min( DIMY, DIMZ ) );
	for( int i=0; i<minDim; i++ ) {
		for( int z=-i; z<i; z++ ) {
			int _z = z + pos.z;
			if( _z >= 0 && _z < DIMZ ) {
				for( int y=-i; y<i; y++ ) {
					int _y = y + pos.y;
					if( _y >= 0 && _y < DIMY ) {
						for( int x=-i; x<i; x++ ) {
							int _x = x + pos.x;
							if( _x >= 0 && _x < DIMX ) {
								SVec3 p( _x,_y,_z);
								if( worldIsOpen(p) ) {
									return p;
								}
							}
						}
					}
				}
			}
		}
	}
	assert( 0 );
	return SVec3(0,0,0);
}

int (*collisionCallback)( Mole *a, Mole *b );


void worldUpdate() {
	// UPDATE the position of every particle.
	// CHECK for collisions. On collision:
	//   Foreach part on mole a vs every part on mole b:
	//     Foreach domain on parta vs each domain on partb
	//       If the domains can bind then:
	//         remove mole b, place all of its parts under control of molea
	// CHECK for dissasociation
	//   Create a new mole for the unbound
	// STUFF the color and verts of the parts into the draw buffer

	worldUpdateCount++;
	Cell_updateCount = worldUpdateCount;

	// CLEAR the draw list
	drawListCount = 0;
	drawListLineCount = 0;

	Mole *m;
	for( m = Mole::moleHead; m; m=m->next ) {
		if( m->isMobile ) {
			SVec3 oldPos = m->pos;

			// POP out of the spatial index
			*worldGrid( oldPos.z, oldPos.y, oldPos.x ) = 0;

			// UPDATE position based on random walk
			SVec3 newPos = oldPos;
			static SVec3 dirs[6] = { SVec3(1,0,0), SVec3(-1,0,0), SVec3(0,1,0), SVec3(0,-1,0), SVec3(0,0,1), SVec3(0,0,-1) };
			newPos.add( dirs[ zrandI(0,6) ] );

			int posReset = 0;

			// COLLISION check with boundary
			if( newPos.z < 0 || newPos.z >= DIMZ ) {
				posReset = 1;
			}
			else if( newPos.y < 0 || newPos.y >= DIMY ) {
				posReset = 1;
			}
			else if( newPos.x < 0 || newPos.x >= DIMX ) {
				posReset = 1;
			}
			else if( ! *worldInBounds( newPos.z, newPos.y, newPos.x ) ) {
				posReset = 1;
			}

			// COLLISION check with other molecules
			Mole *collide = *worldGrid( newPos.z, newPos.y, newPos.x );
			if( collide ) {
				// There is a molecule here.  If they can bind then this
				// Mole becomes the master and incorporates all of the
				// parts from the other one.  The other one is deleted.
				
				int merged = (*collisionCallback)( m, collide );
				if( !merged ) {
					// These molecules collided but were unable to bind so
					// move this particle back to where it was
					posReset = 1;
				}
				else {
					newPos = m->pos;
				}
			}

			// PUSH into the spatial index
			if( posReset ) {
				m->pos = oldPos;
				*worldGrid( oldPos.z, oldPos.y, oldPos.x ) = m;
			}
			else {
				m->pos = newPos;	
				*worldGrid( newPos.z, newPos.y, newPos.x ) = m;
			}
		}
		
		// CHECK for disassociation
		//Mole *newMole = m->disassociateCheck();
		//if( newMole ) {
		//}

		// ADD the parts to the draw list
		SVec3 pos = m->pos;
		drawListVert[drawListCount] = pos;
		drawListColr[drawListCount] = partColorPalette[ m->type ];
		drawListCount++;

		assert( drawListCount < DRAW_LIST_MAX );
		assert( drawListLineCount < DRAW_LIST_LINE_MAX );
	}
}

//////////////////////////////////////////////////////////////////////////
// render
//////////////////////////////////////////////////////////////////////////

void render() {
	for( int i=0; i<Cell_update; i++ ) {
		worldUpdate();
	}

	glClearColor( 0.f, 0.f, 0.f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float viewport[4];
	glGetFloatv( GL_VIEWPORT, viewport );
	gluPerspective( 30.f, viewport[2] / viewport[3], Cell_near, Cell_far );
	glMatrixMode(GL_MODELVIEW);
	gluLookAt( 0.0, 0.0, DIMX*2.1, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 );
	zviewpointSetupView();
	glTranslatef( -DIMX/2, -DIMY/2, -DIMZ/2 );

	drawListRender();

	glBegin( GL_LINES );
		glColor3f( 1.f, 0.f, 0.f );
		glVertex3i( 0, 0, 0 );
		glVertex3i( DIMX, 0, 0 );

		glColor3f( 0.f, 1.f, 0.f );
		glVertex3i( 0, 0, 0 );
		glVertex3i( 0, DIMY, 0 );

		glColor3f( 0.f, 0.f, 1.f );
		glVertex3i( 0, 0, 0 );
		glVertex3i( 0, 0, DIMZ );
	glEnd();
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Collision callbacks 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int cheCallback( Mole *a, Mole *b ) {
	if( a->type == 1 && b->type == 2 ) {
		b->type = 3;
		return 1;
	}
	if( a->type == 2 && b->type == 1 ) {
		a->type = 3;
		return 1;
	}
	return 0;
		// Return 1 if the two merge into one and 0 otherwise
};



void startup() {
	worldAlloc();
	worldUpdateCount = 0;
	worldCylinderBoundary();
	zviewpointReset();

	for( int i=0; i<100; i++ ) {
		Mole *m = worldAddAtEndLeft( 1 );
		m->isMobile = 0;
	}
	
	collisionCallback = cheCallback;	
	for( int i=0; i<1000; i++ ) {
		worldAddSinglePartMoleAnywhere( 2 );
	}
}

void shutdown() {
	worldFree();
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,a) ) {
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

