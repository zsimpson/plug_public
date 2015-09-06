// OPERATING SYSTEM specific includes :
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
#include "ztmpstr.h"
#include "zviewpoint.h"
#include "zrand.h"
#include "ztime.h"
#include "zmathtools.h"
#include "zgltools.h"

ZPLUGIN_BEGIN( em );

ZVAR( float, Em_scale, 1.0 );


GLuint makeArrow() {
	GLuint index = glGenLists(1);
	glNewList(index,GL_COMPILE);
		const int segs = 16;
		const double r = 0.1;
		glBegin( GL_TRIANGLES );
		for( int i=0; i<segs; i++ ) {
			double t0 = PI2 * (i+0) / (double)segs;
			double t1 = PI2 * (i+1) / (double)segs;

			glNormal3d( 0.0, sin((t0+t1)/2.0), cos((t0+t1)/2.0) );

			glVertex3d( 0.0, r*sin(t0), r*cos(t0) );
			glVertex3d( 1.0, 0.0, 0.0 );
			glVertex3d( 0.0, r*sin(t1), r*cos(t1) );

			glNormal3d( -1.0, 0.0, 0.0 );
			glVertex3d( 0.0, r*sin(t0), r*cos(t0) );
			glVertex3d( 0.0, 0.0, 0.0 );
			glVertex3d( 0.0, r*sin(t1), r*cos(t1) );
		}
		glEnd();
	glEndList();
	return index;
}


void render() {
	glClear( GL_DEPTH_BUFFER_BIT );
	zviewpointSetupView();
	
	GLuint arrow = makeArrow();

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_NORMALIZE );
	glEnable( GL_LIGHTING );
	glEnable( GL_LIGHT0 );

	GLfloat lightPosition[] = { 1.0f, 1.0f, 1.0f, 0.0f };
	glLightfv( GL_LIGHT0, GL_POSITION, lightPosition );
	GLfloat lightAmbient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
	GLfloat lightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
	
	float electricMatDiffuse[] = { 1.0f, 0.5f, 0.5f, 1.0f };
	float electricMatAmbient[] = { 0.5f, 0.1f, 0.1f, 1.0f };
	float magneticMatDiffuse[] = { 0.5f, 0.5f, 1.0f, 1.0f };
	float magneticMatAmbient[] = { 0.1f, 0.1f, 0.5f, 1.0f };

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, electricMatDiffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, electricMatAmbient);
	
	const int steps = 17;
	const double stepsF = (double)steps;
	const double dimF = 10.0;

	double c1[16] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
	DVec3 charge( cos(zTime)+dimF/2.0, dimF/2.0, dimF/2.0 );

	for( int xi=0; xi<steps; xi++ ) {
		double x = (double)xi * dimF / stepsF;
		for( int yi=0; yi<steps; yi++ ) {
			double y = (double)yi * dimF / stepsF;
			for( int zi=0; zi<steps; zi++ ) {
				double z = (double)zi * dimF / stepsF;
				
				DVec3 q(x,y,z);
				q.sub( charge );
				double mag = q.mag();
				q.div( mag );
				
				c1[0] = q.x;
				c1[1] = q.y;
				c1[2] = q.z;
				DMat4 mat( c1 );
				mat.orthoNormalize();
				
				mag = Em_scale / (mag*mag);
				
				glPushMatrix();
					glTranslated( x, y, z );
					glScaled( mag, mag, mag );
					glMultMatrixd( (const GLdouble *)mat.m );
					glCallList( arrow );
				glPopMatrix();
			}
		}
	}
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

