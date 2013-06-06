// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zviewpoint.h"
#include "zmathtools.h"
#include "zrgbhsv.h"
#include "zgltools.h"
#include "zglmatlight.h"
#include "zstr.h"
#include "zhashtable.h"
#include "ztmpstr.h"
#include "ztl.h"
#include "zvec.h"

extern ZHashTable options;

ZPLUGIN_BEGIN( plasma );


ZVARB( int, Plasma_drawHelicies, 1, 0, 1 );
ZVARB( int, Plasma_drawHelix2, 1, 0, 1 );
ZVARB( int, Plasma_drawHelix3, 0, 0, 1 );
ZVARB( int, Plasma_drawHelix4, 0, 0, 1 );


ZVAR( float, Plasma_magnetScale, 0.5 );
ZVAR( float, Plasma_magnetRotX, 90 );
ZVAR( float, Plasma_magnetRotY, 1 );
ZVAR( float, Plasma_magnetRotZ, 1 );
ZVAR( float, Plasma_m, 4.42 );
ZVAR( float, Plasma_n, 3.97 );
ZVAR( float, Plasma_tfColor, 0.34 );
ZVAR( float, Plasma_tfAlpha, 1 );

ZVAR( float, Plasma_bumpW, 4.4e-2 );
ZVAR( float, Plasma_bumpAmp, 0.1 );
ZVAR( float, Plasma_bumpOffset,-2.7e-1 );

ZVAR( float, Plasma_coldPlasmaX, 1.15 );
ZVAR( float, Plasma_coldPlasmaY, 0.1 );

ZVAR( float, Plasma_hotPlasmaX, 1.06 );
ZVAR( float, Plasma_hotPlasmaY, 0.1 );

ZVAR( float, Plasma_radialOffset, 1.4 );
ZVAR( float, Plasma_helixScale, 1 );
ZVAR( float, Plasma_helixSpin, 1 );
ZVAR( float, Plasma_helixSpinSpeed, 1 );


ZTLVec<FVec3> helix[3];

void drawXYPlaneMagnet() {
	glBegin( GL_TRIANGLES );
		glColor3ub( 255, 0, 0 );
		glVertex2f( -0.1f, 0.f );
		glVertex2f( +0.1f, 0.f );
		glVertex2f( 0.f, 1.f );
		glColor3ub( 255, 255, 255 );
		glVertex2f( -0.1f, 0.f );
		glVertex2f( +0.1f, 0.f );
		glVertex2f( 0.f, -1.f );
	glEnd();

	glColor3ub( 255, 255, 255 );
	glBegin( GL_LINE_STRIP );
	for( int i=0; i<=64; i++ ) {
		float angle = PI2F*(float)i/64.f;
		glVertex3f( 0.1f*cosf(angle), 0.f, 0.1f*sinf(angle) );
	}
	glEnd();

	glBegin( GL_LINES );
		glVertex3f( 0.f, 0.f, -0.1f );
		glVertex3f( 0.f, 0.f, +0.1f );
		glVertex3f( +0.1f, 0.f, 0.f );
		glVertex3f( -0.1f, 0.f, 0.f );
	glEnd();
}

void drawCube() {
	glBegin( GL_LINE_LOOP );
		glVertex3f( +1.f, -1.f, -1.f );
		glVertex3f( +1.f, +1.f, -1.f );
		glVertex3f( -1.f, +1.f, -1.f );
		glVertex3f( -1.f, -1.f, -1.f );
	glEnd();

	glBegin( GL_LINE_LOOP );
		glVertex3f( +1.f, -1.f, +1.f );
		glVertex3f( +1.f, +1.f, +1.f );
		glVertex3f( -1.f, +1.f, +1.f );
		glVertex3f( -1.f, -1.f, +1.f );
	glEnd();

	glBegin( GL_LINES );
		glVertex3f( +1.f, -1.f, -1.f );
		glVertex3f( +1.f, -1.f, +1.f );

		glVertex3f( +1.f, +1.f, -1.f );
		glVertex3f( +1.f, +1.f, +1.f );

		glVertex3f( -1.f, +1.f, -1.f );
		glVertex3f( -1.f, +1.f, +1.f );

		glVertex3f( -1.f, -1.f, -1.f );
		glVertex3f( -1.f, -1.f, +1.f );
	glEnd();
}

void drawXYPlaneCircle() {
	glBegin( GL_LINE_STRIP );
	for( int i=0; i<=64; i++ ) {
		float angle = PI2F*(float)i/64.f;
		glVertex2f( cosf(angle), sinf(angle) );
	}
	glEnd();
}

void drawXYPlaneDisc() {
	glBegin( GL_TRIANGLE_FAN );
	for( int i=0; i<=64; i++ ) {
		float angle = PI2F*(float)i/64.f;
		glVertex2f( cosf(angle), sinf(angle) );
	}
	glEnd();
}

void render() {
	glClearColor( 0.8f, 0.8f, 0.8f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float viewport[4];
	glGetFloatv( GL_VIEWPORT, viewport );
	gluPerspective( 30.f, viewport[2] / viewport[3], 0.01f, 10.0f );
	glMatrixMode(GL_MODELVIEW);
	gluLookAt( 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 );
	zviewpointSetupView();

/*
	// DRAW the TF lines
	glColor3ub( 0, 255, 0 );
	for( float y=-1.f; y<1.f; y+=2.f/20.f ) {
		glPushMatrix();
			glTranslatef( -Plasma_radialOffset, y, 0.f );
			glRotatef( 90.f, 1.f, 0.f, 0.f );
			drawXYPlaneCircle();
		glPopMatrix();
	}
*/

	if( ! Plasma_drawHelicies ) {
		// DRAW the TF density
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glPushMatrix();
			glTranslatef( -Plasma_radialOffset, 0.f, 0.f );
			glBegin( GL_QUAD_STRIP );
				for( float x=0.f; x<=3.f; x+=3.f/20.f ) {
					glColor4f( Plasma_tfColor * x, Plasma_tfColor * x, Plasma_tfColor * x, Plasma_tfAlpha );
					glVertex3f( x, -1.f, 0.01f );
					glVertex3f( x, +1.f, 0.01f );
				}
			glEnd();
		glPopMatrix();
		glDisable( GL_BLEND );
	}

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_LIGHTING );
	glEnable( GL_NORMALIZE );

	// DRAW the core cylinder
	ZGLLight light0;
	light0.setLightNumber( 0 );
	light0.makeDirectional();
	light0.dir[0] = 0.18f;
	light0.dir[1] = 1.06f;
	light0.dir[2] = 0.16f;
	light0.diffuse[0] = 1.f;
	light0.diffuse[1] = 1.f;
	light0.diffuse[2] = 1.f;
	light0.active = 1;
	glEnable( GL_LIGHT0 );
	light0.setGL();

	GLUquadricObj *quadric = gluNewQuadric();
	glPushMatrix();
		glColor3f( 0.5f, 0.5f, 0.5f );
		glTranslatef( -Plasma_radialOffset, +2.f, 0.f );
		glRotatef( 90.f, 1.f, 0.f, 0.f );
		gluCylinder( quadric, 0.1f, 0.1f, 4.f, 20, 1 );
		gluDeleteQuadric( quadric );
	glPopMatrix();

	glDisable( GL_LIGHTING );

	if( ! Plasma_drawHelicies ) {
		zglDrawStandardAxis();

		// DRAW the main pressure lines	
		for( int i=0; i<20; i++ ) {
			glPushMatrix();
				float rad = (float)i / 20.f;
				float p = powf( 1.f - powf(rad,Plasma_m), Plasma_n);
				glScalef( p, p, p );

				float r, g, b;
				zHSVF_to_RGBF( 0.f, 1.f-p, 1.f, r, g, b );
				glColor3f( r, g, b );
				drawXYPlaneCircle();
			glPopMatrix();
		}

		// DRAW cold plasma circle
		glPushMatrix();
			float rad = Plasma_bumpW;
			glTranslatef( Plasma_bumpOffset+Plasma_coldPlasmaX, Plasma_coldPlasmaY, 0.f );
			glScalef( rad, rad, rad );

			glColor3f( 0.f, 0.f, 1.f );
			drawXYPlaneDisc();
		glPopMatrix();

		// DRAW hot plasma circle
		glPushMatrix();
			rad = Plasma_bumpW;
			glTranslatef( Plasma_bumpOffset+Plasma_hotPlasmaX, Plasma_hotPlasmaY, 0.f );
			glScalef( rad, rad, rad );

			glColor3f( 1.f, 0.f, 0.f );
			drawXYPlaneDisc();
		glPopMatrix();

		// DRAW pressure projection
		glLineWidth( 3.f );
		glPushMatrix();
			glTranslatef( 0.f, 1.f, 0.f );
			glColor3ub( 144, 117, 84 );
			glBegin( GL_QUAD_STRIP );
			for( int i=0; i<100; i++ ) {
				float r = (float)i / 100;
				float p = 
					Plasma_bumpAmp
					+ powf( 1.f - powf(r,Plasma_m), Plasma_n) 
					- Plasma_bumpAmp * expf( -powf( (r - Plasma_bumpOffset-Plasma_coldPlasmaX) / Plasma_bumpW ,2.f) )
					+ Plasma_bumpAmp * expf( -powf( (r - Plasma_bumpOffset-Plasma_hotPlasmaX ) / Plasma_bumpW ,2.f) )
				;
				glVertex2f( r, 0.f );
				glVertex2f( r, p );
			}
			glEnd();
			glBegin( GL_QUAD_STRIP );
			for( int i=0; i<100; i++ ) {
				float r = (float)i / 100.f;
				float p = Plasma_bumpAmp + powf( 1.f - powf(r,Plasma_m), Plasma_n);
				glVertex2f( -r, 0.f );
				glVertex2f( -r, p );
			}
			glEnd();


			// PLOT the bumps separately
			glColor3ub( 0, 0, 255 );
			glBegin( GL_LINE_STRIP );
			for( int i=0; i<100; i++ ) {
				float r = (float)i / 100;
				float p = - Plasma_bumpAmp * expf( -powf( (r - Plasma_bumpOffset-Plasma_coldPlasmaX) / Plasma_bumpW ,2.f) );
				glVertex3f( r, p, 0.01f );
			}
			glEnd();

			glColor3ub( 255, 0, 0 );
			glBegin( GL_LINE_STRIP );
			for( int i=0; i<100; i++ ) {
				float r = (float)i / 100;
				float p = Plasma_bumpAmp * expf( -powf( (r - Plasma_bumpOffset-Plasma_hotPlasmaX) / Plasma_bumpW ,2.f) );
				glVertex3f( r, p, 0.01f );
			}
			glEnd();

		glPopMatrix();
		glLineWidth( 1.f );

/*		
		glPushMatrix();
			glScalef( Plasma_magnetScale, Plasma_magnetScale, Plasma_magnetScale );
			glRotatef( Plasma_magnetRotX, 1.f, 0.f, 0.f );
			glRotatef( Plasma_magnetRotY, 0.f, 1.f, 0.f );
			glRotatef( Plasma_magnetRotZ, 0.f, 0.f, 1.f );
			drawXYPlaneMagnet();
		glPopMatrix();
*/
	}
	else {
		// DRAW helicies
		glLineWidth( 3.f );
		
		Plasma_helixSpin += Plasma_helixSpinSpeed;

		for( int repeat=0; repeat<3; repeat++ ) {
			if( repeat==0 && !Plasma_drawHelix2 ) {
				continue;
			}
			if( repeat==1 && !Plasma_drawHelix3 ) {
				continue;
			}
			if( repeat==2 && !Plasma_drawHelix4 ) {
				continue;
			}
			int i = repeat;

			glPushMatrix();
				glTranslatef( -Plasma_radialOffset, 0.f, 0.f );
				glRotatef( 90.f, 1.f, 0.f, 0.f );

				glPushMatrix();
					switch( i ) {
						case 0: glColor3ub( 0, 0, 255 ); break;
						case 1: glColor3ub( 0, 255, 0 ); break;
						case 2: glColor3ub( 255, 0, 0 ); break;
					}
					glRotatef( Plasma_helixSpin, 0.f, 0.f, 1.f );
					glScalef( Plasma_helixScale, Plasma_helixScale, Plasma_helixScale );
					glBegin( GL_LINE_STRIP );
					for( int j=0; j<helix[i].count; j++ ) {
						glVertex3fv( helix[i][j] );
					}
					glEnd();
				glPopMatrix();

				glPushMatrix();				
					switch( i ) {
						case 0: glColor3ub( 0, 0, 160 ); break;
						case 1: glColor3ub( 0, 160, 0 ); break;
						case 2: glColor3ub( 160, 0, 0 ); break;
					}
					glBegin( GL_LINE_STRIP );
						for( int j=0; j<helix[i].count; j++ ) {
							glVertex3f( 0.f, sqrtf( powf(helix[i][j].x,2.f) + powf(helix[i][j].y,2.f) ), helix[i][j].z );
						}
					glEnd();
				glPopMatrix();
			glPopMatrix();
		}

		glLineWidth( 1.f );
	}

	// DRAW outline cube
	glPushMatrix();
		glTranslatef( -Plasma_radialOffset, 0.f, 0.f );
		glScalef( 2.f*Plasma_radialOffset, 2.f*Plasma_radialOffset, 2.f*Plasma_radialOffset );
		glColor3f( 0.5f, 0.5f, 0.5f );
		drawCube();
	glPopMatrix();

}

void startup() {
	char *dir = options.getS("pluginPath_plasma");
	for( int i=0; i<3; i++ ) {
		FILE *file = fopen( ZTmpStr( "%s/q_%d_EFTM_nch_205.dat", dir, i+2 ), "rt" );
		char line[1024];
		while( fgets( line, 1024, file ) ) {
			ZStr *zstr = zStrSplit( "\\s+", line );
			char *a = zstr->getS(1);
			char *b = zstr->getS(2);
			char *c = zstr->getS(3);
			helix[i].add( FVec3((float)atof(a),(float)atof(b),(float)atof(c)) );
		}
	}
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

