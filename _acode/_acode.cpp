// @ZBS {
//		*SDK_DEPENDS gsl-1.8 pthread
//		*EXTRA_DEFINES ZMSG_MULTITHREAD
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "string.h"
#include "math.h"
#include "stdlib.h"
#ifdef WIN32
	#include "malloc.h"
#endif
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zmat.h"
#include "zbittexcache.h"
#include "zbits.h"
#include "zviewpoint.h"
#include "zrand.h"
#include "zchangemon.h"
#include "zmathtools.h"
#include "zgltools.h"
#include "zglfont.h"
#include "ztmpstr.h"
#include "ztl.h"
#include "kineticbase.h"
#include "zrand.h"

#include "zprof.h"

extern int zprofDumpFlag;

ZPLUGIN_BEGIN( acode );

ZVAR( double, Acode_dt, 2.71e-001 );
ZVAR( float, Acode_vizScale, 1.15e-001 );

ZVAR( double, Acode_icS0, 8.63e-002 );
ZVAR( double, Acode_icS1, 8.63e-002 );
ZVAR( double, Acode_icS2, 3.00e-001 );
ZVAR( double, Acode_icS3, 3.00e-001 );
ZVAR( double, Acode_icS4, 3.00e-001 );
ZVAR( double, Acode_icS5, 3.00e-001 );
ZVAR( double, Acode_icS6, 3.00e-001 );
ZVAR( double, Acode_icS7, 3.00e-001 );
ZVAR( double, Acode_icS8, 3.00e-001 );
ZVAR( double, Acode_icS9, 3.00e-001 );

ZVAR( double, Acode_icA, 1.10e+000 );
ZVAR( double, Acode_icI, 1.10e+000 );
ZVAR( double, Acode_rateSignalDecay, 2.55e+000 );
ZVAR( double, Acode_rateActivate, 6.14e+000 );
ZVAR( double, Acode_rateProduction, 2.22e+001 );
ZVAR( double, Acode_rateInhibition, 7.39e+001 );
ZVAR( double, Acode_rateAttack, 7.70e+001 );
ZVAR( double, Acode_rateRNAaseH, 5.09e+000 );
ZVAR( float, Acode_noiseIC, 8e-2 );
ZVAR( float, Acode_diffusion, 1 );


struct CircuitModel {
	int signalCount;
	int gateCount;

	double rates[6];
		#define rateSignalDecay (0)
		#define rateGateActivate (1)
		#define rateWorkingGateProduction (2)
		#define rateInhibition (3)
		#define rateFreeActivatorAttack (4)
		#define rateRNAaseH (5)

	struct Gate {
		int op;
			#define GATEOP_NOT  (1)
			#define GATEOP_NAND (2)
			#define GATEOP_ONE  (3)
		int in0, in1;
		int out;

		Gate( int _op, int _in0, int _in1, int _out ) {
			op = _op;
			in0 = _in0;
			in1 = _in1;
			out = _out;
		}
	};

	ZTLVec<Gate> gates;
	ZTLVec<int> reactionRateIndicies;

	KineticSystem kSystem;

	void compile();

	CircuitModel() {
		signalCount = 0;
		gateCount = 0;
	}
};

void CircuitModel::compile() {
	int i, rateIndex;

	kSystem.clear();
	reactionRateIndicies.clear();

	// The first reagent is a trash bucket representing all discarded reagents
	kSystem.reagentAdd( "trash" );

	// The next signalCount reagents are reserved for the signal lines
	for( i=0; i<signalCount; i++ ) {
		int ri = kSystem.reagentAdd( ZTmpStr("s%d", i) );
			// Signal

		// Every signal has an RNAase path to ground
		kSystem.reactionAdd( ri, -1, 0, -1 );
		rateIndex = rateSignalDecay;
		reactionRateIndicies.add( rateIndex );
	}

	for( i=0; i<gates.count; i++ ) {
		if( gates[i].op == GATEOP_NOT ) {
			int ai = kSystem.reagentAdd( ZTmpStr("a%d", i) );
				// Free activator
			int ii = kSystem.reagentAdd( ZTmpStr("i%d", i) );
				// Inhibited gate
			int wi = kSystem.reagentAdd( ZTmpStr("w%d", i) );
				// Working gate
			int oi = kSystem.reagentAdd( ZTmpStr("o%d", i) );
				// Occupied activator

			// 2
			kSystem.reactionAdd( ai, ii, wi, -1 );
			rateIndex = rateGateActivate;
			reactionRateIndicies.add( rateIndex );

			// 3
			kSystem.reactionAdd( wi, -1, wi, gates[i].out+1 );
			rateIndex = rateWorkingGateProduction;
			reactionRateIndicies.add( rateIndex );

			// 4
			kSystem.reactionAdd( gates[i].in0+1, wi, oi, ii );
			rateIndex = rateInhibition;
			reactionRateIndicies.add( rateIndex );

			// 5
			kSystem.reactionAdd( gates[i].in0+1, ai, oi, -1 );
			rateIndex = rateFreeActivatorAttack;
			reactionRateIndicies.add( rateIndex );

			// 6
			kSystem.reactionAdd( oi, -1, ai, -1 );
			rateIndex = rateRNAaseH;
			reactionRateIndicies.add( rateIndex );
		}

		else if( gates[i].op == GATEOP_NAND ) {
			int ai = kSystem.reagentAdd( ZTmpStr("a%d", i) );
				// Free activator
			int ii = kSystem.reagentAdd( ZTmpStr("i%d", i) );
				// Inhibited gate
			int w0i = kSystem.reagentAdd( ZTmpStr("w0%d", i) );
				// Working gate with nothing bound
			int w1i = kSystem.reagentAdd( ZTmpStr("w1%d", i) );
				// Working gate with one input bound
			int oi = kSystem.reagentAdd( ZTmpStr("o%d", i) );
				// Fully occupied activator
			int oli = kSystem.reagentAdd( ZTmpStr("ol%d", i) );
				// Left half-occupied activator
			int ori = kSystem.reagentAdd( ZTmpStr("or%d", i) );
				// Right half-occupied activator

			// Activation
			// 3
			kSystem.reactionAdd( ai, ii, w0i, -1 );
			rateIndex = rateGateActivate;
			reactionRateIndicies.add( rateIndex );

			// Production
			// 4
			kSystem.reactionAdd( w0i, -1, w0i, gates[i].out+1 );
			rateIndex = rateWorkingGateProduction;
			reactionRateIndicies.add( rateIndex );

			// 5
			kSystem.reactionAdd( w1i, -1, w1i, gates[i].out+1 );
			rateIndex = rateWorkingGateProduction;
			reactionRateIndicies.add( rateIndex );

			// Inhibition
			// 6
			kSystem.reactionAdd( gates[i].in0+1, w0i, w1i, -1 );
			rateIndex = rateInhibition;
			reactionRateIndicies.add( rateIndex );

			// 7
			kSystem.reactionAdd( gates[i].in1+1, w1i, oi, ii );
			rateIndex = rateInhibition;
			reactionRateIndicies.add( rateIndex );

			// Free activator attack
			// 8
			kSystem.reactionAdd( gates[i].in0+1, ai, oli, -1 );
			rateIndex = rateFreeActivatorAttack;
			reactionRateIndicies.add( rateIndex );

			// 9
			kSystem.reactionAdd( gates[i].in1+1, ai, ori, -1 );
			rateIndex = rateFreeActivatorAttack;
			reactionRateIndicies.add( rateIndex );

			// 10
			kSystem.reactionAdd( gates[i].in0+1, ori, oi, -1 );
			rateIndex = rateFreeActivatorAttack;
			reactionRateIndicies.add( rateIndex );

			// 11
			kSystem.reactionAdd( gates[i].in1+1, oli, oi, -1 );
			rateIndex = rateFreeActivatorAttack;
			reactionRateIndicies.add( rateIndex );

			// RNAaseH
			// 12
			kSystem.reactionAdd( oli, -1, ai, -1 );
			rateIndex = rateRNAaseH;
			reactionRateIndicies.add( rateIndex );

			// 13
			kSystem.reactionAdd( ori, -1, ai, -1 );
			rateIndex = rateRNAaseH;
			reactionRateIndicies.add( rateIndex );

			// 14
			kSystem.reactionAdd( oi, -1, ai, -1 );
			rateIndex = rateRNAaseH;
			reactionRateIndicies.add( rateIndex );

			// 15
			kSystem.reactionAdd( w1i, -1, w0i, -1 );
			rateIndex = rateRNAaseH;
			reactionRateIndicies.add( rateIndex );
		}

		else if( gates[i].op == GATEOP_ONE ) {
			int wi = kSystem.reagentAdd( ZTmpStr("w%d", i) );
				// Working gate always active

			// 2
			kSystem.reactionAdd( wi, -1, wi, gates[i].out+1 );
			rateIndex = rateWorkingGateProduction;
			reactionRateIndicies.add( rateIndex );
		}
	}
}



//---------------------------------------------------------------------------------------------------------------------------

struct CircuitIntegrator {
	CircuitModel *cModel;
	KineticVMCodeD *vmd;
		// Not owned

	KineticIntegrator *integrator;
		// Owned

	KineticTrace integrationResults;
		// The first col of this is the initial conditions

	void setSignalInitCond( int signalIndex, double val );
	void setActivatorInitCond( double val );
	void setInhibitedInitCond( double val );

	void integrateAlloc();
	void integratePrepare();
	void integrateTick( double dur );

	void init( CircuitModel *_cModel, KineticVMCodeD *_vmd ) {
		cModel = _cModel;
		vmd = _vmd;
	}

	void set( int r, double v ) {
		integrationResults.getLastColPtr()[ r ] = v;
		integrator->setC( r, v );
	}

	CircuitIntegrator() {
		cModel = 0;
		vmd = 0;
		integrator = 0;
	}

	~CircuitIntegrator() {
		if( integrator ) {
			delete integrator;
		}
	}
};

void CircuitIntegrator::setSignalInitCond( int signalIndex, double val ) {
	integrationResults.set( 0, signalIndex+1, val );
}

void CircuitIntegrator::setActivatorInitCond( double val ) {
	int signalCount = cModel->signalCount;
	for( int i=signalCount+1; i<cModel->kSystem.reagentCount(); i++ ) {
		if( cModel->kSystem.reagentGet(i)[0] == 'a' ) {
			integrationResults.set( 0, i, val );
		}
	}
}

void CircuitIntegrator::setInhibitedInitCond( double val ) {
	int signalCount = cModel->signalCount;
	for( int i=signalCount+1; i<cModel->kSystem.reagentCount(); i++ ) {
		if( cModel->kSystem.reagentGet(i)[0] == 'i' ) {
			integrationResults.set( 0, i, val );
		}
	}
}

void CircuitIntegrator::integrateAlloc() {
	// INIT the experiment
	if( integrator ) {
		delete integrator;
	}
	integrator = new KineticIntegrator( KI_ROSS, vmd );

	int reagentCount = cModel->kSystem.reagentCount();
	int reactionCount = cModel->kSystem.reactionCount();

	// ALLOCATE space in the integrationResults for the initial conditions
	integrationResults.init( reagentCount );
	double *zero = (double *)alloca( sizeof(double) * reagentCount );
	memset( zero, 0, sizeof(double) * reagentCount );
	integrationResults.add( 0.0, zero );
}

void CircuitIntegrator::integratePrepare() {
	// PREPARE the integrator for stepping
	int reagentCount = cModel->kSystem.reagentCount();
	int reactionCount = cModel->kSystem.reactionCount();

	// SETUP the reaction rates for each reaction based on the type
	double *reactionRates = (double *)alloca( reactionCount * sizeof(double) );
	for( int i=0; i<reactionCount; i++ ) {
		reactionRates[i] = cModel->rates[ cModel->reactionRateIndicies[i] ];
	}

	integrator->prepareD( integrationResults.getColPtr(0), reactionRates, 10000.0 );
}

void CircuitIntegrator::integrateTick( double dur ) {
	integrator->stepD( dur );
	integrationResults.add( integrator->getTime(), integrator->getC() );
}


//---------------------------------------------------------------------------------------------------------------------------

struct SpatialCircuit1D {
	CircuitModel *cModel;
		// not owned

	KineticVMCodeD *vmd;
		// owned and common to all the circuit integrators

	#define SPACE_DIM (64)
	#define TIME_DIM (128)
	CircuitIntegrator buckets[ SPACE_DIM ];

	double diffusion;

	#define VIEW_MAX (32)
		// maximum number of reagents that can be viewed
	ZBits viewBits[ VIEW_MAX ];
	int *texCache[ VIEW_MAX ];

	double activatorInitCond;
	double inhibitedInitCond;
	ZMat signalInitCond;
		// This is a signalCount by SPACE_DIM

	void init( CircuitModel *cModel );
	void simulate( double dt );
	void makeVisualization( float vizScale );
	void render();

	void loadICUniform( int reagent, double value, double variance );
	void loadICSpot( double x, double width, int reagent, double value, double variance );
	void loadICGradient( int reagent, double value0, double value1, double variance );

	SpatialCircuit1D() {
		cModel = 0;
		vmd = 0;
		for( int i=0; i<VIEW_MAX; i++ ) {
			texCache[i] = 0;
		}
	}

	~SpatialCircuit1D() {
		if( vmd ) {
			delete vmd;
		}
		for( int i=0; i<VIEW_MAX; i++ ) {
			zbitTexCacheFree( texCache[i] );
		}
	}
};

void SpatialCircuit1D::init( CircuitModel *cModel ) {
	this->cModel = cModel;

	if( vmd ) {
		delete vmd;
	}
	vmd = new KineticVMCodeD( &cModel->kSystem, cModel->kSystem.reactionCount() );

	// SETUP the integrators for each bucket
	for( int x=0; x<SPACE_DIM; x++ ) {
		buckets[x].init( cModel, vmd );
	}

	// ALLOCATE init cond
	signalInitCond.alloc( cModel->signalCount, SPACE_DIM, zmatF64 );
}

void SpatialCircuit1D::simulate( double dt ) {
	int x;

	int reagentCount = buckets[0].cModel->kSystem.reagentCount();
	int signalCount = buckets[0].cModel->signalCount;

	// @TODO: This is both crashing and taking forewver, I have to separate
	// out the compilation stage so that I can hold it independently

	zprofBeg( sc1d_setup );
	for( x=0; x<SPACE_DIM; x++ ) {
		// ALLOC for the init conds
		buckets[x].integrateAlloc();

		// LOAD ICs
		for( int i=0; i<signalCount; i++ ) {
			buckets[x].setSignalInitCond( i, signalInitCond.getD(i,x) );
		}
		buckets[x].setActivatorInitCond( activatorInitCond );
		buckets[x].setInhibitedInitCond( inhibitedInitCond );

		// PREPARE for intergation (loads the integrator and preps the VM)
		buckets[x].integratePrepare();
	}
	zprofEnd();

	for( int t=0; t<TIME_DIM; t++ ) {
		// KINETICS
		zprofBeg( sc1d_tick );
		for( x=0; x<SPACE_DIM; x++ ) {
			buckets[x].integrateTick( dt );
		}
		zprofEnd();

		// DIFFUSION
		zprofBeg( sc1d_diffuse );
		for( int r=0; r<reagentCount; r++ ) {
			double delta[ SPACE_DIM ] = { 0, };

			char c = buckets[0].cModel->kSystem.reagentGet(r)[0];
			if( c=='s' || c=='a' || c=='o' ) {
				for( x=0; x<SPACE_DIM; x++ ) {
					double a = buckets[ (x - 1 + SPACE_DIM) % SPACE_DIM ].integrationResults.getLastColPtr()[ r ];
					double b = buckets[  x                              ].integrationResults.getLastColPtr()[ r ];
					double c = buckets[ (x + 1            ) % SPACE_DIM ].integrationResults.getLastColPtr()[ r ];

					delta[x] = dt * diffusion * (a + c - b - b);
				}

				for( x=0; x<SPACE_DIM; x++ ) {
					double a = buckets[x].integrationResults.getLastColPtr()[ r ] + delta[x];
					a = max( 0.0, a );
					buckets[x].set( r, a );
				}
			}
		}
		zprofEnd();
	}
}

void SpatialCircuit1D::makeVisualization( float vizScale ) {
	int signalCount = buckets[0].cModel->signalCount;

	// COPY into the visualization buffers and texture
	for( int i=1; i<=signalCount; i++ ) {
//	for( int i=0; i<cModel->kSystem.reagentCount; i++ ) {
		viewBits[i].clear();
		viewBits[i].createCommon( SPACE_DIM, TIME_DIM, ZBITS_COMMON_LUM_FLOAT, 1, 1 );

		for( int t=0; t<TIME_DIM; t++ ) {
			float *dst = viewBits[i].lineFloat( t );
			for( int x=0; x<SPACE_DIM; x++ ) {
				*dst++ = (float)buckets[x].integrationResults.getColPtr(t)[ i ];
			}
		}

		// @TODO: This may need to be spearated so that you can change vizualization without reloading
		texCache[i] = zbitTexCacheLoadFloatScale( &viewBits[i], 0, vizScale );
	}
}

void SpatialCircuit1D::render() {
	int i;
	int signalCount = buckets[0].cModel->signalCount;

	glPushMatrix();

	for( i=1; i<=signalCount; i++ ) {
//	for( i=0; i<cModel->kSystem.reagentCount; i++ ) {
		zbitTexCacheRender( texCache[i] );

		glPushMatrix();
		glScalef( 0.05f, 0.05f, 0.05f );
		zglFontPrint( cModel->kSystem.reagentGet(i), 0.f, -12.f, "controls" );
		glPopMatrix();

		float offset = max( 2.f, (float)SPACE_DIM * 1.05f );
		glTranslatef( offset, 0.f, 0.f );
	}
	glPopMatrix();

	// LINE plot
//	for( i=0; i<cModel->kSystem.reagentCount(); i++ ) {
//		glColor3ub( (i&1)*255, (i&2)*255, (i&4)*255 );
//		if( i >= 8 ) {
//			glColor3ub( (i&1)*128+128, (i&2)*128+128, (i&4)*128+128 );
//		}
//		else {
//			glColor3ub( (i&1)*255, (i&2)*255, (i&4)*255 );
//		}
//		glBegin( GL_LINE_STRIP );
//		for( double t=0.0; t<buckets[0].integrationResults.getLastTime(); t+=0.1 ) {
//			double y = buckets[0].integrationResults.getElemLERP( t, i );
//			glVertex2d( t, y );
//		}
//		glEnd();
//
//		glPushMatrix();
//		zglPixelMatrixFirstQuadrant();
//		zglFontPrint( cModel->kSystem.reagents[i], 0.f, (float)i*12.f, "controls" );
//		glPopMatrix();
//	}
}

void SpatialCircuit1D::loadICUniform( int reagent, double value, double variance ) {
	for( int x=0; x<SPACE_DIM; x++ ) {
		double v = signalInitCond.getD( reagent, x );
		double add = value + variance * (double)zrandGaussianF();
		add = max( 0.0, add );
		signalInitCond.putD( reagent, x, v + add );
	}
}

void SpatialCircuit1D::loadICSpot( double x, double width, int reagent, double value, double variance ) {
	int wi = (int)( (double)SPACE_DIM * (width / 2.0) );
	int xi = (int)( (double)SPACE_DIM * x );
	int x0 = ( xi - wi + SPACE_DIM ) % SPACE_DIM;
	int x1 = ( xi + wi + SPACE_DIM ) % SPACE_DIM;

	if( x0 <= x1 ) {
		for( int x=x0; x<=x1; x++ ) {
			double v = signalInitCond.getD( reagent, x );
			double add = value + variance * (double)zrandGaussianF();
			add = max( 0.0, add );
			signalInitCond.putD( reagent, x, v + add );
		}
	}
}

void SpatialCircuit1D::loadICGradient( int reagent, double value0, double value1, double variance ) {
	for( int x=0; x<SPACE_DIM; x++ ) {
		double v = signalInitCond.getD( reagent, x );
		double add = (value1-value0) * ((double)x/(double)SPACE_DIM) + value0;
		add += variance * (double)zrandGaussianF();
		add = max( 0.0, add );
		signalInitCond.putD( reagent, x, v + add );
	}
}

// Experiments
//---------------------------------------------------------------------------------------------------------------------------

CircuitModel *cModel = 0;
SpatialCircuit1D *spatialCircuit = 0;

void setupBistable() {
	cModel = new CircuitModel;
	cModel->signalCount = 2;
	cModel->gates.add( CircuitModel::Gate(GATEOP_NOT,0,-1,1) );
	cModel->gates.add( CircuitModel::Gate(GATEOP_NOT,1,-1,0) );
	cModel->compile();

	spatialCircuit = new SpatialCircuit1D;
	spatialCircuit->init( cModel );
	spatialCircuit->loadICUniform( 0, Acode_icS0, Acode_noiseIC );
	spatialCircuit->loadICUniform( 1, Acode_icS1, Acode_noiseIC );
}

void setupRing() {
	cModel = new CircuitModel;
	cModel->signalCount = 3;
	cModel->gates.add( CircuitModel::Gate(GATEOP_NOT,0,-1,1) );
	cModel->gates.add( CircuitModel::Gate(GATEOP_NOT,1,-1,2) );
	cModel->gates.add( CircuitModel::Gate(GATEOP_NOT,2,-1,0) );
	cModel->compile();

	spatialCircuit = new SpatialCircuit1D;
	spatialCircuit->init( cModel );
	spatialCircuit->loadICUniform( 0, Acode_icS0, Acode_noiseIC );
	spatialCircuit->loadICUniform( 1, Acode_icS1, Acode_noiseIC );
	spatialCircuit->loadICUniform( 2, Acode_icS2, Acode_noiseIC );
}

void render() {
	glScalef( 0.01f, 0.01f, 0.01f );
	zviewpointSetupView();
	spatialCircuit->render();
}

void visualize() {
	spatialCircuit->makeVisualization( Acode_vizScale );
}

void simulate() {
	zmathDeactivateFPExceptions();

//zprofBeg( circuit_sim );
//for( int i=0; i<10; i++ ) {
	zprofBeg( circuit_setup );
	if( spatialCircuit ) {
		delete spatialCircuit;
		spatialCircuit = 0;
	}

	if( cModel ) {
		delete cModel;
		cModel = 0;
	}

	zrandSeed( 1 );

//	setupBistable();
	setupRing();

	spatialCircuit->activatorInitCond = Acode_icA;
	spatialCircuit->inhibitedInitCond = Acode_icI;
	spatialCircuit->cModel->rates[ rateSignalDecay ] = Acode_rateSignalDecay;
	spatialCircuit->cModel->rates[ rateGateActivate ] = Acode_rateActivate;
	spatialCircuit->cModel->rates[ rateWorkingGateProduction ] = Acode_rateProduction;
	spatialCircuit->cModel->rates[ rateInhibition ] = Acode_rateInhibition;
	spatialCircuit->cModel->rates[ rateFreeActivatorAttack ] = Acode_rateAttack;
	spatialCircuit->cModel->rates[ rateRNAaseH ] = Acode_rateRNAaseH;
	spatialCircuit->diffusion = Acode_diffusion;
	zprofEnd();

	zprofBeg( circuit_simulate );
	spatialCircuit->simulate( Acode_dt );
	zprofEnd();
//}
//zprofEnd();
//zprofDumpFlag = 1;
}

void startup() {
	simulate();
	visualize();
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		simulate();
		visualize();
	}
	if( zmsgIs(type,Key) && zmsgIs(which,w) ) {
		visualize();
	}
}



ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

