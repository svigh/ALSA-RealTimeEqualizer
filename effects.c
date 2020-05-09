#include "effects.h"
#include "utils.h"

// TODO: Echo persistency if the echo amount is larger than the buffer_size / 2
void add_echo(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size) {
	short *circular_buffer;
	int circular_buffer_index = 0;

	circular_buffer = malloc(ECHO_AMOUNT * sizeof(short));
	memset(circular_buffer, 0, ECHO_AMOUNT * sizeof(short));

	for (int ch = 0; ch < CHANNELS; ch++) {
		for (int sample = ch; sample < buffer_size / CHANNELS; sample+=CHANNELS) {
			output_buffer[sample] = (input_buffer[sample] / 2) + (circular_buffer[circular_buffer_index] / 2);
			circular_buffer[circular_buffer_index] = output_buffer[sample];

			circular_buffer_index = (circular_buffer_index + 1) % ECHO_AMOUNT;
		}
	}
	free(circular_buffer);
}

void add_gain(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size, double gain) {
	for (int ch = 0; ch < CHANNELS; ch++) {
		for (int sample = ch; sample < buffer_size / CHANNELS; sample+=CHANNELS) {
			if ((input_buffer[sample] * gain) > SHORT_MAX) {
				output_buffer[sample] = SHORT_MAX;
			} else {
				if ((input_buffer[sample] * gain) < SHORT_MIN) {
					output_buffer[sample] = SHORT_MIN;
				} else {
					output_buffer[sample] = input_buffer[sample] * gain;
				}
			}
		}
	}
}

void add_distort(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size, double min_multiplier, double max_multiplier) {
	short max_threshold = SHORT_MAX * min_multiplier / 100;
	short min_threshold = SHORT_MIN * max_multiplier / 100;
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

// TODO: Add calculation of the frequency values affected
void add_eq(short *input_buffer, short *output_buffer, snd_pcm_sframes_t buffer_size) {
	// From the GUI input - get the bands amplitudes to use to modify each band
	pthread_mutex_lock(&mtx);
		FILE *f = fopen("eq_vals.txt", "r");
		if (!f) {
			fprintf(stderr, "Could not open EQ files.\n");
			return;
		}
		for (int i = 0; i < 10; i++) {
			fscanf(f, "%lf", &EQ_bands_amplitude[i]);
		}
		fclose(f);
	pthread_mutex_unlock(&mtx);

	// Buffers to keep the audio data and frequency data
	fftw_complex *time_data = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_WINDOW_SIZE);
	fftw_complex *freq_data = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_WINDOW_SIZE);

	// FFT 'objects' to execute the FFT alg
	fftw_plan fft  = fftw_plan_dft_1d(FFT_WINDOW_SIZE, time_data, freq_data, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_plan ifft = fftw_plan_dft_1d(FFT_WINDOW_SIZE, freq_data, time_data, FFTW_BACKWARD, FFTW_ESTIMATE);

	///////////////////////////////////////////////////////////
	//	 IF THE FFT WINDOW SIZE IS A POWER OF 2 ITS FASTER   //
	// http://www.fftw.org/fftw3_doc/Real_002ddata-DFTs.html //
	///////////////////////////////////////////////////////////
	double mux = 1.0 / FFT_WINDOW_SIZE;

	for (int ch = 0; ch < CHANNELS; ch++) {
		for (int per = ch; per < buffer_size / FFT_WINDOW_SIZE; per+=CHANNELS) {

			for(int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
				time_data[sample][0] = input_buffer[sample + per * FFT_WINDOW_SIZE];
				time_data[sample][1] = 0;
			}

			// Get the frequency domain values
			fftw_execute(fft);

			// From N FFT points we get N symetric frequency data
			// First frequency value doesnt matter for us(DC offset and carries no frequency dependent information)
			// 1 - N/2 are relevant frequencies
			// MODIFY THE FREQUENCIES ACCORDINGLY
			int band = NUM_EQ_BANDS;
			for (int sample = 1; sample <= (FFT_WINDOW_SIZE / 2); sample++, band--) {
				freq_data[sample][0] *= dB_TO_LINEAR(EQ_bands_amplitude[band]);
				freq_data[sample][1] *= dB_TO_LINEAR(EQ_bands_amplitude[band]);
			}

			// FOR THE MIRRORED PART OF THE FREQUENCY DATA
			for (int sample = ((FFT_WINDOW_SIZE / 2) + 1); sample < FFT_WINDOW_SIZE; sample++, band++) {
				freq_data[sample][0] *= dB_TO_LINEAR(EQ_bands_amplitude[band]);
				freq_data[sample][1] *= dB_TO_LINEAR(EQ_bands_amplitude[band]);
			}

			// Needed, look into it, it may be normalization
			for(int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
				freq_data[sample][0] *= mux;
				freq_data[sample][1] *= mux;
			}

			// Rebuild the audio samples
			fftw_execute(ifft);

			// Write to output, also cramp the results
			for (int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
				double current_time_data = round(time_data[sample][0]);

				if (current_time_data > SHORT_MAX) time_data[sample][0] = SHORT_MAX;
				if (current_time_data < SHORT_MIN) time_data[sample][0] = SHORT_MIN;

				output_buffer[sample + per * FFT_WINDOW_SIZE] = (short)current_time_data;
			}
		}
	}

	fftw_destroy_plan(fft);
	fftw_destroy_plan(ifft);
	fftw_free(time_data); fftw_free(freq_data);
}
