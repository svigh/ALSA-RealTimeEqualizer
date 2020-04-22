#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

#define PLAYBACK_DEVICE "default"		// Let the system pick the wanted output
#define CAPTURE_DEVICE  "plughw:0,0"	// Set the line device you are recording from

// TODO: make these parameterized
static unsigned int rate = 44100;
static unsigned int format = SND_PCM_FORMAT_S16_LE;
static unsigned int in_channels  = 1;
static unsigned int out_channels = 1;

int periods_per_buffer = 3;
snd_pcm_uframes_t period_size = 2048; //bytes
uint32_t buffer_time;

typedef struct{
	int periods_per_buffer;
	char *direction;
	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buffer_size;
	uint32_t buffer_time;
}audioParams;

audioParams playbackParams, captureParams;

void print_params(audioParams params) {
	printf("Direction: %s\n\tPeriods: %d\n\tPeriod size: %zu\n\tBuffer size: %zu\n\tBuffer time: %zu\n",
		params.direction, params.periods_per_buffer, params.period_size, params.buffer_size, params.buffer_time);
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

void add_echo(uint32_t *current_buffer, uint32_t *next_output, uint32_t buffer_size) {
	// uint32_t *circular_buffer;
	// int circular_buffer_size = 1024;

	// circular_buffer = malloc(circular_buffer_size * sizeof(uint32_t));
	// memset(circular_buffer, 0, circular_buffer_size * sizeof(uint32_t));

	// Circular buffer = previous output
	// for (int i = 0; i < circular_buffer_size; i++) {
	// 	circular_buffer[ 4*i ] = current_buffer[ 4*i ];
	// 	circular_buffer[4*i+1] = current_buffer[4*i+1];
	// 	circular_buffer[4*i+2] = current_buffer[4*i+2];
	// 	circular_buffer[4*i+3] = current_buffer[4*i+1];
	// }

	for (int i = 0; i < buffer_size / 4; i++){
		// for (int j = 0; j < circular_buffer_size/4; j++) {
		// 	circular_buffer[ 4*j ] = current_buffer[ 4*i ];
		// 	circular_buffer[4*j+1] = current_buffer[4*i+1];
		// 	circular_buffer[4*j+2] = current_buffer[4*i+2];
		// 	circular_buffer[4*j+3] = current_buffer[4*i+1];
		// }
		//														V->so here should be the circular buffer add
		next_output[ 4*i ] = (current_buffer[ 4*i ] * 0.5) + (current_buffer[ 4*i ] * 0.5);
		next_output[4*i+1] = (current_buffer[4*i+1] * 0.5) + (current_buffer[4*i+1] * 0.5);
		next_output[4*i+2] = (current_buffer[4*i+2] * 0.5) + (current_buffer[4*i+2] * 0.5);
		next_output[4*i+3] = (current_buffer[4*i+3] * 0.5) + (current_buffer[4*i+3] * 0.5);
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

	// TODO: calculate better buffer sizes
	// 												  V->bytes per sample
	uint32_t buffer_size_in  = captureParams.buffer_size ;//captureParams.period_size  * captureParams.periods_per_buffer  * in_channels ;
		printf("In buffer size: %d\n", buffer_size_in);
	uint32_t buffer_size_out = playbackParams.buffer_size;//playbackParams.period_size * playbackParams.periods_per_buffer * out_channels;
		printf("Out buffer size: %d\n", buffer_size_out);

	uint32_t *read_buffer  = malloc(buffer_size_in  * sizeof(uint32_t));
	uint32_t *write_buffer = malloc(buffer_size_out * sizeof(uint32_t));
	uint32_t *proc_buffer  = malloc(buffer_size_out * sizeof(uint32_t));

	memset(read_buffer , 0, buffer_size_in  * sizeof(uint32_t));
	memset(write_buffer, 0, buffer_size_out * sizeof(uint32_t));
	memset(proc_buffer , 0, buffer_size_out * sizeof(uint32_t));

	snd_pcm_sframes_t inframes, outframes, avail_frames;	// The numbers of read frames
	while (1) {
		// Read line in
		while ((inframes = snd_pcm_readi(capture_handle, read_buffer, buffer_size_in)) < 0) {
			fprintf(stderr, "Input buffer overrun (%s)\n", strerror(inframes));
			snd_pcm_prepare(capture_handle);
		}
		avail_frames = (inframes <= buffer_size_out) ? inframes : buffer_size_out;

		if (inframes != buffer_size_in) {
			fprintf(stderr, "Short read\n");
		}

		// Process output
		add_echo(read_buffer, proc_buffer, avail_frames);
		memcpy(write_buffer, proc_buffer, buffer_size_out * sizeof(uint32_t));
		// *write_buffer = *proc_buffer;

		// Write to audio device
		while ((outframes = snd_pcm_writei(playback_handle, write_buffer, buffer_size_out)) < 0) {
			fprintf(stderr, "Output buffer underrun (%s)\n", strerror(outframes));
			snd_pcm_prepare(playback_handle);
		}

		if (outframes != avail_frames) {
			fprintf(stderr, "Short write. %d vs %d\n", outframes, avail_frames);
		}
	}


	snd_pcm_drain(playback_handle);	// Maybe not necesarry to drain the buffer
	snd_pcm_close(playback_handle);
	snd_pcm_close(capture_handle);

	return 0;
}