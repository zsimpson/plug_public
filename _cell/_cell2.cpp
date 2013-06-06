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

ZVAR( int, Cell2_________RENDER________, 0.f );
ZVAR( int, Cell2_update, 1 );
ZVAR( float, Cell2_pointSize, 3.f );
ZVAR( float, Cell2_near, 1 );
ZVAR( float, Cell2_far, 2000.0 );
ZVARB( int, Cell2_freezeMembrane, 0, 0, 1  );

ZVAR( int, Cell2_________PARTS_________, 0.f );
ZVAR( int, Cell2_membraneViscoscity, 1 );
ZVAR( int, Cell2_ab, 10000 );
ZVAR( int, Cell2_bb, 0 );
ZVAR( int, Cell2_bc, 10000 );
ZVAR( int, Cell2_aParts, 4000 );
ZVAR( int, Cell2_bParts, 100 );
ZVAR( int, Cell2_cParts, 0 );
ZVAR( int, Cell2_updateCount, 0 );
ZVAR( int, Cell2_redBlue_dissociate, 100 );

ZPLUGIN_BEGIN( cell2 );
    
int worldUpdateCount = 0;

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

	glPointSize( Cell2_pointSize );
	
	glVertexPointer( 3, GL_SHORT, 0, drawListVert );
	glEnableClientState( GL_VERTEX_ARRAY );

	glColorPointer( 4, GL_UNSIGNED_BYTE, 0, drawListColr );
	glEnableClientState( GL_COLOR_ARRAY );

 	glDrawArrays( GL_POINTS, 0, drawListCount );

	glVertexPointer( 3, GL_SHORT, 0, drawListLineVert );
	glDisableClientState( GL_COLOR_ARRAY );
	glColor3ub( 255, 255, 255 );
 	glDrawArrays( GL_LINES, 0, drawListLineCount );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Boundary hashes
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DIMX (256)
#define DIMY (64)
#define DIMZ (64)

struct Mole;
Mole **worldGridPtr = 0;
char *worldInBoundsPtr = 0;

struct MembraneNeighbor {
	SVec3 pos;
		// The position of this element of the membrane

	int count;
		// The number of neighbors it has

	#define MEMBRANE_NEIGHBORS_MAX (16)
	SVec3 neighbors[MEMBRANE_NEIGHBORS_MAX];
		// The positions of the neighbors
};

int membraneCount = 0;

MembraneNeighbor *membraneNeighborsPtr = 0;
	// For every membrane point, a MembraneNeighor records gives the list of all neighbors

MembraneNeighbor **membraneNeighborPtrFromPosPtr = 0;
	// This is a lookup from space to the neighbor list


inline Mole **worldGrid( int z, int y, int x ) {
	return &worldGridPtr[ z*DIMY*DIMX + y*DIMX + x ];
}

inline char *worldInBounds( int z, int y, int x ) {
	return &worldInBoundsPtr[ sizeof(char) * z*DIMY*DIMX + y*DIMX + x ];
}

inline MembraneNeighbor *membraneNeighbor( int i ) {
	return &membraneNeighborsPtr[ i ];
}

inline MembraneNeighbor *membraneNeighbor( int z, int y, int x ) {
	return membraneNeighborPtrFromPosPtr[ z*DIMY*DIMX + y*DIMX + x ];
}

inline int worldIsOpen( SVec3 pos ) {
	return *worldInBounds(pos.z,pos.y,pos.x) != 0 && *worldGrid(pos.z,pos.y,pos.x) == 0;
}

inline int worldIsMembrane( SVec3 pos ) {
	return pos.z>=0 && pos.z<DIMZ && pos.y>=0 && pos.y<DIMY && pos.x>=0 && pos.x<DIMX && *worldInBounds(pos.z,pos.y,pos.x) == 2;
}

inline void worldInsertMole( Mole *m, SVec3 pos ) {
	Mole **index = worldGrid( pos.z, pos.y, pos.x );
	assert( index );
	assert( !*index );
	*index = m;
}

inline void worldRemoveMole( Mole *m, SVec3 pos ) {
	Mole **index = worldGrid( pos.z, pos.y, pos.x );
	assert( index );
	assert( *index == m );
	*index = 0;
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
	if( membraneNeighborsPtr ) {
		free( membraneNeighborsPtr );
		membraneNeighborsPtr = 0;
	}
	if( membraneNeighborPtrFromPosPtr ) {
		free( membraneNeighborPtrFromPosPtr );
		membraneNeighborPtrFromPosPtr = 0;
	}
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

void worldBoundaryBuild() {
	int x, y, z, i;

	// ALLOCATE the temporary membrane structs
	int *membranePosToOffset = (int *)malloc( sizeof(int) * DIMZ * DIMY * DIMX );
	memset( membranePosToOffset, -1, sizeof(membranePosToOffset) );
		// The membranePosToOffset gives us a position to offset in the membraneNeighbors list

	SVec3 *membranePos = (SVec3 *)malloc( sizeof(SVec3) * DIMZ * DIMY * DIMX );
	memset( membranePos, 0, sizeof(SVec3) * DIMZ * DIMY * DIMX );
		// This gives us the position of each membrane point

	// BUILD a membrane list by finding in inBounds place that has an out of bounds neighbor
	membraneCount = 0;
	for( z=1; z<DIMZ-1; z++ ) {
		for( y=1; y<DIMY-1; y++ ) {
			for( x=1; x<DIMX-1; x++ ) {
				if( *worldInBounds(z,y,x) ) {
					if(
						!*worldInBounds(z,y,x+1) ||
						!*worldInBounds(z,y,x-1) ||
						!*worldInBounds(z,y+1,x) ||
						!*worldInBounds(z,y-1,x) ||
						!*worldInBounds(z+1,y,x) ||
						!*worldInBounds(z-1,y,x)
					) {
						*worldInBounds(z,y,x) = 2;
						membranePos[membraneCount] = SVec3( x, y, z );
						membranePosToOffset[z*DIMY*DIMX + y*DIMX + x] = membraneCount;
						membraneCount++;
					}
				}
			}
		}
	}

	// ALLOCATE the permanent membrane neighbor structures
	membraneNeighborsPtr = (MembraneNeighbor *)malloc( sizeof(MembraneNeighbor) * membraneCount );
	memset( membraneNeighborsPtr, 0, sizeof(MembraneNeighbor) * membraneCount );

	membraneNeighborPtrFromPosPtr = (MembraneNeighbor **)malloc( sizeof(MembraneNeighbor *) * DIMX * DIMY * DIMZ );
	memset( membraneNeighborPtrFromPosPtr, 0, sizeof(MembraneNeighbor *) * DIMX * DIMY * DIMZ );

	// SEARCH for neighbors by accumulating a list of all nearby membranes spots within sqrt(3) voxels
	const int searchRadius = 2;
	for( i=0; i<membraneCount; i++ ) {
		SVec3 p0 = membranePos[i];
	
		membraneNeighborsPtr[i].pos = p0;

		for( int z=-searchRadius; z<searchRadius; z++ ) {
			for( int y=-searchRadius; y<searchRadius; y++ ) {
				for( int x=-searchRadius; x<searchRadius; x++ ) {

					SVec3 p1 = p0;
					p1.add( SVec3(x, y, z) );

					if( worldIsMembrane(p1) && !(x==0 && y==0 && z==0) ) {
						if( x*x + y*y + z*z <= 3 ) {
							assert( membraneNeighborsPtr[i].count < MEMBRANE_NEIGHBORS_MAX );
							membraneNeighborsPtr[i].neighbors[ membraneNeighborsPtr[i].count ] = p1;
							membraneNeighborsPtr[i].count++;
						}
					}
				}
			}
		}
	}

	// BUILD the spatial to membrane neighbor lookup table
	for( z=0; z<DIMZ; z++ ) {
		for( y=0; y<DIMY; y++ ) {
			for( x=0; x<DIMX; x++ ) {
				membraneNeighborPtrFromPosPtr[z*DIMY*DIMX + y*DIMX + x] = &membraneNeighborsPtr[ membranePosToOffset[z*DIMY*DIMX + y*DIMX + x] ];
			}
		}
	}

	free( membranePosToOffset );
	free( membranePos );
}


SVec3 worldFindOpenNear( SVec3 pos, int membrane ) {
	// 3D SPRIAL search looking for a place nearby to place the new mole
	// This is not a very efficient algorithm since it checks some
	// of the same places more than once but it is good enough for now
	// since this isn't in the inner loop.
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
								if( worldIsOpen(p) && (!membrane || worldIsMembrane(p) ) ) {
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part and Mole 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// I need some kind of more generic way of programming molecular interactions
// A callback which says something like: "A collision has taken place, decide if they would bind"
// For simple cases, the callbacks will place a state on the molecule
// Also, anything that is bound can setup a timer that says when it will next break and
// gets a callback on that event


#define MAX_PART_TYPES (3)

int membraneBoundByType[MAX_PART_TYPES];

int partColorPalette[ MAX_PART_TYPES ] = {
	0xFF0000FF,
	0xFF00FF00,
	0xFFFF0000,
};

struct Mole {
	static Mole *moleHead;

	SVec3 pos;
		// The position in world coords (world array).  Note that only one
		// molecule can exist at a given world coord at a time

	short membraneBound;
		// True is some part of this molecule is membrane bound (a flag)

	#define MAX_PARTS_PER_MOLE (8)
	int numParts;
	int parts[MAX_PARTS_PER_MOLE];
		// @TODO: Changing this to a linked list would allow us to
		// build more complex large polymers, etc.  For now this is
		// the simplest thing to do.

	int nextDisassociate;
		// When the next disassociation will occur among any of these particles

	Mole *next;
	Mole *prev;
		// Double link pointers

	virtual void indexRemove() {
		worldRemoveMole(this,pos);
		pos.origin();
	}

	virtual void indexInsert() {
		worldInsertMole(this,pos);
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

	void linkToHead() {
		next = Mole::moleHead;
		prev = 0;
		if( Mole::moleHead ) {
			Mole::moleHead->prev = this;
		}
		Mole::moleHead = this;
	}

	void addPart( int part ) {
		parts[numParts] = part;
		numParts++;
		assert( numParts < MAX_PARTS_PER_MOLE );
	}

	int hasPartType( int type ) {
		for( int i=0; i<numParts; i++ ) {
			if( parts[i] == type ) {
				return 1;
			}
		}
		return 0;
	}

	virtual void bind( Mole *o ) {
		memcpy( &parts[numParts], &o->parts[0], sizeof(parts[0]) * o->numParts );
		numParts += o->numParts;
		assert( numParts < MAX_PARTS_PER_MOLE );
		delete o;
	}

	virtual void unbind( Mole *prototype, int partIndex ) {
		memcpy( prototype->parts, &parts[partIndex], sizeof(parts[0])*(numParts-partIndex) );
		prototype->numParts = numParts - partIndex;
		numParts = partIndex;
		assert( numParts > 0 && numParts < MAX_PARTS_PER_MOLE );
		assert( prototype->numParts > 0 && prototype->numParts < MAX_PARTS_PER_MOLE );
	}

	virtual int collision( Mole *b )=0;
		// Returns one if the collision is handled.

	virtual void dissociate()=0;

	Mole() {
		pos.origin();
		memset( parts, 0, sizeof(parts) );
		numParts = 0;
		nextDisassociate = 0;
		next = prev = 0;
		membraneBound = 0;
	}

	virtual ~Mole() {
		indexRemove();
		unlink();
	}

};

Mole *Mole::moleHead = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct HomelessMole : Mole {
	int state;
	enum {
		UNKNOWN=0, RED_SOLO=1, GREEN_SOLO, BLUE_SOLO, RED_GREEN, RED_BLUE,
	};

	HomelessMole( int type ) : Mole() {
		switch( type ) {
			case 0: state=RED_SOLO  ; break;
			case 1: state=GREEN_SOLO; break;
			case 2: state=BLUE_SOLO ; break;
		}
	}

	virtual int collision( Mole *_b ) {
		HomelessMole *b = (HomelessMole *)_b;
		// Red are bound to the membrane
		// Green will bind to red permanently
		// Blue when they collide with Red-Green cause the Red-Green bonds to break
		// CHECK for Red / Green collision
		if( state == RED_SOLO && b->state == GREEN_SOLO ) {
			bind( b );
			state = RED_GREEN;
			return 1;
		}

		// CHECK for Red-Green / Blue
		else if( state == RED_GREEN && b->state == BLUE_SOLO ) {
			assert( numParts == 2 && b->numParts == 1 );
			HomelessMole *o = new HomelessMole( 1 );
			unbind( o, 1 );
			o->pos = worldFindOpenNear( pos, 0 );
			o->indexInsert();
			bind( b );
			state = RED_BLUE;
			nextDisassociate = worldUpdateCount + Cell2_redBlue_dissociate;
			return 1;
		}
		return 0;
	}

	virtual void dissociate() {
		assert( state == RED_BLUE );
		HomelessMole *o = new HomelessMole( 2 );
		unbind( o, 1 );
		o->pos = worldFindOpenNear( pos, 0 );
		o->indexInsert();
		state = RED_SOLO;
		nextDisassociate = 0;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void addSinglePartMole( Mole *prototype, int type, int membraneBound ) {
	// FIND a place for the new particle
	int i;
	SVec3 pos;
	int membraneIndex = 0;
	
	if( membraneBound ) {
		for( i=0; i<10000; i++ ) {
			membraneIndex = zrandI( 0, membraneCount );
			pos = membraneNeighbor( membraneIndex )->pos;
			if( worldIsOpen(pos) ) {
				break;
			}
		}
		assert( i < 10000 );
	}
	else {
		for( i=0; i<10000; i++ ) {
			pos.x = zrandI(0,DIMX);
			pos.y = zrandI(0,DIMY);
			pos.z = zrandI(0,DIMZ);
			if( worldIsOpen(pos) ) {
				break;
			}
		}
		assert( i < 10000 );
	}

	prototype->addPart( type );
	prototype->pos = pos;
	prototype->membraneBound = membraneBound;
	prototype->indexInsert();
	prototype->linkToHead();
}

Mole *findSoloMole( int type ) {
	for( Mole *m = Mole::moleHead; m; m=m->next ) {
		if( m->numParts == 1 && m->parts[0] == type ) {
			return m;
		}
	}
	return 0;
}

void update( int setDrawList ) {
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
	Cell2_updateCount = worldUpdateCount;

	int partCount[MAX_PART_TYPES] = {0,};

	// CLEAR the draw list
	drawListCount = 0;
	drawListLineCount = 0;

	Mole *m, *next;
	for( m = Mole::moleHead; m; m=next ) {
		// UPDATE position based on random walk
		next = m->next;

		// CHECK for disassociation
		if( worldUpdateCount >= m->nextDisassociate && m->nextDisassociate ) {
			// A callback has been requested at this point
			m->dissociate();
		}

		// ADD the parts to the draw list
		if( setDrawList ) {
			SVec3 pos = m->pos;
			for( int i=0; i<m->numParts; i++ ) {
				int type = m->parts[i];
				partCount[type]++;

				drawListVert[drawListCount] = pos;
				drawListColr[drawListCount] = partColorPalette[ type ];
				drawListCount++;

				if( m->numParts >= 2 && i > 0 ) {
					SVec3 pos0 = pos;
					pos0.x-=2;
					drawListLineVert[drawListLineCount++] = pos0;
					drawListLineVert[drawListLineCount++] = pos;
				}

				pos.x+=2;
			}
			assert( drawListCount < DRAW_LIST_MAX );
			assert( drawListLineCount < DRAW_LIST_LINE_MAX );
		}

		SVec3 oldPos = m->pos;
		SVec3 newPos = m->pos;
		int moved = 0;
		if( m->membraneBound ) {
			if( !Cell2_freezeMembrane && !zrandI(0,Cell2_membraneViscoscity) ) {
				MembraneNeighbor *mn = membraneNeighbor( oldPos.z, oldPos.y, oldPos.x );
				newPos = mn->neighbors[ zrandI(0,mn->count) ];
			}
		}
		else {
			static SVec3 dirs[6] = { SVec3(1,0,0), SVec3(-1,0,0), SVec3(0,1,0), SVec3(0,-1,0), SVec3(0,0,1), SVec3(0,0,-1) };
			newPos.add( dirs[ zrandI(0,6) ] );
		}

		// COLLISION check with boundary, don't move if you hit the edge
		if( newPos.z < 0 || newPos.z >= DIMZ || newPos.y < 0 || newPos.y >= DIMY || newPos.x < 0 || newPos.x >= DIMX ) {
			continue;
		}
		else if( ! *worldInBounds( newPos.z, newPos.y, newPos.x ) ) {
			continue;
		}
		else {
			// COLLISION check with other molecules
			Mole *collide = *worldGrid( newPos.z, newPos.y, newPos.x );
			if( collide ) {
				// RUN the collision callback. If this returns 1 then it
				// means that the collision was handled.  Otherwise the
				// opposite symetric collision is given a chance.
				moved = m->collision( collide );
				if( !moved ) {
					moved = collide->collision( m );
				}

				if( !moved ) {
					// collision is unresolved, disallow move
					moved = 1;
				}
			}

			if( !moved ) {
				// POP out of the spatial index, move, insert again
				m->indexRemove();
				m->pos = newPos;
				m->indexInsert();
			}
		}
	}

	// ADD / REMOVE parts as needed
	if( setDrawList ) {
		int desiredCount[MAX_PART_TYPES] = { Cell2_aParts, Cell2_bParts, Cell2_cParts };
		for( int i=0; i<MAX_PART_TYPES; i++ ) {
			while( partCount[i] < desiredCount[i] ) {
				// @TODO: This should be abstracted so that it doesn't know anything about homeless
				HomelessMole *m = new HomelessMole( i );
				addSinglePartMole( m, i, membraneBoundByType[i] );
				partCount[i]++;
			}

			while( partCount[i] > desiredCount[i] ) {
				Mole *_m = findSoloMole( i );
				if( !_m ) {
					break;
				}
				if( _m != m ) {
					delete _m;
					partCount[i]--;
				}
			}
		}
	}
}

void buildPlots() {
//	guiGet(distPlot,GUI2DPlot);
//	if( !distPlot ) {
//		return;
//	}
//
//	// Build a histogram of all the distances
//	const int bucketCount = 40;
//	double dist[2][bucketCount];
//	memset( dist, 0, sizeof(dist) );
//	int maxRad = max( DIMZ, max( DIMX, DIMY ) );
//	const int step = 2;
//	int i;
//
//	int radius = maxRad/2;
//	SVec3 center( radius, radius, radius );
//	float bucketCountF = (float)bucketCount;
//	float radiusF = (float)radius;
//
//	Mole *m;
//	for( m = Mole::moleHead; m; m=m->next ) {
//		for( int i=0; i<m->numParts; i++ ) {
//			int type = m->parts[i];
//			if( type ) {
//				SVec3 pos = m->pos;
//				pos.sub( center );
//				int bucket = max( 0, min( bucketCount-1, zmathRoundToInt(bucketCountF * (float)pos.mag() / radiusF) ) );
//				dist[type-1][bucket]++;
//			}
//		}
//	}
//
//
//	GUI2dSeries *series[2];
//	series[0] = new GUI2dSeries;
//	series[1] = new GUI2dSeries;
//	for( i=0; i<2; i++ ) {
//		for( int j=0; j<bucketCount; j++ ) {
//			series[i]->add( j, dist[i][j] );
//		}
//	}
//
//	distPlot->seriesRemoveAll();
//	distPlot->seriesAdd( series[0] );
//	distPlot->seriesAdd( series[1] );
//
//	distPlot->setAttrF( "minX", 0.f );
//	distPlot->setAttrI( "maxX", bucketCount );
//	distPlot->setAttrF( "minY", 0.f );
//	distPlot->setAttrD( "maxY", 10.f/*maxVal*/ );
//	if( !distPlot->getAttrI("builtSpace") ) distPlot->buildSpaceFromMinMax();
}


void render() {
	for( int i=0; i<Cell2_update; i++ ) {
		update( i==Cell2_update-1 );
	}

	glClearColor( 0.f, 0.f, 0.f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float viewport[4];
	glGetFloatv( GL_VIEWPORT, viewport );
	gluPerspective( 30.f, viewport[2] / viewport[3], Cell2_near, Cell2_far );
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

//	if( !(zTimeFrameCount % 5) ) {
//		worldBuildPlots();
//	}
}

void startup() {
	membraneBoundByType[0] = 1;
	membraneBoundByType[1] = 0;
	membraneBoundByType[2] = 0;

	worldAlloc();

	worldUpdateCount = 0;

	//worldSphereBoundary();
	worldCylinderBoundary();

	worldBoundaryBuild();
	zviewpointReset();
//	guiExecuteFile( "_cell/_cell.gui" );
}

void shutdown() {
	worldFree();

//	GUIObject *o = guiFind("cell");
//	if( o ) o->die();
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,a) ) {
		if( Cell2_near <= 10 ) {
			Cell2_near = 1.1e3;
			Cell2_far = 1.07e3;
		}
		else {
			Cell2_near = 1;
			Cell2_far = 2000;
		}
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

