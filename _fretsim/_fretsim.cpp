// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "memory.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "fft.h"
#include "zviewpoint.h"
#include "zrand.h"
#include "zmathtools.h"
#include "zmat.h"
#include "zmat_clapack.h"
#include "zmat_gsl.h"
#include "ztl.h"
#include "fftgsl.h"

ZPLUGIN_BEGIN( fretsim );


ZVARB( double, Fretsim_aToB0, 1.20e-002, 0.00e+000, 5.00e-001 );
ZVARB( double, Fretsim_bToA0, 7.20e-003, 0.00e+000, 5.00e-001 );
ZVARB( double, Fretsim_aToB1, 1.27e-001, 0.00e+000, 5.00e-001 );
ZVARB( double, Fretsim_bToA1, 8.35e-003, 0.00e+000, 5.00e-001 );

ZVAR( double, Fretsim_eigenVal0, 0.0 );
ZVAR( double, Fretsim_eigenVal1, 0.0 );
ZVAR( int, Fretsim_whichSVD, 0 );


int randStateFromDistribution( ZMat &distribution, int col ) {
	// Given the column in the distribution, pick a random state
	int r;
	double sum = 0.0;
	for( r=0; r<distribution.rows; r++ ) {
		sum += distribution.getD( r, col );
	}
	double randVal = zrandD( 0.0, sum );
	sum = 0.0;
	for( r=0; r<distribution.rows; r++ ) {
		sum += distribution.getD( r, col );
		if( sum > randVal ) {
			return r;
		}
	}
	return distribution.rows - 1;
}

ZMat *fftRowsPowerSpectrum( ZMat *mat ) {
	// FFT each row generating a power spectrum as a row vector for each column
	FFTGSL fft;
	ZMat *spectrum = new ZMat( mat->rows, mat->cols / 2, zmatF64 );

	for( int r=0; r<mat->rows; r++ ) {
		fft.fftD( (double *)mat->mat, mat->cols, mat->rows * mat->elemSize );
		for( int c=0; c<spectrum->cols; c++ ) {
			double real, imag;
			fft.getComplexD( c, real, imag );
			spectrum->setD( r, c, real*real + imag*imag );
		}
	}

	return spectrum;
}

struct SingleMoleculeModel {
	ZMat markovMat;
	ZMat obsMat;
	ZMat markovEigenVec;
	ZMat markovEigenVal;

	void initMarkov( int stateCount ) {
		markovMat.alloc( stateCount, stateCount, zmatF64 );
	}

	void setMarkov( int srcState, int dstState, double val ) {
		markovMat.setD( dstState, srcState, val );
	}

	void completeMarkov() {
		// CHECK that all of the columns sum to less than one
		for( int c=0; c<markovMat.cols; c++ ) {
			double sum = 0.0;
			for( int r=0; r<markovMat.rows; r++ ) {
				sum += markovMat.getD( r, c );
			}
			assert( sum <= 1.0 );
			markovMat.setD( c, c, 1.0 - sum );
		}

		// EIGEN FACTOR the matrix to get the equilibrium
		zmatEigen_CLAPACK( markovMat, markovEigenVec, markovEigenVal );
		// @TODO: I have to compare this against matlab because there are complex eigen vals and I don't thin kit is handling them right

		Fretsim_eigenVal0 = markovEigenVal.getD(0,0);
		Fretsim_eigenVal1 = markovEigenVal.getD(1,0);
//		assert( markovEigenVal.getD(0,0) == 1.0 );
			// The first eigen val should be 1.0 on a Markov Matrix
			// @TODO: It might be +- epsilon

	}

	void initObs( int obsCount ) {
		obsMat.alloc( obsCount, markovMat.cols+1, zmatF64 );
	}

	void setObs( int obs, int srcState, double val ) {
		assert( obs >= 0 && obs < obsMat.rows );
		assert( srcState >= 0 && srcState < markovMat.cols+1 );
		obsMat.setD( obs, srcState, val );
	}

	ZMat *simulate( int ticks ) {
		ZMat *trace = new ZMat;

		int stateCount = markovMat.rows;
		int obsCount = obsMat.rows;
		trace->alloc( obsCount, ticks, zmatF64 );

		// START in a random state proportional to the equilibirum
		int state = randStateFromDistribution( markovEigenVec, 0 );

		for( int tick=0; tick<ticks; tick++ ) {
			// COMPUTE the observables from the current state and write into the trace
			for( int o=0; o<obsCount; o++ ) {
				// The observable is the fraction from the current state plus the background (homogenous matrix)
				double obsVal = obsMat.getD( o, state ) + obsMat.getD( o, stateCount );
				trace->setD( o, tick, obsVal );
			}

			// Decide which state to go into
			state = randStateFromDistribution( markovMat, state );
		}

		return trace;
	}
};


SingleMoleculeModel model0;
SingleMoleculeModel model1;

struct Trace {
	ZMat *timeTrace;
	ZMat *powerSpectrum;
	int srcModel;

	Trace() {
		timeTrace = 0;
		powerSpectrum = 0;
		srcModel = 0;
	}

	~Trace() {
		if( timeTrace ) delete timeTrace;
		if( powerSpectrum ) delete powerSpectrum;
	}
};

ZTLPVec<Trace> traces;

void setupTest1() {
	// A <=> B
	model0.initMarkov( 2 );
	model0.setMarkov( 0, 1, Fretsim_aToB0 );
	model0.setMarkov( 1, 0, Fretsim_bToA0 );
	model0.completeMarkov();

	model0.initObs( 1 );
	model0.setObs( 0, 0, 0.99 ); // mostly watching A
	model0.setObs( 0, 1, 0.00 ); // not watching B
	model0.setObs( 0, 2, 0.00 ); // background


	// A <=> B
//	model1.initMarkov( 2 );
//	model1.setMarkov( 0, 1, Fretsim_aToB1 );
//	model1.setMarkov( 1, 0, Fretsim_bToA1 );
//	model1.completeMarkov();
//
//	model1.initObs( 1 );
//	model1.setObs( 0, 0, 0.99 ); // mostly watching A
//	model1.setObs( 0, 1, 0.00 ); // not watching B
//	model1.setObs( 0, 2, 0.00 ); // background

}

void render() {
	int i, f;
	setupTest1();

	// RUN simulations.  This is like gathering data from some large number of molecules
	// @TODO: This will eventually be polluted by multiple different kinds of systems
	traces.clear();
	for( i=0; i<100; i++ ) {
		Trace *trace = new Trace;

		SingleMoleculeModel *model = 0;
//		if( zrandI(0,10) < 5 ) {
			model = &model0;
			trace->srcModel = 0;
//		}
//		else {
//			model = &model1;
//			trace->srcModel = 1;
//		}

		// Simulate 128 ticks.  Use a power of two to keep the FFT fast
		trace->timeTrace = model->simulate( 128 );

		// GENERATE power spectrum because we don't care about phase
		trace->powerSpectrum = fftRowsPowerSpectrum( trace->timeTrace );

		traces.add( trace );
	}

	// MAKE a powerspectrum vs experiment matrix for the first observable
	int freqCount = traces[0]->powerSpectrum->cols;
	int expCount = traces.count;
	ZMat allSpectrumMat( expCount, freqCount, zmatF64 );
	for( f=0; f<freqCount; f++ ) {
		for( int e=0; e<expCount; e++ ) {
			allSpectrumMat.putD( e, f, traces[e]->powerSpectrum->getD( 0, f ) );
		}
	}

	zviewpointSetupView();

//	for( i=0; i<traces.count; i++ ) {
//		glTranslatef( 0.f, 1.f, 0.f );
//		glBegin( GL_LINE_STRIP );
//		for( int t=0; t<traces[i]->timeTrace->cols; t++ ) {
//			double v = traces[i]->timeTrace->getD( 0, t );
//			glVertex2d( (double)t, v );
//		}
//		glEnd();

//		glBegin( GL_LINE_STRIP );
//		for( int t=0; t<traces[i]->powerSpectrum->cols; t++ ) {
//			double v = traces[i]->powerSpectrum->getD( 0, t );
//			glVertex2d( (double)t, log( v + 1.0 ) );
//		}
//		glEnd();
//	}

	glColor3ub( 0, 0, 0 );
	glBegin( GL_LINES );
		glVertex2f( 0.f, 0.f );
		glVertex2f( 128.f, 0.f );
		glVertex2f( 0.f, 0.f );
		glVertex2f( 0.f, 128.f );
	glEnd();

	// DRAW the average power spectrum for each model
	for( int m=0; m<1; m++ ) {
		if( m == 0 ) glColor3ub( 255, 255, 255 );
		if( m == 1 ) glColor3ub( 100, 255, 100 );
		glBegin( GL_LINE_STRIP );
		for( f=0; f<traces[0]->powerSpectrum->cols; f++ ) {
			double sum = 0.0;
			int count = 0;
			for( int i=0; i<traces.count; i++ ) {
				if( traces[i]->srcModel == m ) {
					sum += traces[i]->powerSpectrum->getD( 0, f );
					count++;
				}
			}
			glVertex2d( (double)f, log( sum/(double)count + 1.0 ) );
		}
		glEnd();
	}


	// PLOT the first eigen vector
	ZMat uMat, sVec, vtMat;
	zmatSVD_GSL( allSpectrumMat, uMat, sVec, vtMat );

//	for( int i=1; i<sVec.rows; i++ ) {
//		sVec.putD( i, 0, 0.0 );
//	}
//	ZMat sDiagMat;
//	zmatVectorToDiagonalMatrix( sVec, sDiagMat );
//
//	ZMat usMat, newMat;
//	zmatMul( uMat, sDiagMat, usMat );
//	zmatMul( usMat, vtMat, newMat );


	glColor3ub( 255, 0, 0 );
	glBegin( GL_LINE_STRIP );
	for( f=0; f<vtMat.rows; f++ ) {
		double v = fabs( vtMat.getD(Fretsim_whichSVD,f) * sVec.getD(Fretsim_whichSVD) );
		glVertex2d( (double)f, log( v + 1.0 ) );
	}
	glEnd();

	// COMPUTE the covariance
//	ZMat covarMat;
//	ZMat allSpectrumTransposeMat;
//	zmatTranspose( allSpectrumMat, allSpectrumTransposeMat );
//	zmatMul( allSpectrumTransposeMat, allSpectrumMat, covarMat );
//	ZMat pcaVec;
//	ZMat pcaVal;
//	zmatEigen_CLAPACK( covarMat, pcaVec, pcaVal );
//
//	glColor3ub( 0, 0, 255 );
//	glBegin( GL_LINE_STRIP );
//	for( f=0; f<pcaVec.cols; f++ ) {
//		double v = sqrt( fabs( pcaVec.getD(Fretsim_whichSVD,f) * pcaVal.getD(Fretsim_whichSVD) ) );
//		glVertex2d( (double)f, log( v + 1.0 ) );
//	}
//	glEnd();
}

void startup() {


	// A -> B <=> C
//	model.initMarkov( 3 );
//	model.setMarkov( 0, 1, 0.1 );
//	model.setMarkov( 1, 2, 0.4 );
//	model.setMarkov( 2, 1, 0.1 );
//	model.completeMarkov();
//
//	model.initObs( 1 );
//	model.setObs( 0, 0, 0.00 ); // not watching state A at all
//	model.setObs( 0, 1, 1.00 ); // mostly watching state B
//	model.setObs( 0, 2, 0.00 ); // but some is from C
//	model.setObs( 0, 3, 0.00 ); // background


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

