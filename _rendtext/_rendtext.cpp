// @ZBS {
//		*MODULE_DEPENDS 
//		*SDK_DEPENDS glew
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/glew.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "assert.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zgltools.h"
#include "zrand.h"
#include "zglfont.h"
#include "ztime.h"
#include "ztmpstr.h"

ZPLUGIN_BEGIN( rendtext );

bool checkFramebufferStatus() {
    // check FBO status
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    switch( status ) {
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			return true;

		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			assert( 0 && "[ERROR] Framebuffer incomplete: Attachment is NOT complete.\n" );
			return false;

		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			assert( 0 && "[ERROR] Framebuffer incomplete: No image is attached to FBO.\n" );
			return false;

		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			assert( 0 && "[ERROR] Framebuffer incomplete: Attached images have different dimensions.\n" );
			return false;

		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			assert( 0 && "[ERROR] Framebuffer incomplete: Color attached images have different internal formats.\n" );
			return false;

		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			assert( 0 && "[ERROR] Framebuffer incomplete: Draw buffer.\n" );
			return false;

		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			assert( 0 && "[ERROR] Framebuffer incomplete: Read buffer.\n" );
			return false;

		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			assert( 0 && "[ERROR] Unsupported by FBO implementation.\n" );
			return false;

		default:
			assert( 0 && "[ERROR] Unknow error.\n" );
			return false;
    }
}



GLuint fboId = 0;
GLuint textureId = 0;
int frameCount = 0;

#define TEXTURE_DIM 512

void render() {

	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fboId );
	glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, textureId, 0 );

	// check FBO status
    bool status = checkFramebufferStatus();
    assert( status );

	glClearColor( 1.f, 1.f, 1.f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT );
	zglPixelMatrixFirstQuadrant();
	glColor3ub( (++frameCount)%255, 0, 0 );
	glBegin( GL_LINES );
		for( int i=0; i<20; i++ ) {
			glVertex2i( TEXTURE_DIM*i/20, 0 );
			glVertex2i( TEXTURE_DIM*i/20, TEXTURE_DIM );
		}

		for( int i=0; i<20; i++ ) {
			glVertex2i( 0, TEXTURE_DIM*i/20 );
			glVertex2i( TEXTURE_DIM, TEXTURE_DIM*i/20 );
		}
	glEnd();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);


	// DRAW the texture
	zglMatrixFirstQuadrant();
    glEnable(GL_TEXTURE_2D);
	glBindTexture( GL_TEXTURE_2D, textureId );
	glColor3f( 1.f, 1.f, 1.f );
	zglTexRect( 0.f, 0.f, 1.f, 1.f );

	zglPixelMatrixFirstQuadrant();
	glScalef( 3.f, 3.f, 1.f );
	glColor3ub( 0, 0, 0 );
	static int count=0;
	static double accum=0.0;
	static double fps=0.0;
	accum += zTimeFPS;
	if( ! (count++ % 100) ) {
		fps = accum / 100.0;
		accum = 0.0;
	}
	zglFontPrint( ZTmpStr("FPS = %2.1lf",fps), 10.f, 10.f, "controls" );

}

void startup() {
	GLenum err = glewInit();
	assert( err == GLEW_OK );

    // CREATE a texture object
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_DIM, TEXTURE_DIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffersEXT(1, &fboId);
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

