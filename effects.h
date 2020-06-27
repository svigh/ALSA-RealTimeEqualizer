#ifndef EFFECTS_H
#define EFFECTS_H
#include "utils.h"

void add_echo(float *input_buffer, float *output_buffer, int buffer_size);
void add_gain(float *input_buffer, float *output_buffer, int buffer_size, double gain);
void add_distort(float *input_buffer, float *output_buffer, int buffer_size, double min_multiplier, double max_multiplier);
void add_eq(float *input_buffer, float *output_buffer, int buffer_size);

#endif
