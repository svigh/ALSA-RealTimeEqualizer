#include "utils.h"

unsigned int rate = RATE;
unsigned int format = SND_PCM_FORMAT_S16_LE;
unsigned int in_channels = CHANNELS;
unsigned int out_channels = CHANNELS;
int periods_per_buffer = PERIODS_PER_BUFFER;
snd_pcm_uframes_t period_size = FRAMES_PER_BUFFER * BYTES_PER_SAMPLE; //bytes

void print_params(audioParams params) {
	printf("Direction: %s\n\tPeriods: %d\n\tPeriod size: %zu\n\tChannels: %d\n\tBuffer size: %zu\n\tBuffer time ms: %lf\n",
		params.direction, params.periods_per_buffer, params.period_size, params.channels, params.buffer_size, params.buffer_time_ms);
}

void print_byte_as_bits(char val) {
	for (int i = 7; 0 <= i; i--) {
		printf("%c", (val & (1 << i)) ? '1' : '0');
	}
	printf("\n");
}

// Open the devices and configure them appropriately
int set_parameters(snd_pcm_t **handle, const char *device, int direction, int channels) {
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


	if ((err = snd_pcm_hw_params_set_rate_near(*handle, hw_params, &rate, NULL)) < 0) {
		fprintf(stderr, "%s (%s): cannot set sample rate(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}


	if ((err = snd_pcm_hw_params_set_periods_near(*handle, hw_params, &periods_per_buffer, NULL)) < 0) {
		fprintf(stderr, "%s (%s): cannot set periods number(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}


	if (!strcmp(dirname, "PLAYBACK"))
		playbackParams.periods_per_buffer = periods_per_buffer;
	else
		captureParams.periods_per_buffer = periods_per_buffer;


	if ((err = snd_pcm_hw_params_set_period_size_near(*handle, hw_params, &period_size, NULL)) < 0) {
		fprintf(stderr, "%s (%s): cannot set period size(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}


	if (!strcmp(dirname, "PLAYBACK"))
		playbackParams.period_size = period_size;
	else
		captureParams.period_size = period_size;


	if ((err = snd_pcm_hw_params_set_channels(*handle, hw_params, channels)) < 0) {
		fprintf(stderr, "%s (%s): cannot set channel count(%s)\n",
			device, dirname, snd_strerror(err));
		return err;
	}


	if (!strcmp(dirname, "PLAYBACK"))
		snd_pcm_hw_params_get_channels(hw_params, &playbackParams.channels);
	else
		snd_pcm_hw_params_get_channels(hw_params, &captureParams.channels);


	if (!strcmp(dirname, "PLAYBACK"))
		snd_pcm_hw_params_get_buffer_size(hw_params, &playbackParams.buffer_size);
	else
		snd_pcm_hw_params_get_buffer_size(hw_params, &captureParams.buffer_size);


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


	uint32_t buffer_time_ms;
	/* latency = periodsize * periods_per_buffer / (rate * bytes_per_frame)	  */
	snd_pcm_hw_params_get_buffer_time(hw_params, &buffer_time_ms, &direction);	// Get buffer time in us. This changes the direction, careful
	if (!strcmp(dirname, "PLAYBACK"))
		playbackParams.buffer_time_ms = (double)buffer_time_ms / (double)uS_IN_MS;
	else
		captureParams.buffer_time_ms = (double)buffer_time_ms / (double)uS_IN_MS;


	snd_pcm_hw_params_free(hw_params);

	return 0;
}
