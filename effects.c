#include "effects.h"
#include "utils.h"

void add_echo(short *input_buffer, short *output_buffer, int buffer_size) {
	short static circular_buffer[ECHO_AMOUNT];	// Have it static if the echo amount is
	int static circular_buffer_index = 0;		// greater than the current buffer size

	for (int sample = 0; sample < buffer_size; sample++) {
		output_buffer[sample] = (input_buffer[sample] / 2) + (circular_buffer[circular_buffer_index] / 2);
		circular_buffer[circular_buffer_index] = output_buffer[sample];

		circular_buffer_index = (circular_buffer_index + 1) % ECHO_AMOUNT;
	}
}

// No need for gain to have per channel split
void add_gain(short *input_buffer, short *output_buffer, int buffer_size, double gain) {
	for (int sample = 0; sample < buffer_size; sample++) {
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

// No need for distort to have per channel split
void add_distort(short *input_buffer, short *output_buffer, int buffer_size, double min_multiplier, double max_multiplier) {
	short max_threshold = SHORT_MAX * min_multiplier / 100;
	short min_threshold = SHORT_MIN * max_multiplier / 100;
	double volume_fix_amount = ((1 + min_multiplier) + (1 + max_multiplier)) / 2;

	for (int sample = 0; sample < buffer_size; sample++) {
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

	// Volume was being reduced a lot, we need to fix it
	add_gain(input_buffer, output_buffer, buffer_size, volume_fix_amount);
}

// Nice-to-have: Add calculation of the frequency values affected
void add_eq(short *input_buffer, short *output_buffer, int buffer_size) {
	// From the GUI input - get the bands amplitudes to use to modify each band
		FILE *f = fopen("eq_vals.txt", "r");
		if (!f) {
			fprintf(stderr, "Could not open EQ files.\n");
			return;
		}
		for (int i = 0; i < NUM_EQ_BANDS; i++) {
			fscanf(f, "%lf", &EQ_bands_amplitude[i]);
		}
		fclose(f);

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

	for (int slice = 0; slice < buffer_size / (FFT_WINDOW_SIZE * CHANNELS); slice++) {
		for (int ch = 0; ch < CHANNELS; ch++) {
			for(int sample = ch, fft_sample = 0; sample < FFT_WINDOW_SIZE * CHANNELS; sample+=CHANNELS, fft_sample++) {
				time_data[fft_sample][0] = input_buffer[sample + slice * FFT_WINDOW_SIZE * CHANNELS];
				time_data[fft_sample][1] = 0;
			}

			// Get the frequency domain values
			// TODO: Use the FFT() code
			fftw_execute(fft);

			// From N FFT points we get N symetric frequency data
			// First frequency value doesnt matter for us(DC offset and carries no frequency dependent information)
			// 1 - N/2 are relevant frequencies
			// MODIFY THE FREQUENCIES ACCORDINGLY
			int band = NUM_EQ_BANDS - 1;
			for (int sample = 1; sample <= (FFT_WINDOW_SIZE / 2); sample++, band--) {
				freq_data[sample][0] *= dB_TO_LINEAR(EQ_bands_amplitude[band]);
				freq_data[sample][1] *= dB_TO_LINEAR(EQ_bands_amplitude[band]);
			}

			// FOR THE MIRRORED PART OF THE FREQUENCY DATA
			// BECAUSE THE MIDDLE IS MIRRORED V SO SKIP MODIFYING IT TWICE
			if (FFT_WINDOW_SIZE % 2 == 0) band++;band++;
			// BECAUSE FIRST LOOP DECREMENTS TOO MUCH ^

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
			// TODO: Use the FFT() code
			fftw_execute(ifft);

			// Write to output, also cramp the results
			for(int sample = ch, fft_sample = 0; sample < FFT_WINDOW_SIZE * CHANNELS; sample+=CHANNELS, fft_sample++) {
				double current_time_data = round(time_data[fft_sample][0]);

				if (current_time_data > SHORT_MAX) time_data[fft_sample][0] = SHORT_MAX;
				if (current_time_data < SHORT_MIN) time_data[fft_sample][0] = SHORT_MIN;

				output_buffer[sample + slice * FFT_WINDOW_SIZE * CHANNELS] = (short)current_time_data;
			}
		}
	}

	fftw_destroy_plan(fft);
	fftw_destroy_plan(ifft);
	fftw_free(time_data); fftw_free(freq_data);
}
