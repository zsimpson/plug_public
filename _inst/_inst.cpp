// @ZBS {
//		+DESCRIPTION {
//			Instrument breeder
//		}
//		*INTERFACE console
//		*SDK_DEPENDS dx8
// }

#include "stdio.h"
#include "memory.h"
#include "math.h"
#include "zsound.h"
#include "zconsole.h"
#include "zrand.h"
#include "ztime.h"
#include "assert.h"

#define max( a, b ) ( (a) > (b) ? (a) : (b) )
#define min( a, b ) ( (a) < (b) ? (a) : (b) )

float params[10][100];

void randomGeneration() {
	for( int i=0; i<10; i++ ) {
		for( int j=0; j<100; j++ ) {
			params[i][j] = zrandF( 0.f, 1.f );
		}
	}
}

void breed( int which ) {
	float master[100];
	memcpy( master, params[which], 100 * sizeof(float) );
	memcpy( params[0], params[which], 100 * sizeof(float) );
	for( int i=1; i<10; i++ ) {
		for( int j=0; j<100; j++ ) {
			params[i][j] = master[j] * zrandF(0.1f,5.f);
			params[i][j] = max( 0.f, params[i][j] );
			params[i][j] = min( 1.f, params[i][j] );
		}
	}
}


void main() {
	int i;

	zrandSeedFromTime();


	int ok = zsoundInit( (void*)zconsoleGetConsoleHwnd() );
	assert( ok );

	randomGeneration();

	int lastWhich;
	while( ! zconsoleKbhit() ) {
		zTimeSleepMils( 100 );
		int which = zconsoleGetch();
		if( which == 'q' ) {
			return;
		}
		if( which == 'r' ) {
			randomGeneration();
		}
		if( which == 'w' ) {
			params[0][0] = 1.f;
			params[0][1] = 0.f;
			params[0][2] = 0.f;
			params[0][3] = 0.f;
			params[0][4] = 1.f;
			params[0][5] = 0.f;
			params[0][6] = 0.f;
			params[0][7] = 0.f;
			params[0][8] = 0.f;
			params[0][9] = 0.f;
		}
		if( which == 'b' ) {
			breed( lastWhich );
		}

		which = which - '1';
		lastWhich = which;


		float *p = &params[ which ][0];

		printf( "\n" );
		for( i=0; i<20; i++ ) {
			printf( "%f\n", p[i] );
		}

		// GENERATE a waveform and play it
		short buffer[1024*16] = {0,};
		for( i=0; i<1024*16; i++ ) {
			float b = 0.f;
			b  = p[ 0] * 5000.f * sinf( (float)i * 0.1f * 1.f / 1.f ) * exp( -p[11] * 0.001f * i );
			b += p[ 1] * 5000.f * sinf( (float)i * 0.1f * 1.f / 2.f ) * exp( -p[12] * 0.001f * i );
			b += p[ 2] * 5000.f * sinf( (float)i * 0.1f * 1.f / 3.f ) * exp( -p[13] * 0.001f * i );
			b += p[ 3] * 5000.f * sinf( (float)i * 0.1f * 2.f / 3.f ) * exp( -p[14] * 0.001f * i );
			b += p[ 4] * 5000.f * sinf( (float)i * 0.1f * 1.f / 4.f ) * exp( -p[15] * 0.001f * i );
			b += p[ 5] * 5000.f * sinf( (float)i * 0.1f * 3.f / 4.f ) * exp( -p[16] * 0.001f * i );
			b += p[ 6] * 5000.f * sinf( (float)i * 0.1f * 1.f / 5.f ) * exp( -p[17] * 0.001f * i );
			b += p[ 7] * 5000.f * sinf( (float)i * 0.1f * 2.f / 5.f ) * exp( -p[18] * 0.001f * i );
			b += p[ 8] * 5000.f * sinf( (float)i * 0.1f * 3.f / 5.f ) * exp( -p[19] * 0.001f * i );
			b += p[ 9] * 5000.f * sinf( (float)i * 0.1f * 4.f / 5.f ) * exp( -p[20] * 0.001f * i );
			b += p[10] * 5000.f * zrandF(-1.f,1.f)                    * exp( -p[21] * 0.001f * i );
			buffer[i] = max( -30000, min( 30000, (short)b ) );
		}

		zsoundPlayBuffer( (char *)buffer, 1024*2*16, 44100, 2, 0 );
	}

}