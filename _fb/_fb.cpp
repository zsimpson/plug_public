// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "string.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zviewpoint.h"
#include "zmathtools.h"

ZPLUGIN_BEGIN( fb );

ZVAR( float, Fb_heaterPower, 1 );
ZVAR( float, Fb_targetTemp, 70 );
ZVAR( float, Fb_wallConduction, 0.1 );
ZVAR( float, Fb_outsideTemp, 30 );
ZVAR( float, Fb_dt, 1 );

float insideTemp = 0.f;

//struct State {
//	float time;
//	float outsideTemp;
//	float insideTemp;
//	float targetTemp;
//	float heaterLevel;
//};
//
//ZTLVec<State> states;


// this and the lnet I have to convert over to using a propoer intergrator

#define STATE_COUNT (512)
int tick = 0;
float temp[ STATE_COUNT ];

void simulate() {
	float heaterLevel = Fb_heaterPower * (Fb_targetTemp - insideTemp);
	insideTemp += Fb_dt * ( heaterLevel - Fb_wallConduction*(insideTemp-Fb_outsideTemp) );
	temp[ tick % STATE_COUNT ] = insideTemp;
	tick++;
}

void render() {
	zmathDeactivateFPExceptions();
	simulate();

	glScalef( 1.f/512.f, 1.f/512.f, 1.f );
	zviewpointSetupView();

	glBegin( GL_LINES );
		glColor3ub( 0, 255, 0 );
		glVertex2f( 0.f, Fb_targetTemp );
		glVertex2f( (float)STATE_COUNT, Fb_targetTemp );

		glColor3ub( 0, 0, 255 );
		glVertex2f( 0.f, Fb_outsideTemp );
		glVertex2f( (float)STATE_COUNT, Fb_outsideTemp );
	glEnd();

	glColor3ub( 255, 255, 255 );
	glBegin( GL_LINE_STRIP );
	glVertex2f( 0.f, 0.f );
	for( int i=0; i<STATE_COUNT; i++ ) {
		glVertex2f( (float)i, temp[i] );
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

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		insideTemp = 0.f;
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

