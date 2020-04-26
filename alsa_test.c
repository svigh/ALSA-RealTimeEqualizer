#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

#define PLAYBACK_DEVICE "default"		// Let the system pick the wanted output
#define CAPTURE_DEVICE  "plughw:0,0"	// Set the line device you are recording from
#define LSB(x) 2*x						// Least significant byte in the sample
#define MSB(x) 2*x+1					// Most significant byte in the sample
#define debug_print printf				// So later it can be quickly removed

typedef struct{
	int periods_per_buffer;
	char *direction;
	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buffer_size;
	uint32_t buffer_time;
}audioParams;

// TODO: make these parameterized
static unsigned int rate = 44100;
static unsigned int format = SND_PCM_FORMAT_S16_LE;
static unsigned int in_channels  = 1;
static unsigned int out_channels = 1;

const int bytes_per_sample = 2;
const int frames = 1024;
snd_pcm_uframes_t period_size = frames * bytes_per_sample; //bytes
int periods_per_buffer = 3;

uint32_t buffer_time;
audioParams playbackParams, captureParams;

void print_params(audioParams params) {
	printf("Direction: %s\n\tPeriods: %d\n\tPeriod size: %zu\n\tBuffer size: %zu\n\tBuffer time: %zu\n",
		params.direction, params.periods_per_buffer, params.period_size, params.buffer_size, params.buffer_time);
}

void print_byte_as_bits(char val) {
	for (int i = 7; 0 <= i; i--) {
		printf("%c", (val & (1 << i)) ? '1' : '0');
	}
	printf("\n");
}

// Open the devices and configure them appropriately
static int set_parameters(snd_pcm_t **handle, const char *device, int direction, int channels) {
	int err;
	snd_pcm_hw_params_t *hw_params;
	const char *dirname = (direction == SND_PCM_STREAM_PLAYBACK) ? "PLAYBACK" : "CAPTURE";
	captureParams.direction  = "CAPTURE";
	playbackParams.direction = "PLAYBACK";

	if ((err = snd_pcm_open(handle, device, direction, 0)) < 0) {
		fprintf(stderr, "%s (%s): cannot open audio device (%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot allocate hardware parameter structure(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_any(*handle, hw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot initialize hardware parameter structure(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_set_access(*handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "%s (%s): cannot set access type(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_set_format(*handle, hw_params, format)) < 0) {
		fprintf(stderr, "%s (%s): cannot set sample format(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	/* Set number of periods_per_buffer. Periods used to be called fragments. */
	if ((err = snd_pcm_hw_params_set_periods_near(*handle, hw_params, &periods_per_buffer, NULL)) < 0) {
		fprintf(stderr, "%s (%s): cannot set periods number(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	/* Set number of periods_per_buffer. Periods used to be called fragments. */
	if ((err = snd_pcm_hw_params_set_period_size_near(*handle, hw_params, &period_size, NULL)) < 0) {
		fprintf(stderr, "%s (%s): cannot set period size(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}
	if (!strcmp(dirname, "PLAYBACK"))
		playbackParams.periods_per_buffer = periods_per_buffer;
	else
		captureParams.periods_per_buffer = periods_per_buffer;

	if ((err = snd_pcm_hw_params_set_channels(*handle, hw_params, channels)) < 0) {
		fprintf(stderr, "%s (%s): cannot set channel count(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	/* Set buffer size (in frames). The resulting latency is given by */
	/* latency = periodsize * periods_per_buffer / (rate * bytes_per_frame)	  */
	snd_pcm_uframes_t buffer_size = (period_size * periods_per_buffer)>>2;
	if ((err = snd_pcm_hw_params_set_buffer_size_near (*handle, hw_params, &buffer_size)) < 0){
		fprintf(stderr, "%s (%s): cannot set buffer size(%s)\n",
		device, dirname, snd_strerror(err));
	}
	if (!strcmp(dirname, "PLAYBACK"))
		playbackParams.buffer_size = buffer_size;
	else
		captureParams.buffer_size = buffer_size;

	if ((err = snd_pcm_hw_params_set_rate_near(*handle, hw_params, &rate, NULL)) < 0) {
		fprintf(stderr, "%s (%s): cannot set sample rate(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params(*handle, hw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot set parameters(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	snd_pcm_hw_params_get_period_size(hw_params, &period_size, &direction);
	if (!strcmp(dirname, "PLAYBACK"))
		playbackParams.period_size = period_size;
	else
		captureParams.period_size = period_size;

	snd_pcm_hw_params_get_buffer_time(hw_params, &buffer_time, &direction);	// This changes the direction, careful
	if (!strcmp(dirname, "PLAYBACK"))
		playbackParams.buffer_time = buffer_time;
	else
		captureParams.buffer_time = buffer_time;

	snd_pcm_hw_params_free(hw_params);

	return 0;
}

void add_echo(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size) {
	short *circular_buffer;
	int circular_buffer_index = 0;
	int echo_amount = 1024;			// How many frames/samples of echo

	circular_buffer = malloc(echo_amount * sizeof(short));
	memset(circular_buffer, 0, echo_amount * sizeof(short));

	for (int sample = 0; sample < buffer_size; sample++){
		output_buffer[sample] = (input_buffer[sample] / 2) + (circular_buffer[circular_buffer_index] / 2);
		circular_buffer[ circular_buffer_index ] = output_buffer[sample];

		circular_buffer_index = (circular_buffer_index + 1) % echo_amount;
	}
}

// TODO: there is a delay between audio in and output
int main(int argc, char **argv) {
	int err;
	snd_pcm_t *playback_handle, *capture_handle;

	if ((err = set_parameters(&playback_handle, PLAYBACK_DEVICE, SND_PCM_STREAM_PLAYBACK, in_channels)) < 0) {
		return err;
	}

	if ((err = set_parameters(&capture_handle, CAPTURE_DEVICE, SND_PCM_STREAM_CAPTURE, out_channels)) < 0) {
		return err;
	}

	// TODO: if there is a way to do this it could help with parameters being the same
	// if ((err = snd_pcm_link(capture_handle, playback_handle)) < 0) {
	// 	fprintf(stderr, "Cannot link handles(%s)\n", snd_strerror(err));
	// 	return err;
	// }

	if ((err = snd_pcm_prepare(playback_handle)) < 0) {
		fprintf(stderr, "Cannot prepare PLAYBACK interface(%s)\n", snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_start(capture_handle)) < 0) {
		fprintf(stderr, "Cannot start CAPTURE interface(%s)\n", snd_strerror(err));
		return err;
	}

	print_params(captureParams);
	print_params(playbackParams);

	uint32_t buffer_size_in  = captureParams.buffer_size ;
		printf("In buffer size: %d\n", buffer_size_in);
	uint32_t buffer_size_out = playbackParams.buffer_size;
		printf("Out buffer size: %d\n", buffer_size_out);

	// Input, processing and output buffers are the size of a reading buffer * sample size (short)
	short *read_buffer  = malloc(buffer_size_in  * sizeof(short));
	short *write_buffer = malloc(buffer_size_out * sizeof(short));
	short *proc_buffer  = malloc(buffer_size_out * sizeof(short));
	memset(read_buffer , 0, buffer_size_in  * sizeof(short));
	memset(write_buffer, 0, buffer_size_out * sizeof(short));
	memset(proc_buffer , 0, buffer_size_out * sizeof(short));

	snd_pcm_sframes_t inframes, outframes;
	while (1) {
		// Read line in
		while ((inframes = snd_pcm_readi(capture_handle, read_buffer, buffer_size_in)) < 0) {
			fprintf(stderr, "Input buffer overrun (%s)\n", strerror(inframes));
			snd_pcm_prepare(capture_handle);
		}

		// Process output
		add_echo(read_buffer, proc_buffer, inframes);
		memcpy(write_buffer, proc_buffer, buffer_size_out * sizeof(short));

		// Write to audio device
		while ((outframes = snd_pcm_writei(playback_handle, write_buffer, inframes)) < 0) {
			fprintf(stderr, "Output buffer underrun (%s)\n", strerror(outframes));
			snd_pcm_prepare(playback_handle);
		}
	}

	snd_pcm_drain(playback_handle);	// Maybe not necesarry to drain the buffer
	snd_pcm_close(playback_handle);
	snd_pcm_close(capture_handle);

	return 0;
}