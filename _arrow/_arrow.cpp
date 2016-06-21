// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:

// STDLIB includes:
#include "math.h"
#include "float.h"
#include "string.h"
#include "stdlib.h"

// MODULE includes:

// ZBSLIB includes:
#include "zmathtools.h"
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zviewpoint.h"
#include "zgltools.h"
#include "ztmpstr.h"

extern void trace( char *fmt, ... );

ZVARB( int, Arrow_a, 1, 0, 1 );

ZPLUGIN_BEGIN( arrow );


void subdivide( FVec3 &v1, FVec3 &v2, FVec3 &v3, FVec3 *verts, int &vertCount, int depth ) {
    if( depth == 0 ) {
		verts[vertCount++] = v1;
		verts[vertCount++] = v2;
		verts[vertCount++] = v3;
        return;
    }
    
    FVec3 v12 = v1;
    v12.add( v2 );
    v12.normalize();
    
    FVec3 v23 = v2;
    v23.add( v3 );
    v23.normalize();
    
    FVec3 v31 = v3;
    v31.add( v1 );
    v31.normalize();
    
    subdivide( v1,  v12, v31, verts, vertCount, depth - 1);
    subdivide( v2,  v23, v12, verts, vertCount, depth - 1);
    subdivide( v3,  v31, v23, verts, vertCount, depth - 1);
    subdivide( v12, v23, v31, verts, vertCount, depth - 1);
}

void render() {
	zviewpointSetupView();
	
	glClearColor( 1.f, 1.f, 1.f, 1.f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glEnable( GL_DEPTH_TEST );

	float X = 0.525731112119133606f;
	float Z = 0.850650808352039932f;
	FVec3 vdata[12] = {
		FVec3( -X, 0.0, Z ), FVec3( X, 0.0, Z ), FVec3( -X, 0.0, -Z ), FVec3( X, 0.0, -Z ),
		FVec3( 0.0, Z, X ), FVec3( 0.0, Z, -X ), FVec3( 0.0, -Z, X ), FVec3( 0.0, -Z, -X ),
		FVec3( Z, X, 0.0 ), FVec3( -Z, X, 0.0 ), FVec3( Z, -X, 0.0 ), FVec3( -Z, -X, 0.0 )
	};
	int tindices[20][3] = {
		{0, 4, 1}, { 0, 9, 4 }, { 9, 5, 4 }, { 4, 5, 8 }, { 4, 8, 1 },
		{ 8, 10, 1 }, { 8, 3, 10 }, { 5, 3, 8 }, { 5, 2, 3 }, { 2, 7, 3 },
		{ 7, 10, 3 }, { 7, 6, 10 }, { 7, 11, 6 }, { 11, 0, 6 }, { 0, 1, 6 },
		{ 6, 1, 10 }, { 9, 0, 11 }, { 9, 11, 2 }, { 9, 2, 5 }, { 7, 2, 11 }
	};

	int depth = 2;
	int vertCount = 0;
	FVec3 verts[1024*50];
	for( int i = 0; i < 20; i++ ) {
		subdivide( vdata[ tindices[i][0] ], vdata[ tindices[i][1] ], vdata[ tindices[i][2] ], verts, vertCount, depth );
	}

	glPointSize( 3.f );
	glColor3ub( 255, 0, 0 );
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer( 3, GL_FLOAT, 0, verts );
	glDrawArrays( GL_POINTS, 0, vertCount );
	glDisableClientState(GL_VERTEX_ARRAY);
}

void render1() {
	zviewpointSetupView();
	
	glClearColor( 1.f, 1.f, 1.f, 1.f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glEnable( GL_DEPTH_TEST );

	// Version 1, a monolithic arrow
	/*	
	float verts[1024][3] = {
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f }
	};

	int rot = 8;
	int v = 2;	
	for( int i=0; i<rot; i++ ) {
		float t0 = PI2F * ( (float)(i+0) / (float)rot );
		float t1 = PI2F * ( (float)(i+1) / (float)rot );
		verts[v][0] = 0.1f * cosf(t0);
		verts[v][1] = 0.1f * sinf(t0);
		verts[v][2] = 0.0f;
		v++;
		
		verts[v][0] = 0.1f * cosf(t0);
		verts[v][1] = 0.1f * sinf(t0);
		verts[v][2] = 0.5f;
		v++;
		
		verts[v][0] = 0.2f * cosf(t0);
		verts[v][1] = 0.2f * sinf(t0);
		verts[v][2] = 0.5f;
		v++;
		
		verts[v][0] = 0.1f * cosf(t1);
		verts[v][1] = 0.1f * sinf(t1);
		verts[v][2] = 0.0f;
		v++;

		verts[v][0] = 0.1f * cosf(t1);
		verts[v][1] = 0.1f * sinf(t1);
		verts[v][2] = 0.5f;
		v++;
		
		verts[v][0] = 0.2f * cosf(t1);
		verts[v][1] = 0.2f * sinf(t1);
		verts[v][2] = 0.5f;
		v++;
	}
	
	unsigned int indicies[1024];

	int index = 0;	
	for( int i=0; i<rot; i++ ) {
		int q = i*6;
	
		indicies[index++] = 0;
		indicies[index++] = q + 5;
		indicies[index++] = q + 2;

		indicies[index++] = q + 2;
		indicies[index++] = q + 5;
		indicies[index++] = q + 3;

		indicies[index++] = q + 5;
		indicies[index++] = q + 6;
		indicies[index++] = q + 3;

		indicies[index++] = q + 3;
		indicies[index++] = q + 6;
		indicies[index++] = q + 4;

		indicies[index++] = q + 4;
		indicies[index++] = q + 6;
		indicies[index++] = q + 7;

		indicies[index++] = q + 4;
		indicies[index++] = q + 7;
		indicies[index++] = 1;
	}

	glColor3ub( 255, 0, 0 );
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer( 3, GL_FLOAT, 0, verts );
	glDrawElements( GL_TRIANGLES, index, GL_UNSIGNED_INT, indicies );
	glDisableClientState(GL_VERTEX_ARRAY);
	*/

	// Version 2, arrow and base separated
	float verts0[1024][3] = {
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.5f }
	};

	int rot = 8;
	int v = 2;	
	for( int i=0; i<rot; i++ ) {
		float t0 = PI2F * ( (float)(i+0) / (float)rot );
		float t1 = PI2F * ( (float)(i+1) / (float)rot );

		verts0[v][0] = 0.1f * cosf(t0);
		verts0[v][1] = 0.1f * sinf(t0);
		verts0[v][2] = 0.0f;
		v++;
		
		verts0[v][0] = 0.1f * cosf(t0);
		verts0[v][1] = 0.1f * sinf(t0);
		verts0[v][2] = 0.5f;
		v++;
		
		verts0[v][0] = 0.1f * cosf(t1);
		verts0[v][1] = 0.1f * sinf(t1);
		verts0[v][2] = 0.0f;
		v++;
		
		verts0[v][0] = 0.1f * cosf(t1);
		verts0[v][1] = 0.1f * sinf(t1);
		verts0[v][2] = 0.5f;
		v++;
	}
	
	unsigned int indicies0[1024];

	int index = 0;	
	for( int i=0; i<rot; i++ ) {
		int q = i*4;
	
		indicies0[index++] = 0;
		indicies0[index++] = q + 4;
		indicies0[index++] = q + 2;
	
		indicies0[index++] = q + 2;
		indicies0[index++] = q + 4;
		indicies0[index++] = q + 3;
	
		indicies0[index++] = q + 4;
		indicies0[index++] = q + 5;
		indicies0[index++] = q + 3;
	
		indicies0[index++] = q + 3;
		indicies0[index++] = q + 5;
		indicies0[index++] = 1;
	}

	glColor3ub( 255, 0, 0 );
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer( 3, GL_FLOAT, 0, verts0 );
	glDrawElements( GL_TRIANGLES, index, GL_UNSIGNED_INT, indicies0 );
	glDisableClientState(GL_VERTEX_ARRAY);


	float verts1[1024][3] = {
		{ 0.0f, 0.0f, 0.5f },
		{ 0.0f, 0.0f, 1.0f }
	};

	rot = 8;
	v = 2;	
	for( int i=0; i<rot; i++ ) {
		float t0 = PI2F * ( (float)(i+0) / (float)rot );
		float t1 = PI2F * ( (float)(i+1) / (float)rot );

		verts1[v][0] = 0.2f * cosf(t0);
		verts1[v][1] = 0.2f * sinf(t0);
		verts1[v][2] = 0.5f;
		v++;
		
		verts1[v][0] = 0.2f * cosf(t1);
		verts1[v][1] = 0.2f * sinf(t1);
		verts1[v][2] = 0.5f;
		v++;
	}
	
	unsigned int indicies1[1024];

	index = 0;	
	for( int i=0; i<rot; i++ ) {
		int q = i*2;
	
		indicies1[index++] = 0;
		indicies1[index++] = q + 3;
		indicies1[index++] = q + 2;
	
		indicies1[index++] = q + 3;
		indicies1[index++] = q + 2;
		indicies1[index++] = 1;
	}

	glColor3ub( 255, 0, 255 );
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer( 3, GL_FLOAT, 0, verts1 );
	glDrawElements( GL_TRIANGLES, index, GL_UNSIGNED_INT, indicies1 );
	glDisableClientState(GL_VERTEX_ARRAY);


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
