// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "stdlib.h"
#include "memory.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zgltools.h"

ZPLUGIN_BEGIN( bitsex );


#define LAYERS (4)
#define OUTPUTS (4)
#define INPUTS (OUTPUTS*2)

struct CascadeLogicNet {
	int connections[LAYERS][INPUTS];
		// Each layer has NODES nodes. Each node is an index
		// as to which output from the previous layer it is sampling
	
	int state[LAYERS][OUTPUTS];

	void init() {
		memset( connections, 0, sizeof(connections) );
	}
		
	void computeState( int startState[OUTPUTS] ) {
		memcpy( &state[0][0], startState, sizeof(int)*OUTPUTS );
		for( int l=1; l<LAYERS; l++ ) {
			// COMPUTE each output as NAND of the input connections
			for( int o=0; o<OUTPUTS; o++ ) {
				state[l][o] = !( state[l-1][ connections[l][o*2+0] ] & state[l-1][ connections[l][o*2+1] ] );
			}
		}
	}

	void print() {
		for( int i=0; i<LAYERS; i++ ) {
			for( int j=0; j<OUTPUTS; j++ ) {
				printf( "%d ", connections[i][j] );
			}
			printf( "\n" );
		}
	}
	
	int *getLastLayer() {
		return &state[LAYERS-1][0];
	}	
};

#define WIDTH (80)
#define HEIGHT (80)

void drawQuad( int color, int x, int y ) {
	switch( color ) {
		case 0: return;
		case 1: glColor3ub( 200, 200, 200 ); break;
		case 2: glColor3ub( 240, 240, 240 ); break;
		case 3: glColor3ub( 240, 200, 200 ); break;
		case 4: glColor3ub( 255, 200, 200 ); break;
	}
	glBegin( GL_QUADS );
		glVertex2f( x+0.1f, y+0.1f );
		glVertex2f( x+0.9f, y+0.1f );
		glVertex2f( x+0.9f, y+0.9f );
		glVertex2f( x+0.1f, y+0.9f );
	glEnd();
}


struct BitCrit {
	int x, y;
	CascadeLogicNet net;
	
	void init() {
		net.init();
	}
	
	void print() {
		int *lastLayer = net.getLastLayer();
		
		for( int i=0; i<OUTPUTS; i++ ) {
			drawQuad( lastLayer[i]+2, (x+i)%WIDTH, y );
		}
	} 
};

#define POOL_SIZE (100)

struct BitWorld {
	char pattern[HEIGHT][WIDTH];

	BitWorld() {
		memset( pattern, 0, sizeof(pattern) );
	}

	void background() {
		// 1/2 the world is alternating 10 and the other is all ones
		int y;
		for( y=0; y<HEIGHT/2; y++ ) {
			for( int x=0; x<WIDTH; x++ ) {
				pattern[y][x] = 2;
			}
		}
		for( ; y<HEIGHT; y++ ) {
			for( int x=0; x<WIDTH; x++ ) {
				pattern[y][x] = ( (x+y)%2 ) + 1;
			}
		}
	}
	
	void print() {
		for( int y=0; y<HEIGHT; y++ ) {
			for( int x=0; x<WIDTH; x++ ) {
				if( pattern[y][x] ) {
					drawQuad( pattern[y][x]-1, x, y );
				}
			}
		}
	}
	
	void insertPattern( char _pattern[], int patternLen, int x, int y ) {
		for(int i=0; i<patternLen; i++ ) {
			pattern[ (x+i)%WIDTH ][y] = _pattern[i];
		}
	} 

};

// Critters move around in the two space and they have a 1D presence in that world
// where they're visibility is 8 bits.  This is their coat pattern which is changable.
// They move around randomly and they can make the following decisions:
// They can "strike".  If they happen to be on another critter they get 100 points but
// if they strike and miss they lose, say, 50.
// They can find some food that is replensihed in the environment.  If they graze and get food
// then they get 10 points but if they miss they lose 1.
// Food is marked as a certain bit pattern in the environment.
// They can change their coat pattern using their logic network to read the background
// and set their coat pattern.
// One bit controls their strike and graze habits. These bits are visible just like the other output bits

// DONE Get world and critters drawing
// Insert food into the world (0000)


#define MAX_CRITS (100)
int bitCritCount = 10;
BitCrit bitCrits[MAX_CRITS];

BitWorld world;
BitWorld food;

void render() {
	zglMatrixFirstQuadrantAspectCorrect();
	glScalef( 1.f/WIDTH, 1.f/HEIGHT, 1.f );

	for( int i=0; i<bitCritCount; i++ ) {
		bitCrits[i].x += (rand()%3)-1;
		if( bitCrits[i].x < 0 ) {
			bitCrits[i].x = WIDTH-1;
		}
		bitCrits[i].x %= WIDTH;

		bitCrits[i].y += (rand()%3)-1;
		if( bitCrits[i].y < 0 ) {
			bitCrits[i].y = HEIGHT-1;
		}
		bitCrits[i].y %= HEIGHT;

		bitCrits[i].print();
	}

	world.print();
	food.print();
}

void startup() {
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

