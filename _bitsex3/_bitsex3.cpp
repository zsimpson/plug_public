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
#include "zmat.h"
#include "zrand.h"



#define LAYERS (8)
#define OUTPUTS (4)
#define INPUTS (OUTPUTS*2)
#define CRIT_COUNT (1000)

#define MUTATION_STRENGTH (0.01)

struct NeuralCascade {
	// Each node of each layer is fully connected to the outputs of each previous layer
	// Each layer is a full matrix beause each node is a vector of weights, one weight for each node in prev layer
	// Multiplying the previous set of outputs by this matrix we get the inputs to this layer which is all NAND gates
	// That gives us the output of this layer which becomes the input to the next layer.
	
	ZMat connections[LAYERS];
		// INPUTS x OUTPUTS
	ZMat layerOutputs[LAYERS];
		// OUTPUTS x 1
		
	NeuralCascade() {
		for( int l=0; l<LAYERS; l++ ) {
			connections[l].alloc( INPUTS, OUTPUTS, zmatF64 );
			layerOutputs[l].alloc( OUTPUTS, 1, zmatF64 );
		}
	}

	void random() {
		for( int l=0; l<LAYERS; l++ ) {
			for( int i=0; i<INPUTS; i++ ) {
				for( int j=0; j<OUTPUTS; j++ ) {
					connections[l].putD( i, j, zrandD(-1.0,1.0) );
				}
			}
		}
	}
	
	void copyAndMutate( NeuralCascade &copy, double mutationStrength ) {
		for( int l=1; l<LAYERS; l++ ) {
			for( int i=0; i<INPUTS; i++ ) {
				for( int o=0; o<OUTPUTS; o++ ) {
					double v = copy.connections[l].getD( i, o ) + zrandGaussianF() * mutationStrength; 
					connections[l].putD( i, o, v );
				}
			}
		}
	}
	
	ZMat &getLastLayer() {
		return layerOutputs[LAYERS-1];
	}
	
	double nand( double a, double b ) {
		if( a > 0.0 && b > 0.0 ) {
			return -1.0;
		}
		return 1.0;
	}
	
	void eval( ZMat inputState ) {
		layerOutputs[0].copyDataD( inputState.getPtrD() );
		for( int l=1; l<LAYERS; l++ ) {
			ZMat inputsToThisLayer;
			zmatMul( connections[l], layerOutputs[l-1], inputsToThisLayer );
			for( int i=0; i<OUTPUTS; i++ ) {
				double v = nand( inputsToThisLayer.getD(i*2+0,0), inputsToThisLayer.getD(i*2+1,0) );
				layerOutputs[l].putD( i, 0, v );
			} 
		}
	}
	
};
	
	


struct BitCrit {
	//CascadeLogicNet net;
	NeuralCascade net;
	int points;
	
	BitCrit() {
		points = 50;
	}
	
	void copyAndMutate( BitCrit &copy, double mutationStrength ) {
		net.copyAndMutate( copy.net, mutationStrength );
	}
	
	void random() {
		net.random();
	}

	int eval( ZMat state ) {
		net.eval( state );
		return net.getLastLayer().getD( 0, 0 ) > 0.0 ? 1 : 0;
	}
};



// Version 1:
// Critters are in a well-mixed 0dimensional space
// They randomly encounter bit patterns.
// One of these patterns is food.  If they graze and they are right they get X points.
// but if they graze and they are wrong they lose x/y points.


// A better way to do the breeding is to run a few rounds and then sort
// by score.  Those in some upper fraction get to reproduce at the expense of the bottom fraction
// This way I don't have to delacre an arbitrary amount that is good to reproduce.

// OK, I've established that the little neural net model works reasonably well
// What I was originalyl trying to do when I started this was evolve a pattern recognition
// system in the context of sexual selection. Still somewhat interested in this.  If there's
// crossover of two very different nets then the results will probably work worse than the 
// parents meaning that there is an incentive for a female to recognize her own species.
// 
// In the second version I started thinking of it as recognize food, camoflauge, etc.
// Not sure which of these little experiments I'm actually interested in.
// Does seem like thre's a lot of variations on pattern matching that by be amusing at
// least.  You could have them hunting and grzing and avoiding obstables, etc.
// None of it really shows anything too profound.  
//
// This started as an idea with Thomas to think of how we could generate bird song or some
// aesthetic object without involving humans as the agents of selection.
// Can a flower / bee relationship or a bird/bird relationship be simulated in a way
// that will create interesting results?  I'm afraid that I might just create bit patterns
// that don't have much meaning to us.  Things like flowers and bird coats are running
// through a visual cortex that has certain constraits / manners of processing
// that I can't easily simulate... unless I can think of a clever constrained simplification
//
// But can I use this little toy to explore other things? Mutualism for example?
// The ability to recognize when I parter wants to help you vs exploit.
//
// I'd probably rather spend the time either getting the transmission line model running again
// or getting a multi-dimensional version of the AC stuff working.

void main() {
	BitCrit bitCrits[CRIT_COUNT];

	#define PATTERN_COUNT (1<<OUTPUTS)
	ZMat patterns[PATTERN_COUNT];
	for( int i=0; i<PATTERN_COUNT; i++ ) {
		patterns[i].alloc( OUTPUTS, 1, zmatF64 );
		for( int j=0; j<OUTPUTS; j++ ) {
			patterns[i].putD( j, 0, ( i & (1<<j) ) ? 1.0 : -1.0 );
		}
	}

	for( int i=0; i<CRIT_COUNT; i++ ) {
		bitCrits[i].random();
	}
	
	double mutationStrength = 0.001;
	int turn = 0;
	int lastHighestI = 0;
	while( 1 ) {
		// PICK a random critter
		// Challenge it with a pattern. Some fraction of the time food, some fraction not food
		// If some bit turns on then graze.  Add or subtract to points
		BitCrit &c = bitCrits[ rand() % CRIT_COUNT ];

		// CHOOSE a pattern to present to the critter
		if( c.points < 0.0 ) {
			continue;
		}

		int pat = rand() % PATTERN_COUNT;
		if( c.eval( patterns[pat] ) ) {
			// GRAZE
			if( pat == 5 ) {
				c.points += 10;
			}
			else {
				c.points -= 20;
			}
		}

		if( c.points > 100 ) {
			// REPRODUCE by replacing the lowest scores.
			int lowestScore = 1000000000;
			int lowestI = 0;
			int highestScore = 0;
			int highestI = 0;
			for( int i=0; i<CRIT_COUNT; i++ ) {
				if( bitCrits[i].points < lowestScore ) {
					lowestScore = bitCrits[i].points;
					lowestI = i;
				}
				if( bitCrits[i].points > highestScore ) {
					highestScore = bitCrits[i].points;
					highestI = i;
				}
			}
			lastHighestI = highestI;
			bitCrits[lowestI].copyAndMutate( c, mutationStrength );
			bitCrits[lowestI].points = c.points / 2;
			c.points /= 2;			
		}
		
		turn++;
		
		#define BINS (10)
		int histogram[BINS] = {0,};
		if( !(turn%1000) ) {
			printf( "%d) ", turn );
		
			// HISTOGRAM the points
			for( int i=0; i<CRIT_COUNT; i++ ) {
				int p = bitCrits[i].points;
				int bin = p / BINS;
				if( bin < 0 ) {
					bin = 0;
				}
				else if( bin >= BINS ) {
					bin = BINS-1;
				}
				histogram[bin]++;
			}
			for( int i=0; i<BINS; i++ ) {
				printf( "%03d ", histogram[i] );
			}
			
			printf( "  " );

			// WHAT fraciton of the population has a perfect score?
			if( !(turn%100000) ) {
				int perfectCount = 0;
				for( int j=0; j<CRIT_COUNT; j++ ) {
					BitCrit &h = bitCrits[ j ];
					int score = 0;
					for( int i=0; i<PATTERN_COUNT; i++ ) {
						int eval = h.eval( patterns[i] );
						if( i==5 && eval ) {
							score++;
						}
						else if( i!=5 && !eval ) {
							score++;
						}
					}
					if( score == 16 ) {
						perfectCount++;
					}
				}
				printf( "%d", perfectCount );
			}
						
			printf( "\n" );
		}
	}
}


// I've got to start with some smple system that I know work and then add in more complex brain logic
// 2873 sadier st. after 3. 

//
// Maybe the random logic net isn't working very well
// Is it too brittle?
// I could test that by starting the system with a lot of good units and see if it breaks them instead of improves them
// Maybe I need a squishier funcation basis like a neural net
// Let M be a connection matrix with positive and negative weights.
// Let it be multiplied by the state vector v and then threshold the result

// What over fun eveolutionary systems: the old pursuit one, and artistic one
// that is a transform function of an image in feedback, or a transform of some signal given an interactive signal
// That is, generator + interactive transform
// There's such huge classes of generators, where do you start?
// Maybe recursive geometry + filters?
// Feedback filters, 
// Or take create commons images off of flickr or google and use them as sources
// CAs, fractals, 
// The ones I'm most intellectually interesting in would be camera feedback with a transform
// Camera reads image. Image is transformed by some function, Image is put on screen, repeat.
// Each pixel is given some function graph
// As with all such graphs, they tend to be brittle, use the tricks I used before where you have
// linear combination of non-linear evolving basis functions.
// But I keep trying to come up with somethign like a logic net or a fuzzy net that somehow
// behaves better.  This exerpiment seems to show that hard logic nets are not so great.
// Maybe I can think of weights in a fully connected logic net
// Like what I have, a series cascading NAND gates but every gate is connected to every input
// with a weighting function.  Is this not exactly what the neural nets do?
// I need a signed system. So that -1 is zero and 1 is 1 so that I can get symmetry
// Seems like the system I already have with that neural cascade might be a good place to start.



