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

GLuint arrow = 0;

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

DVec3 rectToSpherePos( DVec3 a ) {
	DVec3 b;
	
	// r
	b.x = sqrt( a.x*a.x + a.y*a.y + a.z*a.z );
	
	// theta
	b.y = acos( a.z / b.x );
	
	// phi
	b.z = atan2( a.x, a.y );
	
	return b;
}

DVec3 sphereToRectPos( DVec3 a ) {
	DVec3 b;
	
	b.x = a.x * sin(a.y) * cos(a.z);
	b.y = a.x * sin(a.y) * sin(a.z);
	b.z = a.x * cos(a.y);
	
	return b;
}

DMat3 sphereToRectUnitVectors( double theta, double phi ) {
	DMat3 a;
	double st = sin(theta);
	double ct = cos(theta);
	double sp = sin(phi);
	double cp = cos(phi);
	a.m[0][0] = st * cp;
	a.m[1][0] = ct * cp;
	a.m[2][0] = -sp;
	a.m[0][1] = st * sp;
	a.m[1][1] = ct * sp;
	a.m[2][1] = cp;
	a.m[0][2] = ct;
	a.m[1][2] = -st;
	a.m[2][2] = 0.0;
	return a;
}

DMat3 rectToSphereUnitVectors( double theta, double phi ) {
	DMat3 a;
	double st = sin(theta);
	double ct = cos(theta);
	double sp = sin(phi);
	double cp = cos(phi);
	a.m[0][0] = st * cp;
	a.m[1][0] = st * sp;
	a.m[2][0] = ct;
	a.m[0][1] = ct * cp;
	a.m[1][1] = ct * sp;
	a.m[2][1] = -st;
	a.m[0][2] = -sp;
	a.m[1][2] = cp;
	a.m[2][2] = 0.0;
	return a;
}

void arrowInUnitDirecton( DVec3 pos, DVec3 dir, double mag ) {
	static double arrowMat[16] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
	arrowMat[0] = dir.x;
	arrowMat[1] = dir.y;
	arrowMat[2] = dir.z;
	DMat4 mat( arrowMat );
	mat.orthoNormalize();
	glPushMatrix();
		glTranslated( pos.x, pos.y, pos.z );
		glScaled( mag, mag, mag );
		glMultMatrixd( (const GLdouble *)mat.m );
		glCallList( arrow );
	glPopMatrix();
}

void render() {
	glClear( GL_DEPTH_BUFFER_BIT );
	zviewpointSetupView();

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
	const double dimF = 17.0;

	double c1[16] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
	//DVec3 charge( cos(zTime)+dimF/2.0, dimF/2.0, dimF/2.0 );

	for( int xi=1; xi<steps; xi++ ) {
		double x = (double)xi * dimF / stepsF;
		for( int yi=1; yi<steps; yi++ ) {
			double y = (double)yi * dimF / stepsF;
			for( int zi=1; zi<steps; zi++ ) {
				double z = (double)zi * dimF / stepsF;
			
				DVec3 rect0(x-dimF/2.0,y-dimF/2.0,z-dimF/2.0);
				DVec3 sphe0 = rectToSpherePos( rect0 );
				
				DVec3 eField;
				double omega = 1.0;
				double beta = 2.0;
				double ot = omega * zTime - beta * sphe0.r;
				double eField_rReal_inside = (2.0 * omega) / (beta * sphe0.r * sphe0.r) * cos(sphe0.t);
				double eField_rImag_inside = -(2.0 * omega) / (beta*beta * sphe0.r * sphe0.r * sphe0.r) * cos(sphe0.t);
				double eField_tReal_inside = omega / (beta * sphe0.r * sphe0.r) * sin(sphe0.t);
				double eField_tImag_inside = ( -omega / (beta*beta * sphe0.r * sphe0.r * sphe0.r) + omega / sphe0.r ) * sin(sphe0.t);
				double eField_pReal_inside = 0.0;
				double eField_pImag_inside = 0.0;

				eField_rReal_inside = 0.0;
				eField_rImag_inside = 0.0;
				eField_tReal_inside = 1.0;
				eField_tImag_inside = 0.0;
				
				double eField_rReal = eField_rReal_inside * cos(ot) - eField_rImag_inside * sin(ot);
				double eField_rImag = eField_rImag_inside * cos(ot) + eField_rReal_inside * sin(ot);
				double eField_tReal = eField_tReal_inside * cos(ot) - eField_tImag_inside * sin(ot);
				double eField_tImag = eField_tImag_inside * cos(ot) + eField_tReal_inside * sin(ot);
				double eField_pReal = eField_pReal_inside * cos(ot) - eField_pImag_inside * sin(ot);
				double eField_pImag = eField_pImag_inside * cos(ot) + eField_pReal_inside * sin(ot);
		
				
				DMat3 uv = rectToSphereUnitVectors( sphe0.t, sphe0.p );
				DVec3 eFieldInRectReal = uv.mul( DVec3( eField_rReal, eField_tReal, eField_pReal ) );
				DVec3 eFieldInRectImag = uv.mul( DVec3( eField_rImag, eField_tImag, eField_pImag ) );
				
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, electricMatDiffuse);
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, electricMatAmbient);
				double eFieldInRectRealMag = eFieldInRectReal.mag();
				DVec3 eFieldInRectRealUnit = eFieldInRectReal;
				eFieldInRectRealUnit.div( eFieldInRectRealMag );
				double logMagReal = Em_scale*log(1.0 + eFieldInRectRealMag);
				arrowInUnitDirecton( rect0, eFieldInRectRealUnit, logMagReal );

				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, magneticMatDiffuse);
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, magneticMatAmbient);
				double eFieldInRectImagMag = eFieldInRectImag.mag();
				DVec3 eFieldInRectImagUnit = eFieldInRectImag;
				eFieldInRectImagUnit.div( eFieldInRectImagMag );
				double logMagImag = Em_scale*log(1.0 + eFieldInRectImagMag);
				arrowInUnitDirecton( rect0, eFieldInRectImagUnit, logMagImag );


				// PLOT e from charge
				/*
				DVec3 q = rect1;
				q.sub( charge );
				double mag = q.mag();
				q.div( mag );
				mag = Em_scale*log(1.0 + 1.0 / (mag*mag));
				arrowInUnitDirecton( rect1, q, mag );
				*/
			}
		}
	}
}

void startup() {
	arrow = makeArrow();
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

