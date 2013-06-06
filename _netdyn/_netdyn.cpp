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
#include "ztl.h"
#include "zmsg.h"
#include "zgltools.h"
#include "zviewpoint.h"

ZPLUGIN_BEGIN( netdyn );

ZVAR( double, Netdyn_domain, 1 );
ZVAR( double, Netdyn_steps, 30 );
ZVAR( double, Netdyn_k1, +1.2 );
ZVAR( double, Netdyn_k2, -5.8 );
ZVAR( double, Netdyn_k3, +1 );
ZVAR( double, Netdyn_k4, -1.75 );
ZVAR( double, Netdyn_k5, 0 );
ZVAR( double, Netdyn_k6, 0 );
ZVAR( double, Netdyn_vizScale, 1.2e-2 );
ZVAR( int, Netdyn_dtSteps, 10000 );
ZVAR( double, Netdyn_dt, 2e-3 );
ZVAR( int, Netdyn_numTraces, 2 );
ZVAR( int, Netdyn_skipPoints, 10 );


//x-|y
//x|-y
//dx/dt = -k1*x -k2*y
//dy/dt = -k3*x -k4*y


DVec2 func1( double x, double y ) {
	return DVec2(
		+Netdyn_k1*x +Netdyn_k2*y,
		+Netdyn_k3*x +Netdyn_k4*y
	);
}

DVec2 func2( double x, double y ) {
	double dx = Netdyn_k1 * x + Netdyn_k2 * y + Netdyn_k3;
	double dy = Netdyn_k4 * x + Netdyn_k5 * y + Netdyn_k6;
	return DVec2( dx, dy );
}

DVec2 func3( double x, double y ) {
	if( fabs(x) > 1e10 || fabs(y) > 1e10 ) {
		return DVec2( 0, 0 );
	}
	double dx = Netdyn_k1 * x * y + Netdyn_k2 * y - Netdyn_k3 * x;
	double dy = Netdyn_k4 * x * y + Netdyn_k5 * x - Netdyn_k6 * y;
	return DVec2( dx, dy );
}

DVec2 tracePos[16];
DVec2 (*funcPtr)( double x, double y );

void render() {
	zviewpointSetupView();

	glColor3ub( 0, 0, 0 );
	glBegin( GL_LINES );
		glVertex2d( -Netdyn_domain, 0.0 );
		glVertex2d( +Netdyn_domain, 0.0 );
		glVertex2d( 0.0, -Netdyn_domain );
		glVertex2d( 0.0, +Netdyn_domain );
	glEnd();

	// DRAW the field
	glColor3ub( 255, 255, 255 );
	for( double y=-Netdyn_domain; y<=Netdyn_domain; y+=2.0*Netdyn_domain/Netdyn_steps ) {
		for( double x=-Netdyn_domain; x<=Netdyn_domain; x+=2.0*Netdyn_domain/Netdyn_steps ) {
			DVec2 f = (*funcPtr)( x, y );
			f.x *= Netdyn_vizScale;
			f.y *= Netdyn_vizScale;
			zglDrawVector( x, y, 0.0, f.x, f.y, 0.0 );
		}
	}

	// DRAW traces
	for( int i=0; i<Netdyn_numTraces; i++ ) {
		DVec2 pos = tracePos[i];
		ZTLVec<DVec2> trace( Netdyn_dtSteps );
		for( int i=0; i<Netdyn_dtSteps; i++ ) {
			DVec2 delta = (*funcPtr)( pos.x, pos.y );
			delta.mul( Netdyn_dt );
			pos.add( delta );
			trace.add( pos );
		}

		glColor3ub( 255, 0, 0 );
		glBegin( GL_LINE_STRIP );
			for( int i=0; i<trace.count; i++ ) {
				glVertex2d( trace[i].x, trace[i].y );
			}
		glEnd();
		glColor3ub( 0, 0, 0 );
		glPointSize( 2.f );
		glBegin( GL_POINTS );
			for( int i=0; i<trace.count; i+=Netdyn_skipPoints ) {
				glVertex2d( trace[i].x, trace[i].y );
			}
		glEnd();

		glColor3ub( 0, 0, 255 );
		glLineWidth( 2.f );
		glBegin( GL_LINES );
			glVertex2d( trace[0].x-0.01, trace[0].y-0.01 );
			glVertex2d( trace[0].x+0.01, trace[0].y+0.01 );
			glVertex2d( trace[0].x-0.01, trace[0].y+0.01 );
			glVertex2d( trace[0].x+0.01, trace[0].y-0.01 );
		glEnd();
	}
}

void setup0() {
	funcPtr = func1;
	Netdyn_k1 = +1.2;
	Netdyn_k2 = -5.8;
	Netdyn_k3 = +1.0;
	Netdyn_k4 = -1.75;
	Netdyn_numTraces = 1;
	tracePos[0] = DVec2( 0.5, 0.5 );
}

void setup1() {
	funcPtr = func1;
	Netdyn_k1 = 0.0;
	Netdyn_k2 = 0.0;
	Netdyn_k3 = +10.0;
	Netdyn_k4 = 0.0;
	Netdyn_numTraces = 1;
	tracePos[0] = DVec2( 0.5, 0.5 );
}

void setup2() {
	funcPtr = func1;
	Netdyn_k1 = 0.0;
	Netdyn_k2 = 0.0;
	Netdyn_k3 = +1.0;
	Netdyn_k4 = -1.0;
	Netdyn_numTraces = 1;
	tracePos[0] = DVec2( 0.5, 0.0 );
}

void setup3() {
	funcPtr = func1;
	Netdyn_k1 = 0.0;
	Netdyn_k2 = 0.0;
	Netdyn_k3 = -1.0;
	Netdyn_k4 = -1.0;
	Netdyn_numTraces = 1;
	tracePos[0] = DVec2( 0.5, 0.0 );
}

void setup4() {
	funcPtr = func1;
	Netdyn_k1 =  0.0;
	Netdyn_k2 = +1.0;
	Netdyn_k3 = +1.0;
	Netdyn_k4 =  0.0;
	Netdyn_numTraces = 1;
	Netdyn_vizScale = 6.8e-2;
	tracePos[0] = DVec2( +0.50, +0.1 );
}

void setup5() {
	funcPtr = func1;
	Netdyn_k1 = 0.0;
	Netdyn_k2 = -1.0;
	Netdyn_k3 = -1.0;
	Netdyn_k4 =  0.0;
	Netdyn_numTraces = 2;
	Netdyn_vizScale = 6.8e-2;
	tracePos[0] = DVec2( +0.50, +0.51 );
	tracePos[1] = DVec2( +0.50, +0.49 );
}

void setup6() {
	funcPtr = func1;
	Netdyn_k1 = 0.0;
	Netdyn_k2 = -1.0;
	Netdyn_k3 = +1.0;
	Netdyn_k4 =  0.0;
	Netdyn_numTraces = 1;
	Netdyn_vizScale = 6.8e-2;
	Netdyn_dt = 6e-4;
	Netdyn_skipPoints = 90;
	tracePos[0] = DVec2( +0.50, +0.51 );
}

void setup7() {
	funcPtr = func1;
	Netdyn_k1 = -0.1;
	Netdyn_k2 = -1.0;
	Netdyn_k3 = +1.0;
	Netdyn_k4 =  0.0;
	Netdyn_numTraces = 1;
	Netdyn_vizScale = 6.8e-2;
	Netdyn_dt = 6e-4;
	Netdyn_skipPoints = 90;
	tracePos[0] = DVec2( +0.50, +0.51 );
}

void setup8() {
	funcPtr = func2;
	Netdyn_k1 = 0.0;
	Netdyn_k2 = 0.0;
	Netdyn_k3 = 0.0;
	Netdyn_k4 = 0.0;
	Netdyn_k5 = -1.0;
	Netdyn_k6 = 1.0;
	Netdyn_numTraces = 1;
	Netdyn_vizScale = 6.8e-2;
	Netdyn_dt = 6e-4;
	Netdyn_skipPoints = 90;
	tracePos[0] = DVec2( +0.50, +0.51 );
}

void setup9() {
	funcPtr = func3;
	Netdyn_k1 = 1.0;
	Netdyn_k2 = 1.0;
	Netdyn_k3 = 0.2;
	Netdyn_k4 = 1.0;
	Netdyn_k5 = 1.0;
	Netdyn_k6 = 0.2;
	Netdyn_numTraces = 1;
	Netdyn_vizScale = 6.8e-2;
	Netdyn_dt = 6e-4;
	Netdyn_skipPoints = 90;
	tracePos[0] = DVec2( +0.50, +0.51 );
}

void startup() {
	setup9();
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
		// FIND the closest point in the path and drag it
		static int clickCount = 0;
		DVec3 mouse = zviewpointProjMouseOnZ0Plane();
		tracePos[clickCount % Netdyn_numTraces] = DVec2( mouse.x, mouse.y );
		clickCount++;
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

