#include "utils.h"
#include "effects.h"

double gain = 0;

int addEcho = 0, addEQ = 0, addGain = 0, addDistort = 0;
audioParams playbackParams, captureParams;

// Get the user/external input - always wait for it
// TODO: Things get printed twice
void *inputListener(void *vargp) {
	int option;
	while(1) {
		printf("Make selection\n");
		printf("1. Remove effects\n");
		printf("2. Add Echo\n");
		printf("3. Add Gain\n");
		printf("4. Add Distort\n");
		printf("5. Add EQ\n");
		option = getchar();

		// Clear the newline buffer - ONLY FOR MANUAL INPUT WITHOUT GUI
		// getchar();

		switch (option)
		{
			case 49:	// 1 - Remove every effect
				addEcho = 0;
				addEQ = 0;
				addGain = 0; gain = 0;
				addDistort = 0;
				break;
			case 50:	// 2 - Add echo
				if (addEcho)
					addEcho = 0;
				else
					addEcho = 1;
				break;
			case 51:	// 3 - Add Amplification
				// Set a fixed amount of maximum gain
				if (addGain && gain >= 8) {
					addGain = 0;
					gain = 0;
				}
				else {
					gain += 2;
					addGain = 1;
				}
				break;
			case 52:	// 4 - Add Distort
				if (addDistort)
					addDistort = 0;
				else
					addDistort = 1;
				break;
			case 53:	// 5 - Add EQ
				if (addEQ)
					addEQ = 0;
				else
					addEQ = 1;
				break;
			default:
				break;
		}
	}
}

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

	print_params(captureParams);
	print_params(playbackParams);

	// Start the input listener thread
	pthread_t tid;
	if ((err = pthread_create(&tid, NULL, inputListener, (void *)&tid)) < 0) {
		fprintf(stderr, "Cannot start input listener thread(%s)\n", snd_strerror(err));
		return err;
	}


	// Input, processing and output buffers are the size of a reading buffer * sample size (sample_size = bytes/sample * CHANNELS)
	uint32_t frames_in  = captureParams.buffer_size;
		printf("In buffer size: %d\n", frames_in * CHANNELS);
	uint32_t frames_out = playbackParams.buffer_size;
		printf("Out buffer size: %d\n", frames_out * CHANNELS);


	short *read_buffer  = malloc(frames_in  * sizeof(short) * CHANNELS);
	short *write_buffer = malloc(frames_out * sizeof(short) * CHANNELS);
	short *proc_buffer  = malloc(frames_out * sizeof(short) * CHANNELS);
	memset(read_buffer , 0, frames_in  * sizeof(short) * CHANNELS);
	memset(write_buffer, 0, frames_out * sizeof(short) * CHANNELS);
	memset(proc_buffer , 0, frames_out * sizeof(short) * CHANNELS);

		// Process output
		//				 ->	read_buffer	  ->proc_buffer		 ->	proc_buffer	  ->proc_buffer		^
		//				 |		V		  |		V			 |		V		  |		V			|
		// |-----------| |	|-----------| |	|-----------|	 |	|-----------| |	|-----------|	|
		// |alsa_readi |->	|	EQ		| ->|	echo	|->etc->|	distort	| ->|alsa_writei|	|	DATA STEPS
		// |-----------| |	|-----------| |	|-----------|	 |	|-----------| |	|-----------|	|
		// 			V	 |			V	  | 		V	 	 |			V	  |					|
		// 	read_buffer	 |	proc_buffer  >|		proc_buffer	>|	  proc_buffer>|					|
		//			V	 |																		|
		//			>>>>>|																		V
		//--------------------------------------TIME STEPS----------------------------------->


	// Start the audio read-process-write
	struct timeval tval_before, tval_after, tval_result;
	double current_time_spent_ms, average_time_ms;
	double recorded_times[BUFFER_COUNT] = {0.0}; int recording_index = 0;

	snd_pcm_sframes_t inframes, outframes;
	while (1) {
		// Read line in	| because we specify the number of frames, not memory size to read ---v
		while ((inframes = snd_pcm_readi(capture_handle, read_buffer, frames_in)) < 0) {
			fprintf(stderr, "Input buffer overrun (%s)\n", strerror(inframes));
			snd_pcm_prepare(capture_handle);
		}

#ifdef TESTING
	gettimeofday(&tval_before, NULL);
#endif

		memcpy(proc_buffer, read_buffer, frames_out * sizeof(short) * CHANNELS);

		if (addEQ)
			add_eq(read_buffer, proc_buffer, inframes * CHANNELS);

		if (addGain)
			add_gain(proc_buffer, proc_buffer, inframes * CHANNELS, gain);

		if (addEcho)
			add_echo(proc_buffer, proc_buffer, inframes * CHANNELS);

		if (addDistort)
			add_distort(proc_buffer, proc_buffer, inframes * CHANNELS, 1.5, 1.5);

		memcpy(write_buffer, proc_buffer, frames_out * sizeof(short) * CHANNELS);

#ifdef TESTING
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_result);

	current_time_spent_ms = (double)tval_result.tv_usec / uS_IN_MS;

	recorded_times[recording_index++] = current_time_spent_ms;

	for (int i = 0; i < BUFFER_COUNT; i++) {
		average_time_ms += recorded_times[i];
	}

	average_time_ms /= BUFFER_COUNT;
	printf("Average time spent last %d buffers: %lf ms/%lf ms\n",
				BUFFER_COUNT, average_time_ms, (double)captureParams.buffer_time_ms / captureParams.periods_per_buffer);

	recording_index %= BUFFER_COUNT;
#endif

		// Write to audio device
		while ((outframes = snd_pcm_writei(playback_handle, write_buffer, inframes)) < 0) {
			fprintf(stderr, "Output buffer underrun (%s)\n", strerror(outframes));
			snd_pcm_prepare(playback_handle);
		}
	}


	if ((err = pthread_join(tid, NULL)) < 0) {
		fprintf(stderr, "Cannot join input listener thread(%s)\n", snd_strerror(err));
		return err;
	}


	snd_pcm_drain(playback_handle);	// Maybe not necesarry to drain the buffer
	snd_pcm_close(playback_handle);
	snd_pcm_close(capture_handle);

	free(read_buffer);
	free(proc_buffer);
	free(write_buffer);
	return 0;
}
