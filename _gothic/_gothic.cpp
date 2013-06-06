// @ZBS {
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "assert.h"
#include "string.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zvec.h"
#include "zmathtools.h"
#include "zviewpoint.h"
#include "zglmatlight.h"


ZPLUGIN_BEGIN( gothic );

ZVAR( double, Gothic_turn, 3.14e-2 );
ZVAR( double, Gothic_translate, -2.76e+1 );


struct Patch {
	DVec3 *verts;
	DVec3 *vertNorms;
	int vertCount;
	int *faces;
	DVec3 *faceNorms;
	int faceCount;


	Patch() {
		verts = 0;
		vertNorms = 0;
		vertCount = 0;
		faces = 0;
		faceNorms = 0;
		faceCount = 0;
	}
	
	~Patch() {
		dealloc();
	}
	
	void alloc( int vertCount, int faceCount ) {
		this->vertCount = vertCount;
		this->faceCount = faceCount;
		verts = new DVec3[ vertCount ];
		vertNorms = new DVec3[ vertCount ];
		faces = new int[ faceCount * 3 ];
		faceNorms = new DVec3[ faceCount ];
	}
	
	void dealloc() {
		delete verts;
		delete vertNorms;
		delete faceNorms;
	}
	
	void renderVerts() {
		glBegin( GL_POINTS );
		for( int v=0; v<vertCount; v++ ) {
			glVertex3dv( verts[ v ] );
		}
		glEnd();
	}
	
	void renderFaces() {
		// @TODO Convert to GL lists?
		glBegin( GL_TRIANGLES );
		for( int f=0; f<faceCount; f++ ) {
			glNormal3dv( faceNorms[f] );
			glVertex3dv( verts[ faces[f*3+0] ] );
			glVertex3dv( verts[ faces[f*3+1] ] );
			glVertex3dv( verts[ faces[f*3+2] ] );
		}
		glEnd();
	}

	void translate( DVec3 trans ) {
		// FOREACH vertex solve the warp equation
		DVec3 *vert = this->verts;
		for( int v=0; v<this->vertCount; v++ ) {
			vert->add( trans );
			vert++;
		}
	}

	void scale( DVec3 scale ) {
		// FOREACH vertex solve the warp equation
		DVec3 *vert = this->verts;
		for( int v=0; v<this->vertCount; v++ ) {
			vert->x *= scale.x;
			vert->y *= scale.y;
			vert->z *= scale.z;
			vert++;
		}
	}

	void bend( double turn ) {
		// FOREACH vertex solve the warp equation
		DVec3 *vert = this->verts;
		double radius = 1.0 / turn;
		for( int v=0; v<this->vertCount; v++ ) {
			double cy = cos( PI * vert->y * turn );
			double sy = sin( PI * vert->y * turn );
			double x = radius * cy + vert->x * cy - radius; 
			double y = radius * sy + vert->x * sy;
			vert->x = x;
			vert->y = y;
			vert++;
		}
	}

};

Patch *patchCylinder( int radialDivs=16, int vertDivs=16 ) {
	int s;

	Patch *p = new Patch;
	int vertCount = radialDivs * (vertDivs+1);
	int faceCount = radialDivs * (vertDivs  ) * 2;

	p->alloc( vertCount, faceCount );

	DVec3 *vert = p->verts;
	int vc = 0;
	for( s=0; s<=vertDivs; s++ ) {
		double y = (double)s / (double)vertDivs;

		for( int r=0; r<radialDivs; r++ ) {
			double t = PI2F * (double)r / (double)radialDivs;
			double x = cos(	t );
			double z = sin(	t );
			*vert++ = DVec3( x, y, z );
			vc ++;
		}
	}
	assert( vc == vertCount );

	int fc = 0;
	int *face = p->faces;
	DVec3 *faceNorm = p->faceNorms;
	for( s=0; s<vertDivs; s++ ) {
		for( int r=0; r<radialDivs; r++ ) {
			*face++ = s*radialDivs + r;
			*face++ = (s+1)*radialDivs + r;
			*face++ = (s+1)*radialDivs + (r+1) % radialDivs;
			DVec3 n = p->verts[ s*radialDivs + r ];
			n.y = 0.0;
			*faceNorm++ = n;

			*face++ = (s+1)*radialDivs + (r+1) % radialDivs;
			*face++ = s*radialDivs + r;
			*face++ = s*radialDivs + (r+1) % radialDivs;
			
			n = p->verts[ s*radialDivs + r ];
			n.y = 0.0;
			*faceNorm++ = n;
			
			fc += 2;
		}
	}
	assert( fc == faceCount );
	
	return p;
}

Patch *cyl;


void render() {
	glClearColor( 1.f, 1.f, 1.f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float viewport[4];
	glGetFloatv( GL_VIEWPORT, viewport );
	gluPerspective( 30.f, viewport[2] / viewport[3], 0.1, 100.0 );
	glMatrixMode(GL_MODELVIEW);
	gluLookAt( 0.0, 0.0, 5.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 );
	zviewpointSetupView();

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_LIGHTING );
	glEnable( GL_LIGHT0 );
	glEnable( GL_NORMALIZE );
	ZGLLight light;
	light.makeDirectional();
	light.diffuse[0] = 1.f;
	light.diffuse[1] = 1.f;
	light.diffuse[2] = 1.f;
	light.dir[0] = 2.f;
	light.dir[1] = 2.f;
	light.dir[2] = 0.f;
	light.active = 1;
	light.setGL();

	Patch *cyl0;
	cyl0 = patchCylinder( 64, 100 );
	cyl0->scale( DVec3( 1.0, 10.0, 1.0 ) );
	cyl0->translate( DVec3( 0.0, 0.0, 0.0 ) );
	cyl0->bend( Gothic_turn );

	Patch *cyl1;
	cyl1 = patchCylinder( 64, 100 );
	cyl1->scale( DVec3( 1.0, 10.0, 1.0 ) );
	cyl1->bend( Gothic_turn );
	cyl1->scale( DVec3( -1.0, 1.0, 1.0 ) );
	cyl1->translate( DVec3( Gothic_translate, 0.0, 0.0 ) );

	cyl0->renderFaces();
	cyl1->renderFaces();

	delete cyl0;
	delete cyl1;
}

void startup() {
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

