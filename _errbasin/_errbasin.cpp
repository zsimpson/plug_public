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
#include "zbits.h"
#include "zbittexcache.h"
#include "zviewpoint.h"

ZPLUGIN_BEGIN( errbasin );

ZVAR( float, Errbasin_variance, 9.7e2 );
ZVAR( float, Errbasin_curvature, 7.29e-003 );
ZVAR( float, Errbasin_parameter, 1.32e+002 );

ZVAR( float, Errbasin_scale, 3.70e-003 );
ZVAR( float, Errbasin_x, -2.26e+002 );
ZVAR( float, Errbasin_y, -1.01e+002 );

void render() {
	glScalef( Errbasin_scale, Errbasin_scale, 1.f );
	glTranslatef( Errbasin_x, Errbasin_y, 0.f );
	zviewpointSetupView();

	ZBits bits( 256, 256, ZBITS_COMMON_LUM_8 );
	bits.fill( 0 );
	for( int x=0; x<256; x++ ) {
		for( int y=0; y<256; y++ ) {
			float xd = (float)x - Errbasin_parameter;
			float yd = (float)y - 180.f;
			float yf = yd + Errbasin_curvature * xd * xd;
			float v = 256.f * expf( yf * yf / -Errbasin_variance );
			v = max( 0.f, min( v, 255.f ) );
			bits.setUchar( x, y, (unsigned char)v );
		}
	}
	int *t = zbitTexCacheLoad( &bits );
	zbitTexCacheRender( t );
	zbitTexCacheFree( t );

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

