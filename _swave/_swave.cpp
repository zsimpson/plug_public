// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "math.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zvec.h"
#include "zgltools.h"
#include "zviewpoint.h"

ZPLUGIN_BEGIN( swave );

ZVAR( int, Swave_steps, 20 );
ZVAR( double, Swave_arrowLen, 1 );

ZVAR( double, Swave_l, -4 );
ZVAR( double, Swave_r, +4 );
ZVAR( double, Swave_b, -4 );
ZVAR( double, Swave_t, +4 );

ZVAR( double, Swave_pullDown, 9.05e-3 );
ZVAR( double, Swave_M1, 6.19e-3 );
ZVAR( double, Swave_M2, 1.42e-2 );
ZVAR( double, Swave_M3, 3.39e-2 );
ZVAR( double, Swave_M4, 2.24e-2 );
ZVAR( double, Swave_M5, 4.14e-2 );


ZVAR( double, Swave_gateP1, 10 );
ZVAR( double, Swave_gateP2, 2.5 );
ZVAR( double, Swave_gateP3, 2 );
ZVAR( double, Swave_gateP4, 0.5 );

// Games to play: twiddling the gate model
// Adding some gates?

double gateModelImOp( double inputConc ) {
    return +( tanh(-Swave_gateP1*inputConc+Swave_gateP2)/Swave_gateP3+Swave_gateP4 );
}

double gateModelIpOp( double inputConc ) {
    return +( tanh(+Swave_gateP1*inputConc+Swave_gateP2)/Swave_gateP3+Swave_gateP4 );
}

double gateModelImOm( double inputConc ) {
    return -( tanh(-Swave_gateP1*inputConc+Swave_gateP2)/Swave_gateP3+Swave_gateP4 );
}

double gateModelIpOm( double inputConc ) {
    return -( tanh(+Swave_gateP1*inputConc+Swave_gateP2)/Swave_gateP3+Swave_gateP4 );
}

DVec2 swave( double x, double y ) {
	DVec2 d;
	d.x = -Swave_pullDown*x -Swave_M1 +Swave_M3*gateModelIpOp(x) + Swave_M5*gateModelIpOm(y);
	d.y = -Swave_pullDown*y -Swave_M2 +Swave_M4*gateModelIpOp(x);
	return d;
}

/*
void integrate( ZMat initCond, ZMat &output, DiffFunc diffFunc, double duration ) {
	int dim = initCond.count;

	ZIntegratorGSLRK45 rk45(
		dim, initCond.vec, 0.0, duration, 1e-10, 1e-10, 1e-10, 1e-12, 1,
		CallbackDeriv _deriv, void *_callbackUserParams
	);
	
	rk45.integrate();

}
*/

void render() {
	glOrtho( Swave_l, Swave_r, Swave_b, Swave_t, -1, 1 );
	zviewpointSetupView();

	double x, y;	

	double l = Swave_l;
	double r = Swave_r;
	double b = Swave_b;
	double t = Swave_t;

	// QUIVER plot
	double xStep = (r-l) / (double)Swave_steps;
	double yStep = (t-b) / (double)Swave_steps;
	for( y=b; y<t; y+=yStep ) {
		for( x=l; x<r; x+=xStep ) {
			DVec2 d = swave( x, y );
			d.mul( Swave_arrowLen );
			zglDrawVector( x, y, 0.0, d.x, d.y, 0.0 );
		}
	}
	
	// PLOT the gate transfer function
	glColor3ub( 0, 0, 255 );
	glBegin( GL_LINE_STRIP );
	for( x=-2.0; x<= 2.0; x+=0.01 ) {
		double y = gateModelImOp( x );
		glVertex2d( x, y );
	}
	glEnd();
}

void startup() {
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

