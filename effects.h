#ifndef EFFECTS_H
#define EFFECTS_H
#include "utils.h"

void add_echo(short *input_buffer, short *output_buffer, int buffer_size);
void add_gain(short *input_buffer, short *output_buffer, int buffer_size, double gain);
void add_distort(short *input_buffer, short *output_buffer, int buffer_size, double min_multiplier, double max_multiplier);
void add_eq(short *input_buffer, short *output_buffer, int buffer_size);

#endif
