// @ZBS {
//		*SDK_DEPENDS dsound portaudio
// }

// OPERATING SYSTEM specific includes:
#include "windows.h"

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glfw.h"
// SDK includes:
#include "portaudio.h"
// STDLIB includes:
#include "assert.h"
#include "math.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zsound.h"
#include "zrand.h"
#include "zchangemon.h"

ZPLUGIN_BEGIN( tone );

ZVAR( float, Tone_t0, 1.13e+001 );
ZVAR( float, Tone_t1, 9.38e-001 );
ZVAR( float, Tone_t2, 8.51e+000 );
ZVAR( int, Tone_tone, 32 );
ZVAR( float, Tone_pan, 1 );


int sound = 0;
char buffer[0x8000];

char noise[1024];
ZChangeMon changeMon;

void render() {
	int i;
	if( changeMon.hasChanged() ) {
		zsoundChangePan( sound, Tone_pan );

		float noiseLen = (float)( sizeof(noise)/sizeof(noise[0]) );
		for( i=0; i<noiseLen; i++ ) {
			float t = (float)i / noiseLen;
			noise[i] = (char)(int)( 125.f * (sinf( Tone_t0 * t ) + sinf( Tone_t1 * t ) + sinf( Tone_t2 * t )) );
		}

		char *p = zsoundLock( sound );

		for( i=0; i<0x8000; i+=1024 ) {
			memcpy( &p[i], noise, 1024 );
		}


/*
		memset( p, 0, 0x8000 );
		for( i=0; i<Tone_tone; i++ ) {
			int pos = i * 0x8000 / Tone_tone;
			int len = min( 1024, 0x8000 - pos );
			memcpy( &p[ pos ], noise, len );
		}
*/
		zsoundUnlock( sound );

	}
}

void startup() {
	int err = Pa_Initialize();
	assert( err == paNoError );

	int numDevices = Pa_GetDeviceCount();
	const PaDeviceInfo *deviceInfo;
    for( int i=0; i<numDevices; i++ )
    {
        deviceInfo = Pa_GetDeviceInfo( i );
        if( deviceInfo->maxOutputChannels == 8 ) {
			const PaStreamParameters *inputParameters = 0;
			const PaStreamParameters *outputParameters = 0;

			// I think that this will work

			err = Pa_IsFormatSupported( inputParameters, outputParameters, 44100 );
			if( err == paFormatIsSupported ) {
			}
        }
    }
/*


	HWND frontWindowHWND = (HWND)glfwZBSExt_GetHWND();
	zsoundInit( frontWindowHWND );

	sound = zsoundPlayBuffer( buffer, 0x8000, 44000, 1, 1 );

	changeMon.add( &Tone_t0, sizeof(Tone_t0) );
	changeMon.add( &Tone_t1, sizeof(Tone_t1) );
	changeMon.add( &Tone_t2, sizeof(Tone_t2) );
	changeMon.add( &Tone_tone, sizeof(Tone_tone) );
	changeMon.add( &Tone_pan, sizeof(Tone_pan) );
*/
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

