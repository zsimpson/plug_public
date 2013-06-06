// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "float.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zwildcard.h"
#include "zmat.h"
#include "ztmpstr.h"
#include "zviewpoint.h"
#include "zgltools.h"
#include "zglfont.h"
#include "zmat_gsl.h"
#include "zglmatlight.h"
#include "colorpalette.h"

ZPLUGIN_BEGIN( fitsurf );

ZVAR( float, Fitsurf_colorScale, 9.2e1 );
ZVAR( float, Fitsurf_xScale, 1.00e+000 );
ZVAR( float, Fitsurf_yScale, 1.00e+000 );
ZVAR( float, Fitsurf_zScale, 6.10e-003 );
ZVAR( float, Fitsurf_zClip, 8.00e+003 );
ZVAR( float, Fitsurf_lightX0, 1.74e+002 );
ZVAR( float, Fitsurf_lightY0, 9.98e+001 );
ZVAR( float, Fitsurf_lightZ0, 6.84e-001 );
ZVAR( float, Fitsurf_lightX1, 1.89e+001 );
ZVAR( float, Fitsurf_lightY1, 1.04e+001 );
ZVAR( float, Fitsurf_lightZ1, 3.18e+003 );



#define DIM 100
int dim;
struct Sample {
	double r0;
	double r1;
	double error;
	ZMat grad;
	ZMat hess;
	ZMat u;
	ZMat s;
	ZMat vt;
	ZMat trajectory;
};

Sample samples[ DIM ][ DIM ];

void zglRenderSurface( FVec3 *pts, int xDim, int yDim, float colorScale ) {
	int x=0, y=0;

	FVec3 *norms = new FVec3[ xDim * yDim ];

	#define p(x,y) pts[(y)*xDim+(x)]
	#define n(x,y) norms[(y)*xDim+(x)]

	// COMPUTE norms
	for( y=0; y<yDim-1; y++ ) {
		for( x=0; x<xDim-1; x++ ) {
			FVec3 a = p(x,y+1);
			a.sub( p(x,y) );

			FVec3 b = p(x+1,y);
			b.sub( p(x,y) );

			a.cross( b );
			a.normalize();
			a.mul( -1.f );
			n(x,y) = a;
		}
	}

	// RENDER
	for( y=0; y<yDim-1; y++ ) {
		glBegin( GL_TRIANGLE_STRIP );
		for( x=0; x<xDim; x++ ) {
			float colorF = p(x,y).z * colorScale;
			int color = max( 0, min( 255, (int)colorF ) );
			glColor3ubv( colorPaletteBlueToRedUB256[color] );
			glNormal3fv( (const float *)&n(x,y) );
			glVertex3fv( (const float *)&p(x,y) );

			colorF = p(x,y+1).z * colorScale;
			color = max( 0, min( 255, (int)colorF ) );
			glColor3ubv( colorPaletteBlueToRedUB256[color] );
			glNormal3fv( (const float *)&n(x,y+1) );
			glVertex3fv( (const float *)&p(x,y+1) );
		}
		glEnd();
	}

	delete norms;
}


void render() {
	int x, y;

	glClearColor( 0.8f, 0.8f, 0.8f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float viewport[4];
	glGetFloatv( GL_VIEWPORT, viewport );
	gluPerspective( 30.f, viewport[2] / viewport[3], 0.001, 10000 );

	glMatrixMode(GL_MODELVIEW);
	gluLookAt( 0.0, 0.0, 10.f, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 );

	zviewpointSetupView();
	glScalef( Fitsurf_xScale, Fitsurf_yScale, Fitsurf_zScale );

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_NORMALIZE );

	FVec2 mouse = zviewpointProjMouseOnZ0Plane();
	mouse.x /= Fitsurf_xScale;
	mouse.y /= Fitsurf_yScale;

	float closest = 1e10f;
	int cx = 0;
	int cy = 0;

	FVec3 *pts = new FVec3[ dim * dim ];
	for( y=0; y<dim; y++ ) {
		for( x=0; x<dim; x++ ) {
			float err = (float)samples[x][y].error;
			err = min( err, Fitsurf_zClip );
			pts[ y*dim+x ] = FVec3( (float)samples[x][y].r0, (float)samples[x][y].r1, err );

			// FIND the closest point in the surface
			FVec2 c( mouse.x, mouse.y );
			c.sub( FVec2( (float)samples[x][y].r0, (float)samples[x][y].r1 ) );
			if( c.mag2() < closest ) {
				closest = c.mag2();
				cx = x;
				cy = y;
			}
		}
	}

	glEnable( GL_COLOR_MATERIAL );
	glEnable(GL_LIGHTING);

	glPolygonMode( GL_FRONT, GL_LINE );
	glPolygonMode( GL_BACK, GL_FILL );

	ZGLLight light0;
	light0.setLightNumber( 0 );
	light0.makePositional();
	light0.pos[0] = Fitsurf_lightX0;
	light0.pos[1] = Fitsurf_lightY0;
	light0.pos[2] = Fitsurf_lightZ0;
	light0.diffuse[0] = 1.f;
	light0.diffuse[1] = 1.f;
	light0.diffuse[2] = 1.f;
	light0.active = 1;
	glEnable( GL_LIGHT0 );
	light0.setGL();

	ZGLLight light1;
	light1.setLightNumber( 1 );
	light1.makePositional();
	light1.pos[0] = Fitsurf_lightX1;
	light1.pos[1] = Fitsurf_lightY1;
	light1.pos[2] = Fitsurf_lightZ1;
	light1.diffuse[0] = 1.f;
	light1.diffuse[1] = 1.f;
	light1.diffuse[2] = 1.f;
	light1.active = 1;
	glEnable( GL_LIGHT1 );
	light1.setGL();

	zglRenderSurface( pts, dim, dim, Fitsurf_colorScale );

	glDisable( GL_LIGHTING );
	glDisable( GL_DEPTH_TEST );

	glColor3ub( 0, 0, 0 );
	glBegin( GL_POINTS );
	for( y=0; y<dim; y++ ) {
		for( x=0; x<dim; x++ ) {
			glVertex3f( (float)samples[x][y].r0, (float)samples[x][y].r1, 0.f );
		}
	}
	glEnd();

	zglDrawStandardAxis();

	float cr0 = (float)samples[cx][cy].r0;
	float cr1 = (float)samples[cx][cy].r1;
	float cz  = (float)samples[cx][cy].error;

	// DRAW the mouse point
	glColor3ub( 255, 255, 255 );
	glBegin( GL_LINES );
		glVertex2f( cr0, cr1 );
		glVertex3f( cr0, cr1, cz );
	glEnd();

	glDisable( GL_DEPTH_TEST );
	glPointSize( 4.f );
	glBegin( GL_POINTS );
		glVertex3f( cr0, cr1, cz );
	glEnd();

	// FETCH the saved full forms
	ZMat &g = samples[cx][cy].grad;
	ZMat &h = samples[cx][cy].hess;
	ZMat &u = samples[cx][cy].u;
	ZMat &s = samples[cx][cy].s;
	ZMat &vt = samples[cx][cy].vt;
	ZMat &traj = samples[cx][cy].trajectory;

	// DRAW the trajectory
	glPointSize( 4.f );
	glColor3ub( 0, 0, 255 );
	glBegin( GL_POINTS );
	for( int c=0; c<traj.cols; c++ ) {
		glVertex3d( traj.getD(0,c), traj.getD(1,c), traj.getD(traj.rows-1,c) );
	}
	glEnd();

	// EXTRACT compact forms
	ZMat gc( 2, 1, zmatF64 );
	gc.putD( 0,0, -g.getD(0,0) );
	gc.putD( 1,0, -g.getD(1,0) );

	ZMat hc( 2, 2, zmatF64 );
	hc.putD( 0,0, h.getD(0,0) );
	hc.putD( 1,0, h.getD(1,0) );
	hc.putD( 0,1, h.getD(0,1) );
	hc.putD( 1,1, h.getD(1,1) );

	// SVD on the compact form
	ZMat uc, sc, vtc;
	zmatSVD_GSL( hc, uc, sc, vtc );

	// DRAW the gradient
	glColor3ub( 255, 0, 0 );
	zglDrawVector( cr0, cr1, cz, (float)gc.getD(0,0), (float)gc.getD(1,0), 0.f );

	// COMPUTE the delta from the compact form
	ZMat utc;
	zmatTranspose( uc, utc );

	ZMat utcgc;
	zmatMul( utc, gc, utcgc );

	for( int r=0; r<utcgc.rows; r++ ) {
		if( sc.getD(r,0) > FLT_EPSILON ) {
			double temp = utcgc.getD(r,0) / sc.getD(r,0);
			utcgc.putD( r, 0, temp );
		}
		else {
			utcgc.putD( r, 0, 0.0 );
		}
	}

	ZMat vttc;
	zmatTranspose( vtc, vttc );

	ZMat stepSol;
	zmatMul( vttc, utcgc, stepSol );

	glLineWidth( 3.f );
	glColor3ub( 255, 255, 0 );
	zglDrawVector( cr0, cr1, cz, (float)stepSol.getD(0,0), (float)stepSol.getD(1,0), 0.f );

	// DRAW u
	float s0 = (float)sc.getD(0,0);
	float s1 = (float)sc.getD(1,0);

	glColor3ub( 0, 0, 0 );
	zglDrawVector( cr0, cr1, cz, s0 * (float)uc.getD(0,0), s0 * (float)uc.getD(1,0), 0.f );
	zglDrawVector( cr0, cr1, cz, s1 * (float)uc.getD(0,1), s1 * (float)uc.getD(1,1), 0.f );


	delete pts;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	zglPixelMatrixInvertedFirstQuadrant();

	glColor3ub( 0, 0, 0 );

	float fx = 0.f;
	float fy = 0.f;
	zglFontPrintInverted( ZTmpStr( "r0 = %lf", cr0 ), fx, fy, "controls" ); fy += 12.f;
	zglFontPrintInverted( ZTmpStr( "r1 = %lf", cr1 ), fx, fy, "controls" ); fy += 12.f;
	zglFontPrintInverted( ZTmpStr( "err = %lf", cz ), fx, fy, "controls" ); fy += 12.f;
	zglFontPrintInverted( ZTmpStr( "gradR0 = %lf", samples[cx][cy].grad.getD(0,0) ), fx, fy, "controls" ); fy += 12.f;
	zglFontPrintInverted( ZTmpStr( "gradR1 = %lf", samples[cx][cy].grad.getD(1,0) ), fx, fy, "controls" ); fy += 12.f;
}

void startup() {
	colorPaletteInit();

	int dimr0=0, dimr1=0;

//	char *dir = "/transfer/fitsurf1";
	char *dir = "/transfer/fitsurf2";

	int count0 = 0;
	ZWildcard w0( ZTmpStr("%s/fitsurf-point-*.txt", dir) );
	while( w0.next() ) {
		count0++;
	}

	int count = 0;
	ZWildcard w( ZTmpStr("%s/fitsurf-point-*.txt",dir) );
	while( w.next() ) {
		printf( "%f%%\r", 100.f * (float)count / (float)count0 );
		count++;

		char *n = w.getFullName();
		FILE *f = fopen( n, "rt" );
		assert( f );
		int r0i, r1i;
		double r0, r1, error;
		fscanf( f, "%d %d %lf %lf %lf\n", &r0i, &r1i, &r0, &r1, &error );
		fclose( f );

		samples[r0i][r1i].r0 = r0;
		samples[r0i][r1i].r1 = r1;
		samples[r0i][r1i].error = error;

		samples[r0i][r1i].grad.loadCSV( ZTmpStr("%s/fitsurf-grad-%02d-%02d.txt", dir, r0i, r1i) );
		samples[r0i][r1i].hess.loadCSV( ZTmpStr("%s/fitsurf-hess-%02d-%02d.txt", dir, r0i, r1i) );
		samples[r0i][r1i].u.loadCSV( ZTmpStr("%s/fitsurf-u-%02d-%02d.txt", dir, r0i, r1i) );
		samples[r0i][r1i].s.loadCSV( ZTmpStr("%s/fitsurf-s-%02d-%02d.txt", dir, r0i, r1i) );
		samples[r0i][r1i].vt.loadCSV( ZTmpStr("%s/fitsurf-vt-%02d-%02d.txt", dir, r0i, r1i) );
		samples[r0i][r1i].trajectory.loadCSV( ZTmpStr("%s/fitsurf-trajectory-%02d-%02d.txt", dir, r0i, r1i) );

		dimr0 = max( dimr0, r0i );
		dimr1 = max( dimr1, r1i );
	}

	assert( dimr0 == dimr1 );
	dim = dimr0 + 1;
	assert( dim <= DIM );
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

