// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "float.h"
#include "string.h"
#include "math.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "ztl.h"
#include "zvec.h"
#include "zviewpoint.h"
#include "zmousemsg.h"
#include "zmathtools.h"

ZPLUGIN_BEGIN( path );

ZVAR( float, Path_connectorW, 4.0 );
ZVAR( float, Path_connectorH, 1.0 );
ZVAR( int, Path_writeG, 0 );



typedef ZTLVec<FVec2> Poly2D;

struct CutPath2D {
	Poly2D verts;
	Poly2D toolPathVerts;

	void vertexAdd( FVec2 p );
	int vertexFindClosest( FVec2 p );
	void vertexMove( int v, FVec2 p );

	FVec2 norm( int v );
		// The face normal defined by the edge that *begins* with vertex v

	void computeToolPath();

	void render();
	void clear();
	void save();
	void open();
	void toG();
};

void CutPath2D::vertexAdd( FVec2 p ) {
	// @TODO: There will probably be a concept of a "slected" vertex and
	// adding a vertex would logically happen next to the selected one
	verts.add( p );
	

}

int CutPath2D::vertexFindClosest( FVec2 p ) {
	int closest = -1;
	float closestMag2 = FLT_MAX;

	for( int i=0; i<verts.count; i++ ) {
		FVec2 d = p;
		d.sub( verts[i] );
		float mag2 = d.mag2();
		if( mag2 < closestMag2 ) {
			closest = i;
			closestMag2 = mag2;
		}
	}
	return closest;
}

void CutPath2D::vertexMove( int v, FVec2 p ) {
	if( v >= 0 && v < verts.count ) {
		verts[v] = p;
	}
}

void CutPath2D::clear() {
	verts.clear();
	toolPathVerts.clear();
}

void CutPath2D::toG() {
	float depth = -0.05f;
	float passes = 3;
	FILE *f = fopen( "test.nc", "wt" );
	fprintf( f, "%% \n");
	fprintf( f, "g20 g00 x0 y0 z0\n");
	for( int p=1; p<=passes; p++)
	{
		fprintf( f, "x%f y%f z%f\n", toolPathVerts[0].x, toolPathVerts[0].y, (depth * p) - depth);
		for( int i=0; i<toolPathVerts.count; i++ ) {
			fprintf( f, "x%f y%f z%f\n", toolPathVerts[i].x, toolPathVerts[i].y, depth * p);
		}
		fprintf( f, "x%f y%f z%f\n", toolPathVerts[0].x, toolPathVerts[0].y, depth * p);
	}
	fprintf( f, "%% \n");
	fclose( f );
}

void CutPath2D::save() {
	FILE *f = fopen( "saved.2shape", "wt" );
	for( int p=0; p<verts.count; p++) {
		fprintf( f, "%f ", verts[p].x);
		fprintf( f, "%f ", verts[p].y);
	}
	fclose( f );

}

void CutPath2D::open() {
	verts.clear();
	float t1 = 0.f;
	float t2 = 0.f; 
	FILE *f = fopen( "saved.2shape", "rt" );
	while( 1 ) {
		int read = fscanf (f, "%f %f\n", &t1, &t2);
		if( read == 2 ) {
			verts.add( FVec2(t1,t2) );
		}
		else {
			break;
		}
	}
	fclose( f );

}

void CutPath2D::render() {
	int i;

	//RENDER a grid for scaling
	glColor3ub( 200, 255, 200);
	glBegin( GL_LINES );
	for( i=-10; i<10; i++ ) {
		glVertex2f( (float)i, -10.f );
		glVertex2f( (float)i, 10.f );
		glVertex2f( -10.f, (float)i );
		glVertex2f( 10.f, (float)i );
	}
	glEnd();

	glColor3ub( 0, 0, 0 );
	glPointSize( 4.f );
	glBegin( GL_POINTS );
	glVertex2f( 0.f, 0.f );
	glEnd();

	// RENDER the object verts in black
	glColor3ub( 0, 0, 0 );
	glPointSize( 4.f );
	glBegin( GL_POINTS );
	for( i=0; i<verts.count; i++ ) {
		glVertex2fv( verts[i] );
	}
	glEnd();

	glBegin( GL_LINE_LOOP );
	for( i=0; i<verts.count; i++ ) {
		glVertex2fv( verts[i] );
	}
	glEnd();

	// RENDER tool verts in blue
	glColor3ub( 0, 0, 255 );
	glPointSize( 4.f );
	glBegin( GL_POINTS );
	for( i=0; i<toolPathVerts.count; i++ ) {
		glVertex2fv( toolPathVerts[i] );
	}
	glEnd();

	glBegin( GL_LINE_LOOP );
	for( i=0; i<toolPathVerts.count; i++ ) {
		glVertex2fv( toolPathVerts[i] );
	}
	glEnd();
}

void CutPath2D::computeToolPath() {
	// @TODO
	// WALK along the path
	// COMPUTE the normal for face i
	// CREATE a tool vertex that is offset by the tool radius along the normal
	// DEAL with accute angles that will require extending the vertex om tj edorectopm pf the face
	// ADD tot he tool verts
	
	const float toolRadius = 1.f / 16;
	
	toolPathVerts.clear();

	// @TODO
	// LOOP though the whole path summing the cross products (z terms)
	// if the sum is positive it is wound one way, neg the other
	// if it is wound wrong, invert it 
	int i;
	float sum = 0.f;
	for( i=0; i<verts.count; i++ ) {
		//SOLVE for d0
		FVec2 d0 = verts[(i + 1) % verts.count];
		d0.sub(verts[i]);

		//SOLVE for d1
		FVec2 d1 = verts[(i + 2) % verts.count];
		d1.sub(verts[(i + 1) % verts.count]);

		FVec3 _d0(d0);
		FVec3 _d1(d1);

		_d0.cross(_d1);
		sum+=_d0.z;
	}

	float sign = sum > 0.f ? -1.f : 1.f;

	for( i=0; i<verts.count; i++ ) {
		//SOLVE for d0
		FVec2 d0 = verts[(i + 1) % verts.count];
		d0.sub(verts[i]);

		//SOLVE for d1
		FVec2 d1 = verts[(i + 2) % verts.count];
		d1.sub(verts[(i + 1) % verts.count]);

		//SOLVE for v0
		FVec2 v0 = d0;
		v0.perp();
		v0.normalize();
		v0.mul(sign * toolRadius);

		//SOLVE for v1
		FVec2 v1 = d1;
		v1.perp();
		v1.normalize();
		v1.mul(sign * toolRadius);

		//SOLVE for p0
		FVec2 p0 = verts[i];
		p0.add(v0);

		//SOLVE for p1
		FVec2 p1 = verts[(i+1)%verts.count];
		p1.add(v1);

		FMat2 m;
		m.m[0][0] = d0.x;
		m.m[0][1] = d0.y;
		m.m[1][0] = -d1.x;
		m.m[1][1] = -d1.y;

		FVec2 b = FVec2( -p0.x + p1.x, -p0.y + p1.y );

		if( m.inverse() ) {
			FVec2 x = m.mul( b );
			FVec2 point( p0.x + x.x * d0.x, p0.y + x.x * d0.y );
			toolPathVerts.add(point);
		}

//toolPathVerts.add(p0);
//toolPathVerts.add(p1);
	}
}

//--------------------------------------------------------------------------------------------------


// For now, shape generation is done here in free functions


void createSym( Poly2D &verts ) {
	float t = 1.f / 8.f;
	float angleDeg = 60.f;
	float angleRad = angleDeg / 360.f * PI2F;
	float dist = .3f;
	float wid = t / tanf(angleRad/2.f); 
	verts.add(FVec2(0.f,0.f));

	float w = Path_connectorW;
	float h = Path_connectorH;

	verts.add(FVec2(w*0.1f,0.f));
	verts.add(FVec2(w*0.1f,h *0.5f));
	verts.add(FVec2(w*0.1f + wid,h * 0.5f));
	verts.add(FVec2(w*0.1f + wid,0.f));

	verts.add(FVec2(w,0.f));
	verts.add(FVec2(w,h));

	verts.add(FVec2(w*0.9f,h));
	verts.add(FVec2(w*0.9f,h * 0.5f));
	verts.add(FVec2(w*0.9f - wid,h * 0.5f));
	verts.add(FVec2(w*0.9f - wid,h));

	verts.add(FVec2(0.f,h));
}

void createConnector( Poly2D &verts ) {
	float t = 1.f / 8.f;
	float angleDeg = 60.f;
	float angleRad = angleDeg / 360.f * PI2F;
	float dist = .3f;
	float wid = t / tanf(angleRad/2.f); 
	verts.add(FVec2(0.f,0.f));

	float w = Path_connectorW;
	float h = Path_connectorH;

	verts.add(FVec2(w*0.1f,0.f));
	verts.add(FVec2(w*0.1f,h *0.5f));
	verts.add(FVec2(w*0.1f + wid,h * 0.5f));
	verts.add(FVec2(w*0.1f + wid,0.f));

	verts.add(FVec2(w,0.f));
	verts.add(FVec2(w,h));

	verts.add(FVec2(w*0.9f,h));
	verts.add(FVec2(w*0.9f,h * 0.5f));
	verts.add(FVec2(w*0.9f - wid,h * 0.5f));
	verts.add(FVec2(w*0.9f - wid,h));

	verts.add(FVec2(0.f,h));
}


//--------------------------------------------------------------------------------------------------


void render() {
	zviewpointSetupView();

	CutPath2D path;
	createConnector( path.verts );

	path.computeToolPath();
	path.render();

	if( Path_writeG ) {
		Path_writeG = 0;
		path.toG();
	}
}

void startup() {
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

//	static int draggingVertex = -1;
//	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
//		// FIND the closest point in the path and drag it
//		FVec2 mouse = zviewpointProjMouseOnZ0Plane();
//		draggingVertex = path.vertexFindClosest( mouse );
//		zMouseMsgRequestExclusiveDrag( "type=Path_MouseDrag" );
//	}
//	else if( draggingVertex >= 0 && zmsgIs(type,Path_MouseDrag) ) {
//		FVec2 mouse = zviewpointProjMouseOnZ0Plane();
//		path.vertexMove( draggingVertex, mouse );
//		if( zmsgI(releaseDrag) ) {
//			zMouseMsgCancelExclusiveDrag();
//			draggingVertex = -1;
//		}
//	}
//
//	else if( zmsgIs(type,Key) && zmsgIs(which,n) ) {
//		FVec2 mouse = zviewpointProjMouseOnZ0Plane();
//		path.vertexAdd( mouse );
//	}

	else if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		Path_writeG = 1;
//		path.clear();
	}
//	else if( zmsgIs(type,Key) && zmsgIs(which,w) ) {
//		path.computeToolPath();
//	}
//	else if( zmsgIs(type,Key) && zmsgIs(which,e) ) {
//		path.toG();
//	}
//	else if( zmsgIs(type,Key) && zmsgIs(which,s) ) {
//		path.save();
//	}
//	else if( zmsgIs(type,Key) && zmsgIs(which,o) ) {
//		path.open();
//	}


}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

