// @ZBS {
//		*MODULE_DEPENDS zuiradiobutton.cpp zuicheck.cpp zuicheckmulti.cpp zuitexteditor.cpp zuifilepicker.cpp zuivar.cpp zuiplot.cpp zuislider.cpp zuitabbutton.cpp zuitabpanel.cpp
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "math.h"
#include "string.h"
// MODULE includes:
#include "mainutil.h"
// ZBSLIB includes:
#include "zmathtools.h"
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zui.h"
#include "ztmpstr.h"
#include "zfilespec.h"
#include "zgltools.h"
#include "zrgbhsv.h"
#include "zglgfiletools.h"
#include "zwildcard.h"

extern void trace( char *msg, ... );

ZPLUGIN_BEGIN( zuiplay );

ZVAR( int, Zuiplay_space, 25 );
ZVARB( float, Zuiplay_bgColor, 1.00e+000, 0.00e+000, 1.00e+000 );
ZVARB( float, Zuiplay_h, 6.11e-001, 0.00e+000, 1.00e+000 );
ZVARB( float, Zuiplay_s, 1.28e-001, 0.00e+000, 1.00e+000 );
ZVARB( float, Zuiplay_v, 1.00e+000, 0.00e+000, 1.00e+000 );


int reload = 0;

void render() {
	glClearColor( Zuiplay_bgColor, Zuiplay_bgColor, Zuiplay_bgColor, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	zglPixelMatrixFirstQuadrant();

	int i;

	float r, g, b;
	zHSVF_to_RGBF( Zuiplay_h, Zuiplay_s, Zuiplay_v, r, g, b );

	glColor4f( r, g, b, 1.f );
	glBegin( GL_LINES );
	for( i=0; i< 5000; i+=Zuiplay_space ) {
		glVertex2i( i, 0 );
		glVertex2i( i, 2000 );
	}
	glEnd();

	glBegin( GL_LINES );
	for( i=0; i< 5000; i+=Zuiplay_space ) {
		glVertex2i( 0, i );
		glVertex2i( 2000, i );
	}
	glEnd();

}

int checkMultiTex = -1;
void startup() {
	reload = 1;
}

void shutdown() {
	if( checkMultiTex != -1 ) {
		glDeleteTextures( 1, (GLuint*)&checkMultiTex );
	}
	ZUI *testZUI = ZUI::zuiFindByName( "testPanel" );
	if( testZUI ) {
		testZUI->die();
	}

}
void update() {
	// CHECK if the input file has been saved and reload it
	static unsigned int lastLoadTime = 0;
	ZFileSpec spec( pluginPath( "zuiplay.zui" ) );
	if( !zWildcardFileExists( spec.get() ) ) {
		int d=1;
	}
	if( reload || spec.getTime() > lastLoadTime ) {
		reload = 0;
		lastLoadTime = spec.getTime();

		ZUI *testZUI = ZUI::zuiFindByName( "testPanel" );
		if( testZUI ) {
			testZUI->die();
		}

		ZUI::zuiSetErrorTrap( 1 );
		ZUI::zuiExecuteFile( spec.get() );
		if( ZUI::zuiErrorMsg ) {
			// @TODO: Dialog box with error
			int a = 1;
		}
		ZUI::zuiSetErrorTrap( 0 );
		
		zMsgQueue( "type=ZUILayout" );
	}
}

void handleMsg( ZMsg *msg ) {
	if( zmsgIs( type, KeyDown ) ) {
		if( zmsgIs( which, b ) )  {
			ZUI *z = ZUI::zuiFindByName( "testbutt" );
			z->x += 1.1f;
			z->dirty();
			
	
		}
	}
}

ZMSG_HANDLER( MySimpleDragInitHandler ) {
	// I know what kind of control this message comes from, since I set this
	// message to be sent...also, fromZUI will tell me specifically.
	ZUI *fromZUI = ZUI::zuiFindByName( zmsgS( fromZUI ) );
	if( fromZUI ) {
		fromZUI->putP( "dragEmitObject", fromZUI );
			// dynamically assign the object to be dragged that may get dropped somewhere else...
			// in this simple example it is ourself we allow to be dragged
		trace( "Object %08X (%s) set as dragEmitObject\n", fromZUI, fromZUI->name );
	}		
}

#include "stdlib.h"
ZMSG_HANDLER( MySimpleDragAcceptHandler ) {
	ZUI *zuiAccept = ZUI::zuiFindByName( zmsgS( zuiAccept ) );
	ZUI *fromZUI = ZUI::zuiFindByName( zmsgS( fromZUI ) );
	if( zuiAccept && fromZUI ) {
		ZUI* object = (ZUI*)atol( zmsgS( dragAcceptObject ) );
			// the object pointer was embedded in the message as a long, which makes
			// it 32/64 bit safe etc...
		if( object ) {
			object->attachTo( zuiAccept );
			ZUI::dirtyAll();
				// the action we perform on accepting a dropped object is to attach it to ourselves,
				// since we know it is a zui 
		}
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( update );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

