#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#define PLAYBACK_DEVICE "default"		// Let the system pick the wanted output
#define CAPTURE_DEVICE  "plughw:0,0"	// Set the line device you are recording from

#define FRAME_SIZE 64

// TODO: make these parameterized
static unsigned int rate = 44100;
static unsigned int format = SND_PCM_FORMAT_S32_LE;
static unsigned int in_channels  = 1;
static unsigned int out_channels = 1;
// snd_pcm_uframes_t buffer_size = 1024;
// snd_pcm_uframes_t period_size = 64;

// Open the devices and configure them appropriately
static int set_parameters(snd_pcm_t **handle, const char *device, int direction, int channels)
{
	int err;
	snd_pcm_hw_params_t *hw_params;
	const char *dirname = (direction == SND_PCM_STREAM_PLAYBACK) ? "PLAYBACK" : "CAPTURE";

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

	if ((err = snd_pcm_hw_params_set_channels(*handle, hw_params, channels)) < 0) {
		fprintf(stderr, "%s (%s): cannot set channel count(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}

	// if ((err = snd_pcm_hw_params_set_buffer_size_near (*handle, hw_params, &buffer_size)) < 0){
	// 	fprintf(stderr, "%s (%s): cannot set buffer size(%s)\n",
	// 	device, dirname, snd_strerror(err));
	// }

	// if ((err = snd_pcm_hw_params_set_period_size_near (*handle, hw_params, &period_size, NULL)) < 0){
	// 	fprintf(stderr, "%s (%s): cannot set period size(%s)\n",
	// 	device, dirname, snd_strerror(err));
	// }

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

	snd_pcm_hw_params_free(hw_params);

	return 0;
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

	if ((err = snd_pcm_prepare(playback_handle)) < 0) {
		fprintf(stderr, "Cannot prepare PLAYBACK interface(%s)\n", snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_start(capture_handle)) < 0) {
		fprintf(stderr, "Cannot start CAPTURE interface(%s)\n", snd_strerror(err));
		return err;
	}

	// TODO: calculate better buffer sizes
	// 										V->to prevent small audio pops
	uint32_t buffer_size_in  = FRAME_SIZE * 1 * in_channels  * snd_pcm_format_width(format) / 8;	printf("In buffer size: %d\n", buffer_size_in);
	uint32_t buffer_size_out = FRAME_SIZE * 1 * out_channels * snd_pcm_format_width(format) / 8;	printf("Out buffer size: %d\n", buffer_size_out);

	uint32_t *read_buffer  = malloc(buffer_size_in  * sizeof(uint32_t));
	uint32_t *write_buffer = malloc(buffer_size_out * sizeof(uint32_t));

	memset(read_buffer , 0, sizeof(read_buffer));
	memset(write_buffer, 0, sizeof(write_buffer));

	while (1) {
		snd_pcm_sframes_t inframes, outframes, avail_frames;
		memset(read_buffer , 0, sizeof(read_buffer));

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
		for (int i = 0; i < buffer_size_in; i++) {
			write_buffer[i] = read_buffer[i];
		}

		// Write to audio device
		while ((outframes = snd_pcm_writei(playback_handle, write_buffer, avail_frames)) < 0) {
			fprintf(stderr, "Output buffer underrun (%s)\n", strerror(outframes));
			snd_pcm_prepare(playback_handle);
		}

		if (outframes != avail_frames) {
			fprintf(stderr, "Short write. %d vs %d\n", outframes, avail_frames);
		}

		memset(write_buffer, 0, sizeof(write_buffer));
	}


	snd_pcm_drain(playback_handle);	// Maybe not necesarry to drain the buffer
	snd_pcm_close(playback_handle);
	snd_pcm_close(capture_handle);

	return 0;
}