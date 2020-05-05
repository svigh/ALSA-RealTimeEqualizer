#include "utils.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

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

void FFT(short int dir, long m, double *reals, double *imags) {
	long n,i,i1,j,k,i2,l,l1,l2;
	double c1,c2,tx,ty,t1,t2,u1,u2,z;

	/* Calculate the number of points */
	n = 1;
	for (i = 0; i < m; i++)
		n *= 2;

	/* Do the bit reversal */
	i2 = n >> 1;
	j = 0;
	for (i = 0; i < n-1; i++) {
		if (i < j) {
			tx = reals[i];
			ty = imags[i];
			reals[i] = reals[j];
			imags[i] = imags[j];
			reals[j] = tx;
			imags[j] = ty;
		}
		k = i2;
		while (k <= j) {
			j -= k;
			k >>= 1;
		}
		j += k;
	}

	/* Compute the FFT */
	c1 = -1.0;
	c2 = 0.0;
	l2 = 1;
	for (l = 0; l < m; l++) {
		l1 = l2;
		l2 <<= 1;
		u1 = 1.0;
		u2 = 0.0;
		for (j = 0; j < l1; j++) {
			for (i = j; i < n; i+=l2) {
				i1 = i + l1;
				t1 = u1 * reals[i1] - u2 * imags[i1];
				t2 = u1 * imags[i1] + u2 * reals[i1];
				reals[i1] = reals[i] - t1;
				imags[i1] = imags[i] - t2;
				reals[i] += t1;
				imags[i] += t2;
			}
			z  = u1 * c1 - u2 * c2;
			u2 = u1 * c2 + u2 * c1;
			u1 = z;
		}
		c2 = sqrt((1.0 - c1) / 2.0);
		if (dir == 1)
			c2 = -c2;
		c1 = sqrt((1.0 + c1) / 2.0);
	}

	/* Scaling for forward transform */
	if (dir == 1) {
		for (i = 0; i < n; i++) {
			reals[i] /= n;
			imags[i] /= n;
		}
	}
}
