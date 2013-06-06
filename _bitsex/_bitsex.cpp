// @ZBS {
//		+DESCRIPTION {
//			DIstributed Work Order Server
//		}
//		*MODULE_DEPENDS diwos.cpp
//		*INTERFACE console
// }

// OPERATING SYSTEM specific includes:
// SDK includes:
// STDLIB includes:
#include "stdio.h"
#ifdef WIN32
	#include "conio.h"
	#include "malloc.h"
#endif
// STDLIB includes:
#include "stdlib.h"
#include "time.h"
#include "memory.h"
// MODULE includes:
// ZBSLIB includes:
#include "ztl.h"
#include "zhashtable.h"
#include "zconsole.h"

#define min(a,b) ( a < b ? a : b )
#define max(a,b) ( a > b ? a : b )

int intCompare( const void *a, const void *b ) {
	int _a = *(int *)a;
	int _b = *(int *)b;
	return _b - _a;
}

int listCompare( const void *a, const void *b ) {
	int *_a = (int *)a;
	int *_b = (int *)b;
	return _b[1] - _a[1];
}

char buf[64];
char *bitsToStr( int a ) {
	buf[32] = 0;
	for( int i=0; i<32; i++ ) {
		buf[31-i] = a & (1<<i) ? '1' : '0';
	}
	return buf;
}

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


int mutationRate = 300;
int crossoverRate = 5;

struct BitCrit {
	int x, y;
	CascadeLogicNet net;
	
	void init() {
		net.init();
	}
	
	void print() {
		int *lastLayer = net.getLastLayer();
		zconsoleGotoXY( x, y );
		for( int i=0; i<OUTPUTS; i++ ) {
			printf( "%c", lastLayer[i] ? '1' : '0' );
		}
	} 
};

struct PermutationVec {
	ZTLVec<int> vec;
	
	PermutationVec( int size ) {
		vec.setCount( size );
		for( int i=0; i<size; i++ ) {
			vec[i] = i;
		}
	}
	
	int operator[] ( int x ) { return vec[x]; }
	
	void shuffle() {
		// @TODO, lookup in zlab
	}
	
};

#define POOL_SIZE (100)



#define WIDTH (78)
#define HEIGHT (40)
struct BitWorld {
	char pattern[HEIGHT][WIDTH];

	BitWorld() {
		// 1/2 the world is alternating 10 and the other is all ones
		int y;
		for( y=0; y<HEIGHT/2; y++ ) {
			for( int x=0; x<WIDTH; x++ ) {
				pattern[y][x] = 1;
			}
		}
		for( ; y<HEIGHT; y++ ) {
			for( int x=0; x<WIDTH; x++ ) {
				pattern[y][x] = (x+y)%2;
			}
		}
	}
	
	void print() {
		for( int y=0; y<HEIGHT; y++ ) {
			for( int x=0; x<WIDTH; x++ ) {
				printf( "%c", pattern[y][x] ? '1' : '0' );
			}
			printf( "\n" );
		}
		
		printf( "\n\n" );
	}
};

void main() {
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

	BitWorld world;
	world.print();
	
	#define MAX_CRITS (100)
	int bitCritCount = 10;
	BitCrit bitCrits[MAX_CRITS];

	// Start wit them just diffusing around
	while( 1 ) {
		zconsoleCls();
//		world.print();

		for( int i=0; i<bitCritCount; i++ ) {
			bitCrits[i].x += (rand()%3)-1;
			bitCrits[i].x %= WIDTH;

			bitCrits[i].y += (rand()%3)-1;
			bitCrits[i].y %= HEIGHT;

			bitCrits[i].print();
			
		}
	}
	
}



