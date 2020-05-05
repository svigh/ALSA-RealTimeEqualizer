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
	// TODO: continuously read from a file to check the GUI EQ vals
	FILE *f = fopen("eq_vals.txt", "r");
	FILE *fout = fopen("FFT_out", "w");


	fftw_complex *time_data, *freq_data;
	fftw_plan fft, ifft;
	time_data = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * buffer_size);
	freq_data = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * buffer_size);

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


	///////////////////////////////////////////////////////////
	//	 IF THE FFT WINDOW SIZE IS A POWER OF 2 ITS FASTER   //
	// http://www.fftw.org/fftw3_doc/Real_002ddata-DFTs.html //
	///////////////////////////////////////////////////////////
	for (int per = 0; per < buffer_size / FFT_WINDOW_SIZE; per++) {
		double mux = 1.0 / buffer_size;
		for(int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
			time_data[sample][0] = input_buffer[sample + per * FFT_WINDOW_SIZE];
			time_data[sample][1] = 0;
		}

		fft = fftw_plan_dft_1d(FFT_WINDOW_SIZE, time_data, freq_data, FFTW_FORWARD, FFTW_ESTIMATE);
		fftw_execute(fft);

		long double acc;
		// TODO: Missing some bins with this way of adding each amplitude to get 10 bands - fix this
		// for (int sample = 0; sample < buffer_size / 2 - ((buffer_size / 2) / 10); sample+=((buffer_size / 2) / 10)) {
		// 	acc = 0;
		// 	for (int i = 0; i < (buffer_size / 2) / 10; i++)
		// 		acc += AMPLITUDE(freq_data[sample + i][0],freq_data[sample + i][1]);
		// 	debug_fprint(fout, "%Lf\n", acc);
		// }

		// MODIFY THE FREQUENCIES ACCORDINGLY
		// TODO: Check if it still works with FFT_WINDOW_SIZE != buffer_size
		int band = 0;
		for (int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
			if (sample % (int)floor(FFT_WINDOW_SIZE / 9) == 0) {
				band++;
			}
			freq_data[sample][0] *= pow(2, EQ_bands_amplitude[band] / 6);
			freq_data[sample][1] *= pow(2, EQ_bands_amplitude[band] / 6);
		}

		// Needed, look into it, it may be normalization
		for(int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
			freq_data[sample][0] *= mux;
			freq_data[sample][1] *= mux;
		}

		// Rebuild the audio samples
		ifft = fftw_plan_dft_1d(FFT_WINDOW_SIZE, freq_data, time_data, FFTW_BACKWARD, FFTW_ESTIMATE);
		fftw_execute(ifft);


		for (int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
			// debug_fprint(fout, "(%lf, %lf, %lf)\n", in[sample][0], in[sample][1], AMPLITUDE(in[sample][0],in[sample][1]));
			output_buffer[sample + per*FFT_WINDOW_SIZE] = time_data[sample][0];
		}
	}

	debug_fprint(fout, "\n");
	fclose(fout);
	fftw_destroy_plan(fft);
	fftw_destroy_plan(ifft);
	fftw_free(time_data); fftw_free(freq_data);
}
