// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "memory.h"
#include "assert.h"
#include "string.h"
#include "float.h"
#include "math.h"
#include "stdio.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zgltools.h"
#include "zviewpoint.h"
#include "zmathtools.h"
#include "zchangemon.h"

ZPLUGIN_BEGIN( che );

ZVARB( int, Che_zDistributed, 0, 0, 1 );
ZVARB( int, Che_divs, 10, 1, 100 );
ZVAR( int, Che_simSteps, 20598 );

ZVAR( double, Che_dt, 1.00e-003 );
ZVAR( double, Che_k1, 5.00e+001 );
ZVAR( double, Che_k2, 1.00e+008 );
ZVAR( double, Che_k3, 5.00e+006 );
ZVAR( double, Che_k4, 5.00e-001 );
ZVAR( double, Che_k5, 3.00e+001 );
ZVAR( double, Che_k6, 1.00e-001 );
ZVAR( double, Che_k7, 5.00e+006 );
ZVAR( double, Che_k8, 1.90e+001 );
ZVAR( double, Che_k9, 1.00e-001 );
ZVAR( double, Che_a_u, 5.00e-006 );
ZVAR( double, Che_y_u, 1.79e-005 );
ZVAR( double, Che_z_u, 1.10e-006 );
ZVAR( double, Che_m_u, 5.80e-006 );
ZVAR( double, Che_yDiff, 1.00e-011 );
ZVAR( double, Che_zDiff, 6.00e-012 );


ZVAR( int, Che_plotSkip, 10 );
ZVAR( double, Che_plotScaleYp, 1.70e+007 );
ZVAR( double, Che_plotScaleT, 2.41e+001 );
ZVAR( double, Che_plotScaleX, 1.68e+001 );
ZVAR( double, Che_plotScaleBeta, 7.00e+002 );
ZVAR( double, Che_plotZIntersectPlane, 3.60e-004 );

ZVAR( double, Che_inputBetaDC, 0.02 );
ZVAR( double, Che_inputBetaPeriod, 4.0 );
ZVAR( double, Che_inputBetaAmp, 0.01 );


// Correct diffusioun in but not the time step must be tiny
// Fix the code so that it doesn't alloacte so much (only what's needed to plot)
// Also make it so I can flip into 0-D interactive mode.

// Why as divs increases is there less Yp?
// Something not right. Slim occupancy shows more than 100% as beta goes to 1
// Error corrector
// FFT on output (Not really needed I think)
// Put motors along the length? (Advanced)
// 
// Experiments:
// Reproduce the static data Fig 5 in Berg 2002 with both distributed and not.
// With distributed it reproduces fine
// Without distribuited it seems like the diffusion const for y is too low

ZChangeMon changeMon;

struct CheState {
	double a_u;
	double a_p;
	double y_u;
	double y_p;
	double z_u;
	double z_yp;
	double m_u;
	double m_yp;
	double beta;
};

struct CheReaction {
	CheState state[2];

	void init() {
		memset( state, 0, sizeof(state) );
	}

	void step( double dt ) {
		// Auto phosphorylation of A dependant on BETA
		double k1 = Che_k1 * state[0].beta * state[0].a_u * dt;
		state[1].a_u -= k1;
		state[1].a_p += k1;
		
		// A handing its phosphoryl to Y
		double k2 = Che_k2 * state[0].y_u * state[0].a_p * dt;
		state[1].a_p -= k2;
		state[1].a_u += k2;
		state[1].y_u -= k2;
		state[1].y_p += k2;

		// Z and YP binding
		double k3 = Che_k3 * state[0].z_u * state[0].y_p * dt;
		state[1].y_p  -= k3;
		state[1].z_u  -= k3;
		state[1].z_yp += k3;

		// Z_YP breaking down without catalyinzg the dephorsphorylation
		double k4 = Che_k4 * state[0].z_yp * dt;
		state[1].y_p  += k4;
		state[1].z_u  += k4;
		state[1].z_yp -= k4;

		// Z_YP breaking down having "successfully" catalyzed the dephorsphorylation
		double k5 = Che_k5 * state[0].z_yp * dt;
		state[1].y_u += k5;
		state[1].z_u += k5;
		state[1].z_yp -= k5;

		// Y spontaneously dephorsphorylating
		double k6 = Che_k6 * state[0].y_p * dt;
		state[1].y_u += k6;
		state[1].y_p -= k6;

		// Motor binding to the Y_P
		double k7 = Che_k7 * state[0].m_u * state[0].y_p * dt;
		state[1].m_u  -= k7;
		state[1].y_p  -= k7;
		state[1].m_yp += k7;

		// Motor disassocating with Y_P
		double k8 = Che_k8 * state[0].m_yp * dt;
		state[1].y_p  += k8;
		state[1].m_u  += k8;
		state[1].m_yp -= k8;

		// Motor disassocating with Y_P and simultaneously dephorsphorylating it
		double k9 = Che_k9 * state[0].m_yp * dt;
		state[1].y_u  += k9;
		state[1].m_u  += k9;
		state[1].m_yp -= k9;
	}

	void advanceState() {
		state[0] = state[1];
		//memset( &state[1], 0, sizeof(state[1]) );
	}
};

// Plotting in general
// I want to be able to make a single call to make a plot. 
// Also a ZUI wrapper around that would be nice
// Should be able to point a graph at at series of data (int, double, etc)
// Should it convert it always to a double?
// Each kind of plot needs different tings but data series tend to be the same

struct PlotSeries {
	int type;
		#define PS_1S (1)
		#define PS_1U (2)
		#define PS_2S (3)
		#define PS_2U (4)
		#define PS_4S (5)
		#define PS_4U (6)
		#define PS_4F (7)
		#define PS_8F (8)
	void *ptr;
	int count;
	int pitch;
	double minY;
	double maxY;

	void setup( int _pitch ) {
		static int pitchLookup[] = { 1, 1, 2, 2, 4, 4, 4, 8 };
		pitch = _pitch==0 ? pitchLookup[_pitch] : _pitch;
		minY = DBL_MAX;
		maxY = -DBL_MAX;
		for( int i=0; i<count; i++ ) {
			double val = getD(i);
			minY = min( minY, val );
			maxY = max( maxY, val );
		}
	}
	PlotSeries( char *_ptr, int _count, int _pitch=0 ) { ptr = _ptr; count=_count; type=PS_1S; setup(_pitch); }
	PlotSeries( short *_ptr, int _count, int _pitch=0 ) { ptr = _ptr; count=_count; type=PS_2S; setup(_pitch); }
	PlotSeries( int *_ptr, int _count, int _pitch=0 ) { ptr = _ptr; count=_count; type=PS_4S; setup(_pitch); }
	PlotSeries( float *_ptr, int _count, int _pitch=0 ) { ptr = _ptr; count=_count; type=PS_4F; setup(_pitch); }
	PlotSeries( double *_ptr, int _count, int _pitch=0 ) { ptr = _ptr; count=_count; type=PS_8F; setup(_pitch); }

	double getD( int i ) {
		void *p = (void *)( (char *)ptr + pitch*i );
		switch( type ) {
			case PS_1S: return (double)*(char*)p;
			case PS_1U: return (double)*(unsigned char*)p;
			case PS_2S: return (double)*(short*)p;
			case PS_2U: return (double)*(unsigned short*)p;
			case PS_4S: return (double)*(int*)p;
			case PS_4U: return (double)*(unsigned int*)p;
			case PS_4F: return (double)*(float*)p;
			case PS_8F: return (double)*(double*)p;
		}
		return 0.0;
	}
};

void plotArea( PlotSeries &p ) {
	glBegin( GL_TRIANGLE_STRIP );
	double x = 0.0;
	double xStep = 1.0 / (double)p.count;
	for( int i=0; i<p.count; i++ ) {
		glVertex2d( x, 0.0 );
		double val = p.getD(i);
		glVertex2d( x, val );
		x += xStep;
	}
	glEnd();
}

/*
void plotGridLines( PlotSeries &p, double xScale=1.0, double yScale=1.0 ) {
	double logPowerY = (double)floor( log( p.maxY - p.minY ) / log( 10.0 ) );
	double gridStepY = pow( 10.0, logPowerY );
	double gridB = floor( p.minY / gridStepY ) * gridStepY - gridStepY;
	double gridT = floor( p.maxY / gridStepY ) * gridStepY + gridStepY;
	gridStepY /= yScale;

	glBegin( GL_LINES );
	for( double y=gridB; y<=gridT; y+=gridStepY ) {
		glVertex2d( 0.0, y );
		glVertex2d( 1.0, y );
	}
	glEnd();
}
*/

		// SET state of the oscillator
double betaFuncConst( int t ) {
	return Che_inputBetaDC;
}

double betaFuncSquare( int t ) {
	double beta = floor( 1.0+sin( PI2 * Che_dt * (double)t / Che_inputBetaPeriod ) );
	return Che_inputBetaDC + Che_inputBetaAmp * (max( 0.0, min( 1.0, beta ) ));
}

double betaFuncWave( int t ) {
	double beta = Che_inputBetaDC + Che_inputBetaAmp * sin( PI2 * Che_dt * (double)t / Che_inputBetaPeriod );
	return max( 0.0, beta );
}

void cheReactionSimulate( int nose, int middle, int motor, int simSteps, CheState *history, double (*betaFunc)(int t) ) {
	// If you want a non spatial model, pass only nose buckets
	// If history is an optional pointer to a [totalBuckets * simSteps] sized array for plotting purposes
	int totalBuckets = nose + middle + motor;
	CheReaction *reactionBuckets = new CheReaction[totalBuckets];

	double lenOfBacteria = 5e-6f;
	double ky = Che_yDiff / pow( (lenOfBacteria / (double)totalBuckets), 2.0 );
	double kz = Che_zDiff / pow( (lenOfBacteria / (double)totalBuckets), 2.0 );

	// @TODO: Che_zDistributed will become local?
	if( Che_zDistributed ) {
		int x = 0;
		for( ; x<nose; x++ ) {
			reactionBuckets[x].init();
			reactionBuckets[x].state[0].a_u = Che_a_u * (double)totalBuckets / (double)nose;
			reactionBuckets[x].state[0].y_u = Che_y_u;
			reactionBuckets[x].state[0].z_u = Che_z_u;
			reactionBuckets[x].state[0].m_u = 0.f;
			if( totalBuckets == 1 ) {
				reactionBuckets[x].state[0].m_u = Che_m_u;
			}
			reactionBuckets[x].state[1] = reactionBuckets[x].state[0];
		}
		for( ; x<nose+middle; x++ ) {
			reactionBuckets[x].init();
			reactionBuckets[x].state[0].a_u = 0.f;
			reactionBuckets[x].state[0].y_u = Che_y_u;
			reactionBuckets[x].state[0].z_u = Che_z_u;
			reactionBuckets[x].state[0].m_u = 0.f;
			reactionBuckets[x].state[1] = reactionBuckets[x].state[0];
		}
		for( ; x<nose+middle+motor; x++ ) {
			reactionBuckets[x].init();
			reactionBuckets[x].state[0].a_u = 0.f;
			reactionBuckets[x].state[0].y_u = Che_y_u;
			reactionBuckets[x].state[0].z_u = Che_z_u;
			reactionBuckets[x].state[0].m_u = Che_m_u * (double)totalBuckets / (double)motor;
			reactionBuckets[x].state[1] = reactionBuckets[x].state[0];
		}
	}
	else {
		int x = 0;
		for( ; x<nose; x++ ) {
			reactionBuckets[x].init();
			reactionBuckets[x].state[0].a_u = Che_a_u * (double)totalBuckets / (double)nose;
			reactionBuckets[x].state[0].y_u = Che_y_u;
			reactionBuckets[x].state[0].z_u = Che_z_u * (double)totalBuckets / (double)nose;
			reactionBuckets[x].state[0].m_u = 0.f;
			if( totalBuckets == 1 ) {
				reactionBuckets[x].state[0].m_u = Che_m_u;
			}
			reactionBuckets[x].state[1] = reactionBuckets[x].state[0];
		}
		for( ; x<nose+middle; x++ ) {
			reactionBuckets[x].init();
			reactionBuckets[x].state[0].a_u = 0.f;
			reactionBuckets[x].state[0].y_u = Che_y_u;
			reactionBuckets[x].state[0].z_u = 0.f;
			reactionBuckets[x].state[0].m_u = 0.f;
			reactionBuckets[x].state[1] = reactionBuckets[x].state[0];
		}
		for( ; x<nose+middle+motor; x++ ) {
			reactionBuckets[x].init();
			reactionBuckets[x].state[0].a_u = 0.f;
			reactionBuckets[x].state[0].y_u = Che_y_u;
			reactionBuckets[x].state[0].z_u = 0.f;
			reactionBuckets[x].state[0].m_u = Che_m_u * (double)totalBuckets / (double)motor;
			reactionBuckets[x].state[1] = reactionBuckets[x].state[0];
		}
	}

	for( int t=0; t<simSteps; t++ ) {
		//if( !(t&0xFFFF) ) {
		//	printf( "%f%%\r", 100.f*(double)t/(double)simSteps );
		//}

		double beta = (*betaFunc)( t );

		for( int x=0; x<totalBuckets; x++ ) {
			// DIFFUSE
			double a, b, c;
			if( totalBuckets > 1 ) {
				a = (x==0) ? reactionBuckets[x].state[0].y_p : reactionBuckets[x-1].state[0].y_p;
				b = reactionBuckets[x  ].state[0].y_p;
				c = (x==totalBuckets-1) ? reactionBuckets[x].state[0].y_p : reactionBuckets[x+1].state[0].y_p;
				reactionBuckets[x].state[1].y_p += Che_dt * ky * ( a + c - 2*b );

				a = (x==0) ? reactionBuckets[x].state[0].y_u : reactionBuckets[x-1].state[0].y_u;
				b = reactionBuckets[x  ].state[0].y_u;
				c = (x==totalBuckets-1) ? reactionBuckets[x].state[0].y_u : reactionBuckets[x+1].state[0].y_u;
				reactionBuckets[x].state[1].y_u += Che_dt * ky * ( a + c - 2*b );

				if( Che_zDistributed ) {
					a = (x==0) ? reactionBuckets[x].state[0].z_u : reactionBuckets[x-1].state[0].z_u;
					b = reactionBuckets[x  ].state[0].z_u;
					c = (x==totalBuckets-1) ? reactionBuckets[x].state[0].z_u : reactionBuckets[x+1].state[0].z_u;
					reactionBuckets[x].state[1].z_u += Che_dt * kz * ( a + c - 2*b );

					a = (x==0) ? reactionBuckets[x].state[0].z_yp : reactionBuckets[x-1].state[0].z_yp;
					b = reactionBuckets[x  ].state[0].z_yp;
					c = (x==totalBuckets-1) ? reactionBuckets[x].state[0].z_yp : reactionBuckets[x+1].state[0].z_yp;
					reactionBuckets[x].state[1].z_yp += Che_dt * kz * ( a + c - 2*b );
				}
			}

			// REACT
			reactionBuckets[x].state[0].beta = beta;
			reactionBuckets[x].state[1].beta = beta;
			reactionBuckets[x].step( Che_dt );
		}

		// ADVANCE state
		for( int x=0; x<totalBuckets; x++ ) {
			reactionBuckets[x].advanceState();
		}

		if( history ) {
			for( int x=0; x<totalBuckets; x++ ) {
				history[t*totalBuckets+x] = reactionBuckets[x].state[0];
			}
		}
	}

	delete reactionBuckets;
}

CheState *plots = 0;

void render() {
	zmathDeactivateFPExceptions();

	// SETUP view
	glClearColor( 0.8f, 0.8f, 0.7f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	double viewport[4];
	glGetDoublev( GL_VIEWPORT, viewport );
	double aspect = viewport[3] / viewport[2];
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
//	gluPerspective( 30.f, viewport[2] / viewport[3], 0.1, 1000.0 );
	glMatrixMode(GL_MODELVIEW);
	zglMatrixFirstQuadrantAspectCorrect();

	double _m[] = { 0.002947f, -0.000260f, -0.001817f, 0.000000f, -0.001240f, -0.002816f, -0.001609f, 0.000000f, -0.001353f, 0.002015f, -0.002483f, 0.000000f, 0.357310f, 0.874117f, 0.000000f, 1.000000f };
	glMultMatrixd( _m );

	zviewpointSetupView();

	int nose = 1;
	int motor = 1;
	int middle = Che_divs-nose-motor;

	int x = 0, t = 0;
	if( changeMon.hasChanged() ) {
		if( plots ) {
			delete plots;
		}
		plots = new CheState[Che_divs * Che_simSteps];
		cheReactionSimulate( nose, middle, motor, Che_simSteps, plots, betaFuncSquare );
	}

	// PLOT surfaces
	double duration = Che_dt * Che_simSteps;
	glEnable( GL_DEPTH_TEST );
	glPushMatrix();
	int numPlots = Che_divs + 3;
	glColor3ub( 255, 255, 255 );
	glBegin( GL_QUADS );
	glVertex2d( 0.f, 0.f );
	glVertex2d( Che_plotScaleT*duration, 0.f );
	glVertex2d( Che_plotScaleT*duration, Che_plotScaleX * numPlots );
	glVertex2d( 0.f, Che_plotScaleX * numPlots );
	glEnd();

	// PLOT seconds
	glColor3ub( 0, 200, 0 );
	glBegin( GL_LINES );
	for( t=0; t<(int)duration+1; t++ ) {
		glVertex3d( Che_plotScaleT * t, 0.f, 0.1f );
		glVertex3d( Che_plotScaleT * t, Che_plotScaleX * numPlots, 0.1f );
	}
	glEnd();

	glTranslated( 0.f, Che_plotScaleX, 0.f );

	int plotSkip = Che_plotSkip;
	int plotPoints = Che_simSteps / Che_plotSkip;

	// PLOT beta
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glColor4ub( 150, 150, 50, 128 );
	glPushMatrix();
	glRotatef( 90.f, 1.f, 0.f, 0.f );
	glScaled( Che_plotScaleT*duration, Che_plotScaleBeta, 1.f );
	PlotSeries ps( &plots[0].beta, plotPoints, plotSkip*Che_divs*sizeof(plots[0]) );
	plotArea( ps );
	glPopMatrix();
	glTranslated( 0.f, Che_plotScaleX, 0.f );

	// PLOT each spatial time trace for y_p
	for( x=0; x<Che_divs; x++ ) {
		glDisable( GL_BLEND );
		glColor3ub( 0, 0, 0 );
		glBegin( GL_LINES );
			glVertex2d( 0.f, 0.f );
			glVertex2d( Che_plotScaleT*duration, 0.f );
		glEnd();

		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glColor4ub( 200, 30, 10, 128 );
		glPushMatrix();
		glRotatef( 90.f, 1.f, 0.f, 0.f );
		glScaled( Che_plotScaleT*duration, Che_plotScaleYp, 1.f );
		PlotSeries psy( &plots[x].y_p, plotPoints, plotSkip*Che_divs*sizeof(plots[0]) );
		plotArea( psy );
		glPopMatrix();

		glTranslated( 0.f, Che_plotScaleX, 0.f );
	}

	// PLOT M_Yp
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glColor4ub( 50, 30, 200, 128 );
	glPushMatrix();
	glRotatef( 90.f, 1.f, 0.f, 0.f );
	glScaled( Che_plotScaleT*duration, Che_plotScaleYp, 1.f );
	PlotSeries psm( &plots[Che_divs-1].m_yp, plotPoints, plotSkip*Che_divs*sizeof(plots[0]) );
	plotArea( psm );
	glPopMatrix();
	glTranslated( 0.f, Che_plotScaleX, 0.f );

	glPopMatrix();

	// PLOT z intersect plane
	glColor4ub( 10, 30, 10, 150 );
	glBegin( GL_QUADS );
	glVertex3d( 0.f, 0.f, Che_plotZIntersectPlane*Che_plotScaleYp );
	glVertex3d( Che_plotScaleT*duration, 0.f, Che_plotZIntersectPlane*Che_plotScaleYp );
	glVertex3d( Che_plotScaleT*duration, Che_plotScaleX * numPlots, Che_plotZIntersectPlane*Che_plotScaleYp );
	glVertex3d( 0.f, Che_plotScaleX * numPlots, Che_plotZIntersectPlane*Che_plotScaleYp );
	glEnd();

	zglDrawStandardAxis(100.f);
}

void startup() {
	zmathDeactivateFPExceptions();

	changeMon.add( &Che_divs, sizeof(Che_divs) );
	changeMon.add( &Che_simSteps, sizeof(Che_simSteps) );
	changeMon.add( &Che_dt, sizeof(Che_dt) );
	changeMon.add( &Che_zDistributed, sizeof(Che_zDistributed) );
	changeMon.add( &Che_k1, sizeof(Che_k1) );
	changeMon.add( &Che_k2, sizeof(Che_k2) );
	changeMon.add( &Che_k3, sizeof(Che_k3) );
	changeMon.add( &Che_k4, sizeof(Che_k4) );
	changeMon.add( &Che_k5, sizeof(Che_k5) );
	changeMon.add( &Che_k6, sizeof(Che_k6) );
	changeMon.add( &Che_k7, sizeof(Che_k7) );
	changeMon.add( &Che_k8, sizeof(Che_k8) );
	changeMon.add( &Che_k9, sizeof(Che_k9) );
	changeMon.add( &Che_a_u, sizeof(Che_a_u) );
	changeMon.add( &Che_y_u, sizeof(Che_y_u) );
	changeMon.add( &Che_z_u, sizeof(Che_z_u) );
	changeMon.add( &Che_m_u, sizeof(Che_m_u) );
	changeMon.add( &Che_yDiff, sizeof(Che_yDiff) );
	changeMon.add( &Che_inputBetaDC, sizeof(Che_inputBetaDC) );
	changeMon.add( &Che_inputBetaPeriod, sizeof(Che_inputBetaPeriod) );
	changeMon.add( &Che_inputBetaAmp, sizeof(Che_inputBetaAmp) );
}

void shutdown() {
}

void staticResponse() {
	const int numBetaDivs = 100;

	const double betaBot = 0.00;
	const double betaTop = 0.11;

	int nose = 1;
	int motor = 1;
	int middle = Che_divs-nose-motor;

	FILE *file = fopen( "static_response.txt", "wt" );

	Che_simSteps = 14000;
	CheState *history = new CheState[Che_divs * Che_simSteps];

	fprintf( file, "beta\tm_yp9\ta_p0\tz_yp0\ty_u0\n" );

	for( int i=0; i<numBetaDivs; i++ ) {
		printf( "%lf%%\r", 100.0*(double)i/(double)numBetaDivs );

		double beta = betaBot + (betaTop-betaBot)*( (double)i / (double)numBetaDivs );
		Che_inputBetaDC = beta;

		cheReactionSimulate( nose, middle, motor, Che_simSteps, history, betaFuncConst );
		fprintf( file, "%le\t%le\t%le\t%le\t%le\n",
			beta,
			history[Che_divs*(Che_simSteps-1) + (Che_divs-1) ].m_yp,
			history[Che_divs*(Che_simSteps-1) + (0) ].a_p,
			history[Che_divs*(Che_simSteps-1) + (0) ].z_yp,
			history[Che_divs*(Che_simSteps-1) + (0) ].y_u
		);
	}
	delete history;

	fclose( file );
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		zviewpointDumpInCFormat( "mat.txt" );
	}
	if( zmsgIs(type,Key) && zmsgIs(which,w) ) {
		staticResponse();
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

