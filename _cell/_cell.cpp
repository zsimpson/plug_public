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
ZVAR( int, Cell_update, 1 );
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// binding domain matrix
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_PART_TYPES (3)

#define MAX_DOMAINS (4)
int bindingMatrix[MAX_PART_TYPES * MAX_DOMAINS][MAX_PART_TYPES * MAX_DOMAINS];

void bindingMatrixSetup0() {
	// non-competitive
	memset( bindingMatrix, 0, sizeof(bindingMatrix) );

	// A:0 <-> B:0 B:1 <-> C:0
	bindingMatrix[ 0*MAX_DOMAINS + 0 ][ 1*MAX_DOMAINS + 0 ] = Cell_ab;
	bindingMatrix[ 1*MAX_DOMAINS + 0 ][ 0*MAX_DOMAINS + 0 ] = Cell_ab;

	bindingMatrix[ 1*MAX_DOMAINS + 1 ][ 2*MAX_DOMAINS + 0 ] = Cell_bc;
	bindingMatrix[ 2*MAX_DOMAINS + 0 ][ 1*MAX_DOMAINS + 1 ] = Cell_bc;
}

void bindingMatrixSetup1() {
	// competitive
	memset( bindingMatrix, 0, sizeof(bindingMatrix) );

	// A:0 <-> B:0 <-> C:0
	bindingMatrix[ 0*MAX_DOMAINS + 0 ][ 1*MAX_DOMAINS + 0 ] = Cell_ab;
	bindingMatrix[ 1*MAX_DOMAINS + 0 ][ 0*MAX_DOMAINS + 0 ] = Cell_ab;

	bindingMatrix[ 1*MAX_DOMAINS + 0 ][ 2*MAX_DOMAINS + 0 ] = Cell_bc;
	bindingMatrix[ 2*MAX_DOMAINS + 0 ][ 1*MAX_DOMAINS + 0 ] = Cell_bc;
}

void bindingMatrixSetup2() {
	// polymer test
	memset( bindingMatrix, 0, sizeof(bindingMatrix) );

	// A:0 <-> B:0 B:1 <-> C:0
	// B:1 <-> B:0
	bindingMatrix[ 0*MAX_DOMAINS + 0 ][ 1*MAX_DOMAINS + 0 ] = Cell_ab;
	bindingMatrix[ 1*MAX_DOMAINS + 0 ][ 0*MAX_DOMAINS + 0 ] = Cell_ab;

	bindingMatrix[ 1*MAX_DOMAINS + 1 ][ 2*MAX_DOMAINS + 0 ] = Cell_bc;
	bindingMatrix[ 2*MAX_DOMAINS + 0 ][ 1*MAX_DOMAINS + 1 ] = Cell_bc;

	bindingMatrix[ 1*MAX_DOMAINS + 1 ][ 1*MAX_DOMAINS + 0 ] = Cell_bb;
	bindingMatrix[ 1*MAX_DOMAINS + 0 ][ 1*MAX_DOMAINS + 1 ] = Cell_bb;

}

int partColorPalette[ MAX_PART_TYPES ] = {
	0xFF0000FF,
	0xFF00FF00,
	0xFFFF0000,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part and Mole 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Part {
	// A Particle is the basic element of interaction in this system.
	// Parts can come together to form Molecules (Mole)
	// Only Molecules have position, etc.

	int type;
		// The type of the particle
	
	Part *domainBindings[MAX_DOMAINS];
		// For each domain, this is a pointer to the bound part
	int domainDisassocTime[MAX_DOMAINS];
		// For each domain, this says when that bond will break

	Part() {
		type = 0;
		memset( domainBindings, 0, sizeof(domainBindings) );
		memset( domainDisassocTime, 0, sizeof(domainDisassocTime) );
	}

	int isDomainFree( int d ) {
		return domainBindings[d] == 0;
	}

	void bindDomain( int d, Part *p, int disassoc ) {
		domainBindings[d] = p;
		domainDisassocTime[d] = disassoc;
	}

	void unbind( Part *p ) {
		for( int i=0; i<MAX_DOMAINS; i++ ) {
			if( domainBindings[i] == p ) {
				domainBindings[i] = 0;
				domainDisassocTime[i] = 0;
				break;
			}
		}
	}
};

struct Mole {
	static Mole *moleHead;

	SVec3 pos;
		// The position in world coords (world array).  Note that only one
		// molecule can exist at a given world coord at a time

	short membraneBound;
		// True is some part of this molecule is membrane bound (a flag)

	#define MAX_PARTS_PER_MOLE (32)
	int numParts;
	Part *parts[MAX_PARTS_PER_MOLE];
		// @TODO: Changing this to a linked list would allow us to
		// build more complex large polymers, etc.  For now this is
		// the simplest thing to do.

	int nextDisassociate;
		// When the next disassociation will occur among any of these particles

	Mole *next;
	Mole *prev;
		// Double link pointers

	Mole() {
		pos.origin();
		memset( parts, 0, sizeof(parts) );
		numParts = 0;
		nextDisassociate = 0;
		next = prev = 0;
		membraneBound = 0;
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
		for( int i=0; i<numParts; i++ ) {
			delete parts[i];
			parts[i] = 0;
		}
		numParts = 0;
		unlink();
	}

	void addPart( Part *p ) {
		parts[numParts] = p;
		numParts++;
		assert( numParts < MAX_PARTS_PER_MOLE );
	}

	int hasPartType( int type ) {
		for( int i=0; i<numParts; i++ ) {
			if( parts[i]->type == type ) {
				return 1;
			}
		}
		return 0;
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
		// @TODO: Unhardcode this and use a proper type table
		for( int i=0; i<numParts; i++ ) {
			if( parts[i]->type == 0 ) {
				return 1;
			}
		}
		return 0;
	}

	Mole *breakBond( Part *a, Part *b ) {
		int i, j, k, m;

		a->unbind( b );
		b->unbind( a );

		int stackTop = 0;
		#define MAX_STACK (128)
		Part *stack[MAX_STACK];
		stack[stackTop++] = b;

		// TRAVERSE the stack adding children of the stack 
		// which aren't already on the stack

		for( j=0; j<stackTop; j++ ) {
			Part *p = stack[j];

			for( k=0; k<MAX_DOMAINS; k++ ) {
				if( p->domainBindings[k] ) {
					// ADD this part if it isn't already on the stack
					int found = 0;
					for( m=0; m<stackTop; m++ ) {
						if( p->domainBindings[k] == stack[m] ) {
							found = 1;
							break;
						}
					}
					if( !found ) {
						stack[stackTop++] = p->domainBindings[k];
					}
				}
			}
		}

		// The stack now has the recursive list of all parts that are
		// on the b side of the bond so we want to make them into a new molecule

		// CREATE the new mole and add all of the parts that belong
		Mole *mole = new Mole;
		mole->numParts = stackTop;
		memcpy( mole->parts, stack, stackTop * sizeof(Part *) );

		// REMOVE all of the parts that are in the new mole from this mole
		for( i=0; i<stackTop; i++ ) {
			for( j=0; j<numParts; j++ ) {
				if( parts[j] == stack[i] ) {
					memmove( &parts[j], &parts[j+1], sizeof(Part*) * (numParts-j-1) );
					numParts--;
				}
			}
		}

		return mole;
	}

	Mole *disassociateCheck() {
		if( worldUpdateCount > nextDisassociate ) {
			// FIND which bond is breaking
			for( int i=0; i<numParts; i++ ) {
				for( int d=0; d<MAX_DOMAINS; d++ ) {
					if( parts[i]->domainBindings[d] && worldUpdateCount >= parts[i]->domainDisassocTime[d] ) {
						// Found a bond that has broken.  We must find all of the parts
						// that are on each side of the bond to create a new mole.
						Mole *newMole = breakBond( parts[i], parts[i]->domainBindings[d] );
						return newMole;
					}
				}
			}
		}
		return 0;
	}

	void updateNextDisassociate() {
		// FIND the disassociate that will happen next and cache it in the mole
		int nextDisassociate = 1 << 30;
		for( int i=0; i<numParts; i++ ) {
			for( int j=0; j<MAX_DOMAINS; j++ ) {
				if( parts[i]->domainDisassocTime[j] ) {
					nextDisassociate = min( nextDisassociate, parts[i]->domainDisassocTime[j] );
				}
			}
		}
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

Mole *worldAddSinglePartMole( int type ) {
	// FIND a place for the new particle
	int i;
	SVec3 pos;
	int isMembraneBound = 0;
	int membraneIndex = 0;
	if( type == 0 ) {
		// This is a membrane bound type
		// @TODO: Make this a general purpose flag
		isMembraneBound = 1;
	}
	
	if( isMembraneBound ) {
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

	Part *p = new Part;
	p->type = type;
	Mole *m = new Mole;
	m->addPart( p );
	m->pos = pos;
	m->membraneBound = isMembraneBound;

	*worldGrid( pos.z, pos.y, pos.x ) = m;

	m->linkToHead();
	return m;
}

Mole *worldFindSoloMole( int type ) {
	for( Mole *m = Mole::moleHead; m; m=m->next ) {
		if( m->numParts == 1 && m->parts[0]->type == type ) {
			return m;
		}
	}
	return 0;
}

int worldCollision( Mole *a, Mole *b ) {
	// TEST to see of any of the parts of molecules a and b have a common binding domain
	for( int ia = 0; ia < a->numParts; ia++ ) {
		Part *ap = a->parts[ia];

		for( int ib = 0; ib < b->numParts; ib++ ) {
			Part *bp = b->parts[ib];

			// Is there a possible binding domain between these two particles?
			for( int da=0; da<MAX_DOMAINS; da++ ) {
				for( int db=0; db<MAX_DOMAINS; db++ ) {
					int disassoc = bindingMatrix[ap->type * MAX_DOMAINS + da][bp->type * MAX_DOMAINS + db];
					if( disassoc ) {
						// If so, are both of these domains available for binding?
						if( ap->isDomainFree(da) && bp->isDomainFree(db) ) {
							// Yes, these molecules can bind.  So merge them.

							// If one of them has a membraneBound component and the other
							// one doesn't then we have to membrane bound the one that doesn't
							if( b->membraneBound && !a->membraneBound ) {
								a->membraneBound = 1;
								a->pos = b->pos;
							}

							for( int bpi=0; bpi < b->numParts; bpi++ ) {
								a->addPart( b->parts[bpi] );
								b->parts[bpi] = 0;
							}
							b->numParts = 0;
							b->unlink();
							*worldGrid( b->pos.z, b->pos.y, b->pos.x ) = 0;
							delete b;

							ap->bindDomain( da, bp, disassoc + worldUpdateCount );
							bp->bindDomain( db, ap, disassoc + worldUpdateCount );

							// UPDATE the next disassociate
							a->updateNextDisassociate();

							return 1;
						}
					}
				}
			}
		}
	}
	return 0;
}

SVec3 worldFindOpenNear( SVec3 pos, int membrane ) {
	// 3D SPRIAL search looking for a place nearby to place the new mole
	// This is not a very efficient elgorithm since it checks some
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

	int partCount[MAX_PART_TYPES] = {0,};

	// CLEAR the draw list
	drawListCount = 0;
	drawListLineCount = 0;

	Mole *m;
	for( m = Mole::moleHead; m; m=m->next ) {
		SVec3 oldPos = m->pos;

		// POP out of the spatial index
		*worldGrid( oldPos.z, oldPos.y, oldPos.x ) = 0;

		// UPDATE position based on random walk
		SVec3 newPos = oldPos;
		if( m->membraneBound ) {
			if( !Cell_freezeMembrane && !zrandI(0,Cell_membraneViscoscity) ) {
				MembraneNeighbor *mn = membraneNeighbor( oldPos.z, oldPos.y, oldPos.x );
				newPos = mn->neighbors[ zrandI(0,mn->count) ];
			}
		}
		else {
			static SVec3 dirs[6] = { SVec3(1,0,0), SVec3(-1,0,0), SVec3(0,1,0), SVec3(0,-1,0), SVec3(0,0,1), SVec3(0,0,-1) };
			newPos.add( dirs[ zrandI(0,6) ] );
		}

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

		else {
			// COLLISION check with other molecules
			Mole *collide = *worldGrid( newPos.z, newPos.y, newPos.x );
			if( collide ) {
				// There is a molecule here.  If they can bind then this
				// Mole becomes the master and incorporates all of the
				// parts from the other one.  The other one is deleted.

				int merged = worldCollision( m, collide );
				if( !merged ) {
					// These molecules collided but were unable to bind so
					// move this particle back to where it was
					posReset = 1;
				}
				else {
					newPos = m->pos;
				}
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

		// CHECK for disassociation
		Mole *newMole = m->disassociateCheck();
		if( newMole ) {
			// A bond broke and a new molecule was created which must be added to the world
			int hasMembraneBoundPart = m->hasMembraneBoundPart();
			if( !hasMembraneBoundPart ) {
				m->membraneBound = 0;
			}

			hasMembraneBoundPart = newMole->hasMembraneBoundPart();
			SVec3 p = worldFindOpenNear( m->pos, hasMembraneBoundPart );
			if( hasMembraneBoundPart ) {
				newMole->membraneBound = 1;
			}
			else {
				newMole->membraneBound = 0;
			}

			*worldGrid( p.z, p.y, p.x ) = newMole;
			newMole->pos = p;
			newMole->linkToHead();

			// UPDATE the next disassociate
			newMole->updateNextDisassociate();
			m->updateNextDisassociate();
		}

		// ADD the parts to the draw list
		SVec3 pos = m->pos;
		for( int i=0; i<m->numParts; i++ ) {
			int type = m->parts[i]->type;
			partCount[type]++;

			drawListVert[drawListCount] = pos;
			drawListColr[drawListCount] = partColorPalette[ type ];
			drawListCount++;

			if( m->numParts >= 3 && i > 0 ) {
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

	// ADD / REMOVE parts as needed
	int desiredCount[MAX_PART_TYPES] = { Cell_aParts, Cell_bParts, Cell_cParts };
	for( int i=0; i<MAX_PART_TYPES; i++ ) {
		while( partCount[i] < desiredCount[i] ) {
			worldAddSinglePartMole( i );
			partCount[i]++;
		}

		while( partCount[i] > desiredCount[i] ) {
			Mole *_m = worldFindSoloMole( i );
			if( !_m ) {
				break;
			}
			if( _m != m ) {
				*worldGrid( _m->pos.z, _m->pos.y, _m->pos.x ) = 0;
				_m->destroy();
				partCount[i]--;
			}
		}
	}

}

void worldBuildPlots() {
//	guiGet(distPlot,GUI2DPlot);
//	if( !distPlot ) {
//		return;
//	}

	// Build a histogram of all the distances
	const int bucketCount = 40;
	double dist[2][bucketCount];
	memset( dist, 0, sizeof(dist) );
	int maxRad = max( DIMZ, max( DIMX, DIMY ) );
	const int step = 2;
//	int i;

	int radius = maxRad/2;
	SVec3 center( radius, radius, radius );
	float bucketCountF = (float)bucketCount;
	float radiusF = (float)radius;

	Mole *m;
	for( m = Mole::moleHead; m; m=m->next ) {
		for( int i=0; i<m->numParts; i++ ) {
			Part *p = m->parts[i];
			if( p->type != 0 ) {
				SVec3 pos = m->pos;
				pos.sub( center );
				int bucket = max( 0, min( bucketCount-1, zmathRoundToInt(bucketCountF * (float)pos.mag() / radiusF) ) );
				dist[p->type-1][bucket]++;
			}
		}
	}


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


//////////////////////////////////////////////////////////////////////////
// render
//////////////////////////////////////////////////////////////////////////

void render() {
	//bindingMatrixSetup0();
	//bindingMatrixSetup1();
	bindingMatrixSetup2();

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

	if( !(zTimeFrameCount % 5) ) {
		worldBuildPlots();
	}
}

void startup() {
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
		if( Cell_near <= 10 ) {
			Cell_near = 2.45e2;
			Cell_far = 2.75e2;
		}
		else {
			Cell_near = 1;
			Cell_far = 2000;
		}
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

