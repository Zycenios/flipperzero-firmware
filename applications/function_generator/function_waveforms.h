#pragma once

#ifndef FUNCTION_WAVEFORMS_H
#define FUNCTION_WAVEFORMS_H

#define WAVEFORM_SAMPLES 1000
#define NUM_WAVEFORMS 5

enum WaveformType
{
	DC=0,
	SINE=1,
	SAWTOOTH=2,
	TRIANGLE=3,
	SQUARE=4,
};

extern char* waveformNames[NUM_WAVEFORMS];

extern float* waveformVals[NUM_WAVEFORMS];

#endif