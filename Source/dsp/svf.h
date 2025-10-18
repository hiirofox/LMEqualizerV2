#pragma once

//Digital State-Variable Filters
//https://kokkinizita.linuxaudio.org/papers/digsvfilt.pdf

#define _USE_MATH_DEFINES
#include <math.h>
#include "biquad.h"

struct SVFCoeffs
{
	float d0, d1, d2, c1, c2;
	SVFCoeffs() : d0(0.0f), d1(0.0f), d2(0.0f), c1(0.0f), c2(0.0f) {}
	SVFCoeffs(float d0, float d1, float d2, float c1, float c2) : d0(d0), d1(d1), d2(d2), c1(c1), c2(c2) {}
};

class SVF
{
private:
	SVFCoeffs coeffs;
	float z1 = 0.0f, z2 = 0.0f;
public:
	SVF() {}
	void SetCoeffs(const SVFCoeffs& c) { coeffs = c; }

	void SetBiquadCoeffs(const BiquadCoeffs& bq)
	{
		float b1 = bq.a1;
		float b2 = bq.a2;
		coeffs.c1 = b1 + 2.0f;
		if (coeffs.c1 == 0.0f) return;
		coeffs.c2 = (1.0f + b1 + b2) / coeffs.c1;
		float a0 = bq.b0;
		float a1 = bq.b1;
		float a2 = bq.b2;
		coeffs.d0 = a0;
		coeffs.d1 = (2.0f * a0 + a1) / coeffs.c1;
		float c1c2 = coeffs.c1 * coeffs.c2;
		if (c1c2 == 0.0f) return;
		coeffs.d2 = (a0 + a1 + a2) / c1c2;
	}

	inline float ProcessSample(float in)
	{
		float x = in - z1 - z2;
		float out = coeffs.d0 * x + coeffs.d1 * z1 + coeffs.d2 * z2;
		z2 += coeffs.c2 * z1;
		z1 += coeffs.c1 * x;
		return out;
	}
};