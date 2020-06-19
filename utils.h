#ifndef UTILS_H
#define UTILS_H

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <fftw3.h>
#include <stdint.h>
#include <time.h>						// Include if necessary

//////////////////////////
// ***AUDIO SETTINGS*** //
//////////////////////////
#define PLAYBACK_DEVICE "default"		// Let the system pick the wanted output
#define CAPTURE_DEVICE  "plughw:0,0"	// Set the line device you are recording from
#define RATE 44100
#define CHANNELS 2
#define PERIODS_PER_BUFFER 3
#define BYTES_PER_SAMPLE 2
#define FRAMES_PER_BUFFER 256

/////////////////
// ***UTILS*** //
/////////////////
#define SHORT_MAX 32767
#define SHORT_MIN -32767
#define PI 3.141592
#define BUFFER_COUNT 100
#define uS_IN_MS 100000.0

////////////////////////////
// ***EFFECTS SETTINGS*** //
////////////////////////////
#define NUM_EQ_BANDS 10
#define FFT_WINDOW_SIZE (NUM_EQ_BANDS * 2)	// To get 10 relevant frequency data { 0R; [1; N/2]-freq; (N/2; N) - mirrored freq }
#define AMPLITUDE(x,y) sqrt((double)(x*x) + (y*y))
#define dB_TO_LINEAR(x) pow(2, x / 3)
#define ECHO_AMOUNT 4096				// How many frames/samples of echo

// ***REMOVE LATER***
#define debug_print printf				// So later it can be quickly removed
#define debug_fprint fprintf			// So later it can be quickly removed

typedef struct {
	int periods_per_buffer;
	char *direction;
	int channels;
	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buffer_size;
	double buffer_time_ms;				// PRECULATED BY ALSA, IN ms => buffer_time_ms = buffer_size / rate
}audioParams;


// TODO: make these parameterized
extern unsigned int rate;
extern unsigned int format;
extern unsigned int in_channels;
extern unsigned int out_channels;

extern int periods_per_buffer;
extern snd_pcm_uframes_t period_size;

extern uint32_t buffer_time_ms;
extern audioParams playbackParams, captureParams;

extern double EQ_bands_amplitude[10];	// The amount in dB on how much each band needs to be modified
extern double gain;

void print_params(audioParams params);
void print_byte_as_bits(char val);
int set_parameters(snd_pcm_t **handle, const char *device, int direction, int channels);

#endif
