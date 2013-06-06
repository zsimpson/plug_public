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
// ZBSLIB includes:
#include "zmathtools.h"
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zui.h"
#include "ztmpstr.h"
#include "zfilespec.h"
#include "zgltools.h"
#include "zwildcard.h"


extern int useDirtyRects;

ZPLUGIN_BEGIN( zuitest );


void render() {
}

void startup() {
	useDirtyRects = 0;
	
	// CREATE a bunch of test UI
	ZUI *root = ZUI::zuiFindByName( "root" );

	ZUI *panel = ZUI::factory( "", "ZUIPanel" );
	panel->attachTo( root );
	panel->putS( "layout", "table" );
	panel->putI( "table_cols", 5 );

	for( int i=0; i<1000; i++ ) {
		ZUI *test = ZUI::factory( "", "ZUIButton" );
		test->putS( "text", ZTmpStr("This is a button %d",i) );
		test->attachTo( panel );
	}	
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

