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
#define OUTPUTS (8)
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
	
	int *getLastLayer() {
		return &state[LAYERS-1][0];
	}	
};


int mutationRate = 300;
int crossoverRate = 5;

struct BitCrit {
	// the logic units are
	
	// The male is nothing but a bit pattern.
	// The female is a computer which hears a round of males and decides which one to mate with
	// Both are encoded in the same genome
	
	int male[OUTPUTS];
	CascadeLogicNet female;
	int sex;
	
	void init() {
		memset( male, 0, sizeof(male) );
		female.init();
		sex = rand() % 2;
	}
	
	int getMaleBits() {
		int a = 0;
		for( int i=0; i<OUTPUTS; i++ ) {
			if( male[i] ) {
				a |= (1<<i);
			}
		}
		return a;
	}
	
	void printGenome() {
		printf( "%s\n", bitsToStr(getMaleBits()) );
		for( int i=0; i<LAYERS; i++ ) {
			for( int j=0; j<OUTPUTS; j++ ) {
				printf( "%d ", female.connections[i][j] );
			}
			printf( "\n" );
		}
	}
	
	static void mate( BitCrit *a, BitCrit *b, BitCrit *child ) {
		int i;
		
		child->sex = rand() % 2;

		// RANDOM crossover and mutation
		int side = rand() % 2;
		for( i=0; i<OUTPUTS; i++ ) {
			child->male[i] = side == 0 ? a->male[i] : b->male[i];
			if( ! (rand() % crossoverRate) ) {
				side = ! side;
			}
			if( ! ( rand() % mutationRate ) ) {
				child->male[i] ^= 1;
			}
			assert( child->male[OUTPUTS] == 0 );
		}
		for( int l=0; l<LAYERS; l++ ) {
			for( i=0; i<INPUTS; i++ ) {
				child->female.connections[l][i] = side == 0 ? a->female.connections[l][i] : b->female.connections[l][i];
				if( ! (rand() % crossoverRate) ) {
					side = ! side;
				}
				if( ! (rand() % mutationRate) ) {
					child->female.connections[l][i] += (rand()%3)-1;
					child->female.connections[l][i] = max( 0, min( OUTPUTS-1, child->female.connections[l][i] ) );
				}
			}
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

struct Generation {
	BitCrit pool[POOL_SIZE];
	int scores[POOL_SIZE];
	
	Generation() {
		for( int i=0; i<POOL_SIZE; i++ ) {
			pool[i].init();
		}
	}
	
	static void run( Generation *t0, Generation *t1 ) {
		// FOREACH female
		
		// @TODO: Use permutation vector to ensure randomness of position
		
		PermutationVec permutation( POOL_SIZE );
		memset( t1->scores, 0, sizeof(t1->scores) );
		
		int t1Count = 0;
		while( t1Count < POOL_SIZE ) {
			permutation.shuffle();
			for( int i=0; i<POOL_SIZE; i++ ) {
				BitCrit *female = &t0->pool[ permutation[i] ];
				if( female->sex == 0 ) {
					// PICK males at random and run their output through its cascade network
					// Score male by count the number of alternating ones and zeros in last layer output (prevents just turning on all the bits as a solution. nec?)
					// MATE with this male in stochastic proportion to score
					
					int mate = 0;
					BitCrit *male = 0;
					do {
						mate = rand() % POOL_SIZE;
						male = &t0->pool[mate];
					} while( male->sex == 0 );
					
					female->female.computeState( t0->pool[mate].male );
					
					// SCORE
					int score = 0;
					int *lastOutputs = t0->pool[i].female.getLastLayer();
					for( int j=1; j<OUTPUTS; j++ ) {
						if( lastOutputs[j] != lastOutputs[j-1] ) {
							score++;
						}
					}
					t1->scores[mate] = score;
					
					// CHANCE of mating
					if( rand() <= score ) {
						BitCrit::mate( female, male, &t1->pool[t1Count] ); 

						t1Count++;
						if( t1Count == POOL_SIZE ) {
							break;
						}
					}
				}
			}
		}
		
		ZHashTable hash;
		qsort( t1->scores, POOL_SIZE, sizeof(int), intCompare );
		for( int i=0; i<POOL_SIZE; i++ ) {
			printf( "%02d ", t1->scores[i] );
			int maleBits = t1->pool[i].getMaleBits();
			int curCount = hash.bgetI( &maleBits, sizeof(int), 0 );
			hash.bputI( &maleBits, sizeof(int), curCount+1 );
		}
		printf( "\n" );
		
		// MAKE a list of the patterns so that we can sort by prevelance
		int *list = (int *)alloca( POOL_SIZE * 2 * sizeof(int) );
		int i = 0;
		int count = 0;
		for( ZHashWalk n( hash ); n.next(); ) {
			list[i++] = *(int *)n.key;
			list[i++] = *(int *)n.val;
			count++;
		}
		qsort( list, count, sizeof(int)*2, listCompare );

		for( int i=0; i<min(count,5); i++ ) {
			printf( "%s %d\n", bitsToStr(list[i*2+0]), list[i*2+1] );
		}
		
		t1->pool[0].printGenome();

		printf( "\n\n" );
		
	}
};


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

	srand( (unsigned int)time(0) );
	#define NUM_GENERATIONS (2000)
	Generation *generations[ NUM_GENERATIONS ];
	generations[0] = new Generation();
	for( int i=1; i<NUM_GENERATIONS; i++ ) {

		mutationRate = 10;
		crossoverRate = 10;

		printf( "Generation %d\n", i );
		generations[i] = new Generation();
		Generation::run( generations[i-1], generations[i] );
	}

//	BitWorld world;
//	world.print();
	
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
	
}



