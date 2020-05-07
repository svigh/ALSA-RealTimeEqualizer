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


#define PLAYBACK_DEVICE "default"		// Let the system pick the wanted output
#define CAPTURE_DEVICE  "plughw:0,0"	// Set the line device you are recording from
#define MAX_VALUE 32767
#define MIN_VALUE -32767
#define CHANNELS  1						// TODO: distorted sound when using 2 channels
#define BYTES_PER_SAMPLE 2
#define FRAMES_PER_BUFFER 1024
#define PI 3.141592
#define NUM_EQ_BANDS 10
#define FFT_WINDOW_SIZE 512
#define ECHO_AMOUNT 1024				// How many frames/samples of echo

#define LSB(x) 2*x						// Least significant byte in the sample
#define MSB(x) 2*x+1					// Most significant byte in the sample
#define PLUS_FIVE(x) (x+5)
#define EQ_BAND_FREQUENCY(x) (1<<(PLUS_FIVE(x)))			// Bands frequencies in Hz
#define AMPLITUDE(x,y) sqrt((double)(x*x) + (y*y))
#define dB_TO_LINEAR(x) pow(2, x / 6)

#define debug_print printf				// So later it can be quickly removed
#define debug_fprint fprintf			// So later it can be quickly removed

typedef struct{
	int periods_per_buffer;
	char *direction;
	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buffer_size;
	uint32_t buffer_time;
}audioParams;

extern pthread_mutex_t mtx;

// TODO: make these parameterized
extern unsigned int rate;
extern unsigned int format;
extern unsigned int in_channels;
extern unsigned int out_channels;

extern int periods_per_buffer;
extern snd_pcm_uframes_t period_size;

extern uint32_t buffer_time;
extern audioParams playbackParams, captureParams;

extern double EQ_bands_amplitude[10];	// The amount in dB on how much each band needs to be modified
extern double gain;

void print_params(audioParams params);
void print_byte_as_bits(char val);
int set_parameters(snd_pcm_t **handle, const char *device, int direction, int channels);
void FFT(short int dir, long m, double *reals, double *imags);

#endif
