#include "effects.h"
#include "utils.h"

void add_echo(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size) {
	short *circular_buffer;
	int circular_buffer_index = 0;
	int echo_amount = 1024;			// How many frames/samples of echo

	circular_buffer = malloc(echo_amount * sizeof(short));
	memset(circular_buffer, 0, echo_amount * sizeof(short));

	for (int ch = 0; ch < CHANNELS; ch++) {
		for (int sample = ch; sample < buffer_size / CHANNELS; sample+=CHANNELS) {
			output_buffer[sample] = (input_buffer[sample] / 2) + (circular_buffer[circular_buffer_index] / 2);
			circular_buffer[ circular_buffer_index ] = output_buffer[sample];

			circular_buffer_index = (circular_buffer_index + 1) % echo_amount;
		}
	}
	free(circular_buffer);
}

void add_gain(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size, double gain) {
	for (int ch = 0; ch < CHANNELS; ch++) {
		for (int sample = ch; sample < buffer_size / CHANNELS; sample+=CHANNELS) {
			if ((input_buffer[sample] * gain) > MAX_VALUE) {
				output_buffer[sample] = MAX_VALUE;
			} else {
				if ((input_buffer[sample] * gain) < MIN_VALUE) {
					output_buffer[sample] = MIN_VALUE;
				} else {
					output_buffer[sample] = input_buffer[sample] * gain;
				}
			}
		}
	}
}

void add_distort(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size, double min_multiplier, double max_multiplier) {
	short max_threshold = MAX_VALUE * min_multiplier / 100;
	short min_threshold = MIN_VALUE * max_multiplier / 100;
	double volume_fix_amount = ((1 + min_multiplier) + (1 + max_multiplier)) / 2;	// TODO: is average or sum better?

	for (int ch = 0; ch < CHANNELS; ch++) {
		for (int sample = ch; sample < buffer_size / CHANNELS; sample+=CHANNELS) {
			if (input_buffer[sample] < min_threshold) {
				output_buffer[sample] = min_threshold;
			} else {
				if (input_buffer[sample] > max_threshold) {
					output_buffer[sample] = max_threshold;
				} else {
					output_buffer[sample] = input_buffer[sample];
				}
			}
		}
	}

	// Volume was being reduced a lot, we need to fix it
	add_gain(input_buffer, output_buffer, buffer_size, volume_fix_amount);
}

// WIP
void add_eq(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size) {
	fftw_complex *in, *out;
	fftw_plan p;
	in  = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * buffer_size);
	out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * buffer_size);

	// TODO: Not sure how good this is, should open it once - but then the values are not being updated realtime
	FILE *f = fopen("eq_vals.txt", "r");
	FILE *fout = fopen("FFT_out", "w");

	if (!f || !fout) {
		fprintf(stderr, "Could not open EQ files.\n");
		return;
	}

	// From the GUI input - get the bands amplitudes to use to modify each band
	for (int i = 0; i < 10; i++) {
		fscanf(f, "%lf", &EQ_bands_amplitude[i]);
	}
	fclose(f);
	// for (int i = 0; i < 10; i++) {
	// 	debug_print("%lf ", EQ_bands_amplitude[i]);
	// }
	// debug_print("\n");

	//////////////////////////////////////////////////
	// IF THE FFT WINDOW SIZE NEEDS TO BE POWER OF 2//
	//////////////////////////////////////////////////
	// for (int per = 0; per < buffer_size / FFT_WINDOW_SIZE; per++) {
	// 	for(int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
	// 		in[sample][0] = input_buffer[sample + per*FFT_WINDOW_SIZE];
	// 		in[sample][1] = 0;
	// 	}

	// 	for (int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
	// 		debug_fprint(fout, "(%lf, %lf, %lf)\n", in[sample][0], in[sample][1], AMPLITUDE(in[sample][0],in[sample][1]));
	// 	}

	// 	// FFT(1, 10, reals, imags);
	// 	p = fftw_plan_dft_1d(FFT_WINDOW_SIZE, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
	// 	fftw_execute(p); /* repeat as needed */

	// 	debug_fprint(fout, "\n\n\n\n\n");

	// 	for (int sample = 0; sample < FFT_WINDOW_SIZE / 2; sample++) {
	// 		debug_fprint(fout, "%lf\n", AMPLITUDE(out[sample][0],out[sample][1]));
	// 	}

	// 	debug_fprint(fout, "\n\n\n\n\n");

	// 	// Rebuild the audio samples
	// 	// FFT(0, 11, reals, imags);
	// 	p = fftw_plan_dft_1d(FFT_WINDOW_SIZE, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
	// 	fftw_execute(p); /* repeat as needed */

	// 	for (int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
	// 		debug_fprint(fout, "(%lf, %lf, %lf)\n", in[sample][0], in[sample][1], AMPLITUDE(in[sample][0],in[sample][1]));
	// 		output_buffer[sample + per*FFT_WINDOW_SIZE] = in[sample][0];
	// 	}

	// 	debug_fprint(fout, "\n\n");
	// }

	for(int sample = 0; sample < buffer_size; sample++) {
		in[sample][0] = input_buffer[sample];
		in[sample][1] = 0;
	}

	// for (int sample = 0; sample < buffer_size; sample++) {
	// 	debug_fprint(fout, "(%lf, %lf, %lf)\n", in[sample][0], in[sample][1], AMPLITUDE(in[sample][0],in[sample][1]));
	// }

	p = fftw_plan_dft_1d(buffer_size, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_execute(p);

	debug_fprint(fout, "\n\n\n\n\n");

	long double acc;
	// TODO: Missing some bins with this way of adding each amplitude to get 10 bands - fix this
	for (int sample = 0; sample < buffer_size / 2 - ((buffer_size / 2) / 10); sample+=((buffer_size / 2) / 10)) {
		acc = 0;
		for (int i = 0; i < (buffer_size / 2) / 10; i++)
			acc += AMPLITUDE(out[sample + i][0],out[sample + i][1]);
		debug_fprint(fout, "%Lf\n", acc);
	}

	debug_fprint(fout, "\n\n\n\n\n");

	// Rebuild the audio samples
	p = fftw_plan_dft_1d(buffer_size, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
	fftw_execute(p);

	// for (int sample = 0; sample < buffer_size; sample++) {
	// 	debug_fprint(fout, "(%lf, %lf, %lf)\n", in[sample][0], in[sample][1], AMPLITUDE(in[sample][0],in[sample][1]));
	// 	output_buffer[sample] = in[sample][0];
	// }

	debug_fprint(fout, "\n");
	fclose(fout);
	fftw_destroy_plan(p);
	fftw_free(in); fftw_free(out);
}
