#include "utils.h"
#include "effects.h"

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

unsigned int rate = 44100;
unsigned int format = SND_PCM_FORMAT_S16_LE;
unsigned int in_channels = CHANNELS;
unsigned int out_channels = CHANNELS;
int periods_per_buffer = 3;
snd_pcm_uframes_t period_size = FRAMES_PER_BUFFER * BYTES_PER_SAMPLE; //bytes
double EQ_bands_amplitude[10] = {0};
double gain = 0;

int addEcho = 0, addEQ = 0, addGain = 0, addDistort = 0;
uint32_t buffer_time;
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
		{	// TODO: are mutexes here necessary?
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
extern unsigned int rate;
extern unsigned int format;
extern unsigned int in_channels;
extern unsigned int out_channels;

extern int periods_per_buffer;
extern snd_pcm_uframes_t period_size;
	print_params(captureParams);
	print_params(playbackParams);


	// Start the input listener thread
	pthread_t tid;
	if ((err = pthread_create(&tid, NULL, inputListener, (void *)&tid)) < 0) {
		fprintf(stderr, "Cannot start input listener thread(%s)\n", snd_strerror(err));
		return err;
	}


	// Input, processing and output buffers are the size of a reading buffer * sample size (short)
	uint32_t buffer_size_in  = captureParams.buffer_size ;
		printf("In buffer size: %d\n", buffer_size_in);
	uint32_t buffer_size_out = playbackParams.buffer_size;
		printf("Out buffer size: %d\n", buffer_size_out);


	short *read_buffer  = malloc(buffer_size_in  * sizeof(short));
	short *write_buffer = malloc(buffer_size_out * sizeof(short));
	short *proc_buffer  = malloc(buffer_size_out * sizeof(short));
	memset(read_buffer , 0, buffer_size_in  * sizeof(short));
	memset(write_buffer, 0, buffer_size_out * sizeof(short));
	memset(proc_buffer , 0, buffer_size_out * sizeof(short));

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
	snd_pcm_sframes_t inframes, outframes;
	while (1) {
		// Read line in
		while ((inframes = snd_pcm_readi(capture_handle, read_buffer, buffer_size_in)) < 0) {
			fprintf(stderr, "Input buffer overrun (%s)\n", strerror(inframes));
			snd_pcm_prepare(capture_handle);
		}
		memcpy(proc_buffer, read_buffer, buffer_size_out * sizeof(short));

		if (addEQ)
			add_eq(proc_buffer, proc_buffer, inframes);

		if (addGain)
			add_gain(proc_buffer, proc_buffer, inframes, gain);

		if (addEcho)
			add_echo(proc_buffer, proc_buffer, inframes);

		if (addDistort)
			add_distort(proc_buffer, proc_buffer, inframes, 0.9, 0.5);

		memcpy(write_buffer, proc_buffer, buffer_size_out * sizeof(short));

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
