// @ZBS {
//		+DESCRIPTION {
//			Experiments with port audio
//		}
//		*MODULE_DEPENDS _zaudio.cpp
//		*INTERFACE console
//		*SDK_DEPENDS portaudio libmad pthread
// }


// OPERATING SYSTEM specific includes:
#include "windows.h"
// SDK includes:
#include "portaudio.h"
#include "mad.h"
#include "pthread.h"
// STDLIB includes:
#include "stdio.h"
#include "assert.h"
// MODULE includes:
// ZBSLIB includes:
#include "zconsole.h"
#include "ztl.h"



struct Mp3Player {
	// Starts which begins the decoding and will monitor
	// waiting for the song to finish and at the end
	// it will clean up all the buffers and terminate the thread.
	// Or owner can ask that the thread be terminated.

	HWAVEOUT out;
	char *filename;
	pthread_t threadId;
	ZTLVec<void *> waveHeaders;

	struct MadContext {
		Mp3Player *_this;
		unsigned char const *start;
		unsigned long length;
	};
	static void *playThreadMain( void * );
	static signed int scale( mad_fixed_t sample );
	static enum mad_flow input( void *data, struct mad_stream *stream );
	static enum mad_flow output( void *data, struct mad_header const *header, struct mad_pcm *pcm );
	static enum mad_flow error( void *data, struct mad_stream *stream, struct mad_frame *frame );

	void play( HWAVEOUT _out, char *filename );
		// Loads and creates the thread

	void stop();
		// Requests that the thread is stopped
	
	Mp3Player();
};

Mp3Player::Mp3Player() {
	out = 0;
	filename = 0;
	memset( &threadId, 0, sizeof(threadId) );
}

enum mad_flow Mp3Player::error( void *data, struct mad_stream *stream, struct mad_frame *frame ) {
	// Ignore all errors
	return MAD_FLOW_CONTINUE;
}

enum mad_flow Mp3Player::input( void *context, struct mad_stream *stream ) {
	// * This is the input callback. The purpose of this callback is to (re)fill
	// * the stream buffer which is to be decoded. In this example, an entire file
	// * has been mapped into memory, so we just call mad_stream_buffer() with the
	// * address and length of the mapping. When this callback is called a second
	// * time, we are finished decoding.
	struct MadContext *madContext = (struct MadContext *)context;
	if( !madContext->length ) {
		return MAD_FLOW_STOP;
	}
	mad_stream_buffer( stream, madContext->start, madContext->length );
	madContext->length = 0;
	return MAD_FLOW_CONTINUE;
}

signed int Mp3Player::scale( mad_fixed_t sample ) {
	// * The following utility routine performs simple rounding, clipping, and
	// * scaling of MAD's high-resolution samples down to 16 bits. It does not
	// * perform any dithering or noise shaping, which would be recommended to
	// * obtain any exceptional audio quality. It is therefore not recommended to
	// * use this routine if high-quality output is desired.
	sample += (1L << (MAD_F_FRACBITS - 16));

	// CLIP
	if( sample >= MAD_F_ONE ) {
		sample = MAD_F_ONE - 1;
	}
	else if( sample < -MAD_F_ONE ) {
		sample = -MAD_F_ONE;
	}

	// QUANTIZE
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

enum mad_flow Mp3Player::output( void *context, struct mad_header const *header, struct mad_pcm *pcm ) {
	// * This is the output callback function. It is called after each frame of
	// * MPEG audio data has been completely decoded. The purpose of this callback
	// * is to output (or play) the decoded PCM audio.
	struct MadContext *madContext = (struct MadContext *)context;
	unsigned int nchannels, nsamples;
	mad_fixed_t const *left_ch, *right_ch;

	// pcm->samplerate contains the sampling frequency
	nchannels = pcm->channels;
	nsamples  = pcm->length;
	left_ch   = pcm->samples[0];
	right_ch  = pcm->samples[1];

	int size = nchannels * nsamples * 2;
	char *samples = (char *)malloc( size );

	char *dst = samples;

	while (nsamples--) {
		signed int sample;

		// output sample(s) in 16-bit signed little-endian PCM
		sample = scale(*left_ch++);
		*dst++ = ((sample >> 0) & 0xff);
		*dst++ = ((sample >> 8) & 0xff);
		if (nchannels == 2) {
			sample = scale(*right_ch++);
			*dst++ = ((sample >> 0) & 0xff);
			*dst++ = ((sample >> 8) & 0xff);
		}
	}

	// ADD to the windows waveOut queue, waveHeader must be on the heap
	WAVEHDR *waveHeader = new WAVEHDR;
	memset( waveHeader, 0, sizeof(WAVEHDR) );
	waveHeader->lpData = (char *)samples;
	waveHeader->dwBufferLength = size;
	void *waveHeaderVoidPtr = (void *)waveHeader;
	madContext->_this->waveHeaders.add( waveHeaderVoidPtr );

	waveOutPrepareHeader( madContext->_this->out, waveHeader, sizeof(WAVEHDR) );
	waveOutWrite( madContext->_this->out, waveHeader, sizeof(WAVEHDR) );

	return MAD_FLOW_CONTINUE;
}

void *Mp3Player::playThreadMain( void *arg ) {
	Mp3Player *_this = (Mp3Player *)arg;

	// LOAD the mp3 file
	FILE *f = fopen( _this->filename, "rb" );
	assert( f );
	fseek( f, 0, SEEK_END );
	int mp3Size = ftell( f );
	fseek( f, 0, SEEK_SET );
	char *mp3Buf = (char *)malloc( mp3Size );
	fread( mp3Buf, mp3Size, 1, f );
	fclose( f );
	free( _this->filename );
	_this->filename = 0;

	// SETUP decoder
	struct MadContext madContext;
	struct mad_decoder decoder;

	_this->waveHeaders.clear();
	_this->waveHeaders.grow = 10000;

	// INITIALIZE context
	madContext.start  = (const unsigned char *)mp3Buf;
	madContext.length = mp3Size;
	madContext._this = _this;

	// CONFIGURE libmad decoder callback functions
	mad_decoder_init(
		&decoder,
		&madContext,
		input,
		0, // header 
		0, // filter
		output,
		error,
		0 // message
	);

	// DECODE
	mad_decoder_run( &decoder, MAD_DECODER_MODE_SYNC );
	mad_decoder_finish(&decoder);

	// FREE the mp3 data now that it is done decoding
	free( mp3Buf );

	// WHILE buffer left playing
	while( _this->waveHeaders.count > 0 ) {
		// TRAVERSE the list of playing buffers and unprepare ones that are done
		Sleep( 100 );

		for( int i=0; i<_this->waveHeaders.count; i++ ) {
			WAVEHDR *h = (WAVEHDR *)_this->waveHeaders[i];
			if( h->dwFlags & WHDR_DONE ) {
				waveOutUnprepareHeader( _this->out, h, sizeof(WAVEHDR) );
				free( h );
				_this->waveHeaders.del( i );
				i--;
			}
		}
	}

	return 0;
}

void Mp3Player::play( HWAVEOUT _out, char *_filename ) {
	out = _out;
	filename = strdup( _filename );	

	// CREATE the thread
	pthread_create( &threadId, 0, &playThreadMain, (void *)this );
}

Mp3Player mp3Player;

#if 0
struct paTestData {
    float left_phase;
    float right_phase;
};

int patestCallback(
	const void *inputBuffer, 
	void *outputBuffer, 
	unsigned long framesPerBuffer, 
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData
) {

	/* Cast data passed through stream to our structure. */
	paTestData *data = (paTestData*)userData; 
	float *out = (float*)outputBuffer;
	unsigned int i;
	(void) inputBuffer; /* Prevent unused variable warning. */

	for( i=0; i<framesPerBuffer; i++ ) {
		*out++ = data->left_phase;  /* left */
		*out++ = data->right_phase;  /* right */

		/* Generate simple sawtooth phaser that ranges between -1.0 and 1.0. */
		data->left_phase += 0.01f;

		/* When signal reaches top, drop back down. */
		if( data->left_phase >= 1.0f ) data->left_phase -= 2.0f;

		/* higher pitch so we can distinguish left and right. */
		data->right_phase += 0.03f;
		if( data->right_phase >= 1.0f ) {
			data->right_phase -= 2.0f;
		}
	}
	return 0;

}
#endif

void main() {
	// OPEN the waveOut device
	int count = waveOutGetNumDevs();
	WAVEOUTCAPS caps[64];
	for( int i=0; i<count; i++ ) {
		waveOutGetDevCaps( i, &caps[i], sizeof(caps[0]) );
	}

	WAVEFORMATEX format;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.nSamplesPerSec = 44100;
	format.nAvgBytesPerSec = 44100 * 2 * 2;
	format.nBlockAlign = 4;
	format.wBitsPerSample = 16;
	format.cbSize = 0;

	HWAVEOUT out;
	int a = WAVE_MAPPER;
	int ret = waveOutOpen( &out, 6/*WAVE_MAPPER*/, &format, NULL, NULL, 0 );
	assert( ret == MMSYSERR_NOERROR );

	mp3Player.play( out, "c:\\music\\Alternative\\Beck\\Odelay\\01-Devil's Haircut.mp3" );
	while(1) {
		Sleep(10);
	}


#if 0

	#define SAMPLE_RATE (44100)
	static paTestData data;

	int err, i;

	err = Pa_Initialize();
	assert( err == paNoError );

	int numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 ) {
		printf( "ERROR: Pa_CountDevices returned 0x%x\n", numDevices );
	}
	
	const PaDeviceInfo *deviceInfo;
	for( i=0; i<numDevices; i++ ) {
		deviceInfo = Pa_GetDeviceInfo( i );
		printf( "%-2d %60s  in:%02d  out:%02d\n", i, deviceInfo->name, deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels );
	}

	PaStream *stream;

	double srate = SAMPLE_RATE;

    unsigned long framesPerBuffer = 256; //could be paFramesPerBufferUnspecified, in which case PortAudio will do its best to manage it for you, but, on some platforms, the framesPerBuffer will change in each call to the callback
    PaStreamParameters outputParameters;
/*
    PaStreamParameters inputParameters;

    memset( &inputParameters, 0, sizeof( inputParameters ) ); //not necessary if you are filling in all the fields
    inputParameters.channelCount = 0;
    inputParameters.device = inDevNum;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inDevNum)->defaultLowInputLatency ;
    inputParameters.hostApiSpecificStreamInfo = NULL; //See you specific host's API docs for info on using this field
*/

    memset( &outputParameters, 0, sizeof( outputParameters ) ); //not necessary if you are filling in all the fields
    outputParameters.channelCount = 2;
    outputParameters.device = 25;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency ;
    outputParameters.hostApiSpecificStreamInfo = NULL; //See you specific host's API docs for info on using this field


	err = Pa_IsFormatSupported(
		0/*&inputParameters*/,
		&outputParameters,
		SAMPLE_RATE
	);
	if( err == paFormatIsSupported ) {
	}


	err = Pa_OpenStream(
		&stream,
		0/*&inputParameters*/,
		&outputParameters,
		SAMPLE_RATE,
		256,
		0,
		patestCallback,
		&data
	);

/*

	// OPEN an audio I/O stream.
	err = Pa_OpenDefaultStream(
		&stream,
		0,
			// no input channels
		2,
			// stereo output
		paFloat32,
			// 32 bit floating point output
		SAMPLE_RATE,
		256,
			// frames per buffer, i.e. the number of sample frames that 
			// PortAudio willrequest from the callback. Many apps
			// may want to use paFramesPerBufferUnspecified, which
			// tells PortAudio to pick the best,
			//possibly changing, buffer size.
		patestCallback,
			// callback function
		&data
			// This is a pointer that will be passed to your callback
	); 
*/
	err = Pa_StartStream( stream );
	
	while( !zconsoleKbhit() ) {
		Sleep( 1 );
	}
	
	err = Pa_StopStream( stream );

	Pa_Terminate();
#endif
}