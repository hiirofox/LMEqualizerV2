#pragma once

//Digital State-Variable Filters
//https://kokkinizita.linuxaudio.org/papers/digsvfilt.pdf

#define _USE_MATH_DEFINES
#include <math.h>
#include "biquad.h"

struct SVFCoeffs
{
	float d0, d1, d2, c1, c2;
	int numStages;
	float d0s[MaxBiquadStages];
	float d1s[MaxBiquadStages];
	float d2s[MaxBiquadStages];
	float c1s[MaxBiquadStages];
	float c2s[MaxBiquadStages];
	//SVFCoeffs() : d0(0.0f), d1(0.0f), d2(0.0f), c1(0.0f), c2(0.0f) {}
	//SVFCoeffs(float d0, float d1, float d2, float c1, float c2) : d0(d0), d1(d1), d2(d2), c1(c1), c2(c2) {}
	SVFCoeffs() : d0(0.0f), d1(0.0f), d2(0.0f), c1(0.0f), c2(0.0f), numStages(0)
	{
		memset(d0s, 0, sizeof(d0s));
		memset(d1s, 0, sizeof(d1s));
		memset(d2s, 0, sizeof(d2s));
		memset(c1s, 0, sizeof(c1s));
		memset(c2s, 0, sizeof(c2s));
		for (int i = 0; i < MaxBiquadStages; ++i) d0s[i] = 1.0;
	}
	SVFCoeffs(float d0, float d1, float d2, float c1, float c2) : d0(d0), d1(d1), d2(d2), c1(c1), c2(c2), numStages(0)
	{
		memset(d0s, 0, sizeof(d0s));
		memset(d1s, 0, sizeof(d1s));
		memset(d2s, 0, sizeof(d2s));
		memset(c1s, 0, sizeof(c1s));
		memset(c2s, 0, sizeof(c2s));
		for (int i = 0; i < MaxBiquadStages; ++i) d0s[i] = 1.0;
	}
	SVFCoeffs(const float* d0s,
		const float* d1s,
		const float* d2s,
		const float* c1s,
		const float* c2s,
		int numStages)
	{
		this->numStages = numStages;
		this->d0 = d0s[0];
		this->d1 = d1s[0];
		this->d2 = d2s[0];
		this->c1 = c1s[0];
		this->c2 = c2s[0];
		for (int i = 1; i < numStages; ++i)
		{
			this->d0s[i] = d0s[i];
			this->d1s[i] = d1s[i];
			this->d2s[i] = d2s[i];
			this->c1s[i] = c1s[i];
			this->c2s[i] = c2s[i];
		}
		for (int i = numStages; i < MaxBiquadStages; ++i)
		{
			this->d0s[i] = d0s[i];
			this->d1s[i] = d1s[i];
			this->d2s[i] = d2s[i];
			this->c1s[i] = c1s[i];
			this->c2s[i] = c2s[i];
		}
	}
};

class SVF
{
private:
	SVFCoeffs coeffs;
	float z1 = 0.0f, z2 = 0.0f;
	float z1s[MaxBiquadStages], z2s[MaxBiquadStages];
public:
	SVF()
	{
		memset(z1s, 0, sizeof(z1s));
		memset(z2s, 0, sizeof(z2s));
	}
	void SetCoeffs(const SVFCoeffs& c) { coeffs = c; }

	void SetBiquadCoeffs(const BiquadCoeffs& bq)
	{
		coeffs.c1 = bq.a1 + 2.0f;
		//if (coeffs.c1 == 0.0f) return;
		coeffs.c2 = (1.0f + bq.a1 + bq.a2) / coeffs.c1;
		coeffs.d0 = bq.b0;
		coeffs.d1 = (2.0f * bq.b0 + bq.b1) / coeffs.c1;
		float c1c2 = coeffs.c1 * coeffs.c2;
		//if (c1c2 == 0.0f) return;
		coeffs.d2 = (bq.b0 + bq.b1 + bq.b2) / c1c2;
		for (int i = 0; i < bq.numStages; ++i)
		{
			float c1 = bq.a1s[i] + 2.0f;
			//if (c1 == 0.0f) continue;
			float c2 = (1.0f + bq.a1s[i] + bq.a2s[i]) / c1;
			float d0 = bq.b0s[i];
			float d1 = (2.0f * bq.b0s[i] + bq.b1s[i]) / c1;
			float c1c2 = c1 * c2;
			//if (c1c2 == 0.0f) continue;
			float d2 = (bq.b0s[i] + bq.b1s[i] + bq.b2s[i]) / c1c2;
			coeffs.c1s[i] = c1;
			coeffs.c2s[i] = c2;
			coeffs.d0s[i] = d0;
			coeffs.d1s[i] = d1;
			coeffs.d2s[i] = d2;
		}
		coeffs.numStages = bq.numStages;
	}

	inline float ProcessSample(float in)
	{
		float x = in - z1 - z2;
		float out = coeffs.d0 * x + coeffs.d1 * z1 + coeffs.d2 * z2;
		z2 += coeffs.c2 * z1;
		z1 += coeffs.c1 * x;
		for (int i = 0; i < coeffs.numStages; ++i)
		{
			float xi = out - z1s[i] - z2s[i];
			out = coeffs.d0s[i] * xi + coeffs.d1s[i] * z1s[i] + coeffs.d2s[i] * z2s[i];
			z2s[i] += coeffs.c2s[i] * z1s[i];
			z1s[i] += coeffs.c1s[i] * xi;
		}
		return out;
	}
};