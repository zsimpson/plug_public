// OPERATING SYSTEM specific includes :
#ifdef __APPLE__
	#include "OpenGL/gl.h"
	#include "OpenGL/glu.h"
#else
	#include "wingl.h"
	#include "GL/gl.h"
	#include "GL/glu.h"
#endif
// SDK includes:
// STDLIB includes:
#include "memory.h"
#include "string.h"
#include "math.h"
#include "float.h"
#include "assert.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "ztmpstr.h"
#include "zviewpoint.h"
#include "zrand.h"
#include "ztime.h"
#include "zgltools.h"
#include "zmathtools.h"
#include "ztl.h"

ZPLUGIN_BEGIN( shape1 );

ZVAR( float, Shape1_r0, 1.0 );
ZVAR( float, Shape1_r1, 5.70 );
ZVAR( float, Shape1_a, 0.228 );
ZVAR( float, Shape1_sa, 100.0 );
ZVAR( int, Shape1_h, 173 );

ZVAR( float, Shape1_p0, 5.5e3 );
ZVAR( float, Shape1_p1, 5.5e3 );
ZVAR( float, Shape1_p2, 1.16 );
ZVAR( float, Shape1_p3, -3.4e-1 );
ZVAR( float, Shape1_p4, 6.3e3 );

void render() {
	zviewpointSetupView();

	zglDrawStandardAxis( 100.f );

	ZTLVec<FVec3> path;
	for( int i=0; i<Shape1_h; i++ ) {
/*
		float in = 1.f - (float)i / (float)Shape1_h + 0.1f;
		float t = Shape1_a * (float)i;
		path.add( FVec3( in*Shape1_r1*cosf(t), in*Shape1_r1*sinf(t), (float)i ) );
*/
/*
		float t = Shape1_a * (float)i;
		float r = Shape1_p3*powf((float)i,3) + Shape1_p2*powf((float)i,2) + Shape1_p1*powf((float)i,1) + Shape1_p0;
		path.add( FVec3( r*cosf(t), r*sinf(t), (float)i*Shape1_p4 ) );
*/

		float t = Shape1_a * (float)i;
		float r1 = Shape1_p1*(float)i;
		float r2 = Shape1_p2*(float)i;

		path.add( FVec3( r1*cosf(t)+r2*cosf(5*t), r1*sinf(t)+r2*sinf(5*t), (float)i*Shape1_p4 ) );


	}

	glBegin( GL_LINE_STRIP );	
	for( int i=0; i<path.count; i++ ) {
		glVertex3fv( (GLfloat *)&path[i] );
	}
	glEnd();
	
	ZTLVec<FVec3> points;
	ZTLVec<IVec3> faces;

	for( int i=0; i<path.count-1; i++ ) {
		FVec3 z = path[i+1];
		z.sub( path[i+0] );
		z.normalize();
		
		FVec3 y = path[i];
		y.cross( z );
		y.normalize();
		
		FVec3 x = z;
		x.cross( y );

		float _m[] = { x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.x };		
		FMat3 m( _m );
		
		FMat4 mt( m, FVec3( path[i] ) );

		for( int a=0; a<16; a++ ) {
			float t = 2.f * 3.14f * (float)a / 16.f;
			FVec3 p1( Shape1_r0 * cosf(t), Shape1_r0 * sinf(t), 0.f );
			FVec3 p2 = mt.mul( p1 );
			points.add( p2 );
		}
	}

	// Cylindrical faces
	for( int i=0; i<path.count-2; i++ ) {
		for( int a=0; a<16; a++ ) {
			IVec3 f1( 16*i+a, 16*i+(a+1)%16, 16*(i+1)+a );
			faces.add( f1 );

			IVec3 f2( 16*i+(a+1)%16, 16*(i+1)+(a+1)%16, 16*(i+1)+a );
			faces.add( f2 );
		}
	}

	// End caps faces
	for( int a=0; a<16; a++ ) {
		IVec3 f1( (a+3)%16, (a+2)%16, 0 );
		faces.add( f1 );
	}

	for( int a=0; a<16; a++ ) {
		IVec3 f1( (path.count-2)*16, (path.count-2)*16+(a+2)%16, (path.count-2)*16+(a+3)%16 );
		faces.add( f1 );
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer( 3, GL_FLOAT, 0, (GLfloat *)&points[0] );
	glDrawArrays( GL_POINTS, 0, points.count );
	glDrawElements( GL_TRIANGLES, faces.count*3, GL_UNSIGNED_INT, (GLint *)&faces[0] );
	glDisableClientState( GL_VERTEX_ARRAY );
	
	FILE *f = fopen( "shape1.scad", "w" );
	fprintf( f, "module curve() {\n" );
	fprintf( f, "polyhedron( points=[\n" );
	for( int i=0; i<points.count; i++ ) {
		fprintf( f, "[%f,%f,%f],\n", points[i].x, points[i].y, points[i].z );
	}
	fprintf( f, "], triangles=[\n" );
	for( int i=0; i<faces.count; i++ ) {
		fprintf( f, "[%d,%d,%d],\n", faces[i].x, faces[i].y, faces[i].z );
	}
	fprintf( f, "]);\n" );
	fprintf( f, "}\n" );
	fclose( f );
	
}

void reset() {
}

void startup() {
   	reset();
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		reset();
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

