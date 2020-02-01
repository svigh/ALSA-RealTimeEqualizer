/*
    Simple sound playback using ALSA API and libasound.

    Dependencies: libasound, alsa
    Build-Dependencies: liasound-dev
    Compile: gcc -lasound -o play sound_playback.c
    Usage: ./play <sample_rate> <channels> <seconds> < <file>
    Examples:
        ./play 44100 2 5 < /dev/urandom
        ./play 22050 1 8 < /path/to/file.wav

    Copyright (C) 2009 Alessandro Ghedini <al3xbio@gmail.com>
    --------------------------------------------------------------
    "THE BEER-WARE LICENSE" (Revision 42):
    Alessandro Ghedini wrote this file. As long as you retain this
    notice you can do whatever you want with this stuff. If we
    meet some day, and you think this stuff is worth it, you can
    buy me a beer in return.
    --------------------------------------------------------------
*/

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <math.h>

#define PCM_DEVICE "default"

typedef enum{
	PLAYBACK,
	CAPTURE
}DIRECTION;

// TODO: make a set_params func

int main(int argc, char **argv) {
	unsigned int tmp, dir = PLAYBACK;
	snd_pcm_t *pcm;
	int rate, channels, seconds;
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
	// char *buff;
	uint32_t *buff;
	int buff_size, loops;
	int input_fd;
	int output_fd;
	if (argc < 6) {
		printf("Usage: %s <sample_rate> <channels> <seconds> <file_path> <ouptut_file>\n",
								argv[0]);
		return -1;
	}

	rate 	 = atoi(argv[1]);
	channels = atoi(argv[2]);
	seconds  = atoi(argv[3]);

	// What flag should I pass here?
	input_fd = open(argv[4], O_SYNC | O_RDONLY);

	if ( input_fd == -1 ){
		printf("ERROR: Can't open %s input file.\n", argv[4]);
		exit(-1);
	}

	// output_fd = open("outBin.txt", O_WRONLY | O_CREAT | O_TRUNC);
	// if ( output_fd == -1 ){
	// 	printf("ERROR: Can't open %s output file.\n", "outBin.txt");
	// 	exit(-1);
	// }

	FILE *out = fopen(argv[5], "w");
	if ( !out ){
		printf("ERROR: Can't open %s output file.\n", argv[5]);
		exit(-1);
	}

	/* Open the PCM device in playback mode */
	if (pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE,
					SND_PCM_STREAM_PLAYBACK, 0) < 0)
		printf("ERROR: Can't open \"%s\" PCM device. %s\n",
					PCM_DEVICE, snd_strerror(pcm));

	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle, params);

	/* Set parameters */
	if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params,
					SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
		printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params,
						SND_PCM_FORMAT_S16_LE) < 0)
		printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0)
		printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, &dir) < 0)
		printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

	/* Write parameters */
	if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
		printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));

	/* Resume information */
	printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));

	printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

	snd_pcm_hw_params_get_channels(params, &tmp);
	printf("channels: %i ", tmp);

	if (tmp == 1)
		printf("(mono)\n");
	else if (tmp == 2)
		printf("(stereo)\n");

	snd_pcm_hw_params_get_rate(params, &tmp, &dir);
	printf("sample rate: %d bps\n", tmp);

	printf("seconds: %d\n", seconds);

	/* Allocate buffer to hold single period */
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);

	buff_size = frames * channels * 2 /* 2 -> sample size */;
	buff = (char *) malloc(buff_size * sizeof(uint32_t));
	printf("buffsize: %d\n", buff_size);
	int printed = 0;
	snd_pcm_hw_params_get_period_time(params, &tmp, &dir);
	printf("pcm val: %d\n", input_fd);
	printf("loops: %d\n", (seconds * 1000000) / tmp);

	for (loops = (seconds * 1000000) / tmp; loops > 0; loops--) {
		// This can also be from the < pipe from console (fd 0)
		if (pcm = read(input_fd, buff, buff_size) == 0) {
			printf("Early end of file.\n");
			return 0;
		}

#if 0
		for(int i = 0; i < buff_size; ++i) {
			if(buff[i] == 0x0){
				buff[i] = buff[i-1];
				printf("%d\n", i);
			}
		}
#endif
		// write(output_fd, buff, buff_size);
		fprintf(out, "loop %d\n", loops);
		for(int i = 0; i < buff_size; ++i) {
			fprintf(out, "buf[%d] %x\n", i, buff[i]);
		}

		if (pcm = snd_pcm_writei(pcm_handle, buff, frames) == -EPIPE) {
			printf("XRUN.\n");
			snd_pcm_prepare(pcm_handle);
		} else if (pcm < 0) {
			printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
		}
	}

	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
	free(buff);

	return 0;
}