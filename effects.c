#include "effects.h"
#include "utils.h"

double EQ_bands_amplitude[NUM_EQ_BANDS] = {0};

void add_echo(float *input_buffer, float *output_buffer, int buffer_size) {
	float static circular_buffer[ECHO_AMOUNT];	// Have it static if the echo amount is
	int static circular_buffer_index = 0;		// greater than the current buffer size

	for (int sample = 0; sample < buffer_size; sample++) {
		output_buffer[sample] = (input_buffer[sample] / 2) + (circular_buffer[circular_buffer_index] / 2);
		circular_buffer[circular_buffer_index] = output_buffer[sample];

		circular_buffer_index = (circular_buffer_index + 1) % ECHO_AMOUNT;
	}
}

// No need for gain to have per channel split
void add_gain(float *input_buffer, float *output_buffer, int buffer_size, double gain) {
	for (int sample = 0; sample < buffer_size; sample++) {
		if ((input_buffer[sample] * gain) > MAX_SAMPLE_VALUE) {
			output_buffer[sample] = MAX_SAMPLE_VALUE;
		} else {
			if ((input_buffer[sample] * gain) < MIN_SAMPLE_VALUE) {
				output_buffer[sample] = MIN_SAMPLE_VALUE;
			} else {
				output_buffer[sample] = input_buffer[sample] * gain;
			}
		}
	}
}

// No need for distort to have per channel split
void add_distort(float *input_buffer, float *output_buffer, int buffer_size, double min_multiplier, double max_multiplier) {
	float max_threshold = MAX_SAMPLE_VALUE * min_multiplier / 100;
	float min_threshold = MIN_SAMPLE_VALUE * max_multiplier / 100;
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
void add_eq(float *input_buffer, float *output_buffer, int buffer_size) {
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
				time_data[fft_sample][0] = input_buffer[sample + slice * FFT_WINDOW_SIZE * CHANNELS] * dB_TO_LINEAR(-5.0);
				time_data[fft_sample][1] = 0;
			}

			// Get the frequency domain values
			// TODO: Use the FFT() code
			fftw_execute(fft);

			// Needed, look into it, it may be normalization
			for(int sample = 0; sample < FFT_WINDOW_SIZE; sample++) {
				freq_data[sample][0] *= mux;
				freq_data[sample][1] *= mux;
			}

			int freq = 1;
			for (int band = 0; band < NUM_EQ_BANDS; band++) {
				freq += (FREQ_BAND_STEP * band);
				freq_data[freq][0] *= dB_TO_LINEAR((double)EQ_bands_amplitude[band]);
				freq_data[freq][1] *= dB_TO_LINEAR((double)EQ_bands_amplitude[band]);
				if (freq < FFT_WINDOW_SIZE / 2) {
					freq_data[FFT_WINDOW_SIZE - 1 - freq][0] *= dB_TO_LINEAR((double)EQ_bands_amplitude[band]);
					freq_data[FFT_WINDOW_SIZE - 1 - freq][1] *= dB_TO_LINEAR((double)EQ_bands_amplitude[band]);
				}
			}

			fftw_execute(ifft);

			// Write to output, also cramp the results
			for(int sample = ch, fft_sample = 0; sample < FFT_WINDOW_SIZE * CHANNELS; sample+=CHANNELS, fft_sample++) {
				double current_time_data = time_data[fft_sample][0];

				// if (current_time_data > MAX_SAMPLE_VALUE)current_time_data = MAX_SAMPLE_VALUE;
				// if (current_time_data < MIN_SAMPLE_VALUE)current_time_data = MIN_SAMPLE_VALUE;

				output_buffer[sample + slice * FFT_WINDOW_SIZE * CHANNELS] = (float)current_time_data;
			}
		}
	}

	fftw_destroy_plan(fft);
	fftw_destroy_plan(ifft);
	fftw_free(time_data); fftw_free(freq_data);
}
