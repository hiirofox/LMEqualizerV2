#pragma once

#include <JuceHeader.h>
#include "../dsp/fft.h"

class SpectrumUI :public juce::Component
{
private:
	float lastspec[8192];
	float specdat[8192];
	float tmpdat[8192];
	float* bufptr = NULL;
	int* bufpos = NULL;
	int buflen = 0;
public:
	void SetBuffer(float* bufptr, int* bufpos, int buflen)
	{
		this->bufptr = bufptr;
		this->bufpos = bufpos;
		this->buflen = buflen;
	}
	void paint(juce::Graphics& g) override
	{
		juce::Rectangle<int> rect = getBounds();
		int w = rect.getWidth(), h = rect.getHeight();

		int start = *bufpos;
		for (int i = 0; i < buflen; ++i)
		{
			specdat[i] = bufptr[(start + i) % buflen];
			tmpdat[i] = 0;
		}
		fft_f32(specdat, tmpdat, buflen, 1);
		for (int i = 0; i < buflen / 2; ++i)
		{
			//tmpdat[i] = sqrtf(specdat[i] * specdat[i] + tmpdat[i] * tmpdat[i]);
			specdat[i] = sqrtf(specdat[i] * specdat[i] + tmpdat[i] * tmpdat[i]);
		}

		for (int i = 1; i < buflen / 2 - 1; ++i)
		{
			tmpdat[i] = (specdat[i - 1] + specdat[i] + specdat[i + 1]) / 3.0;
		}
		tmpdat[0] = (specdat[0] + specdat[1]) / 2.0;
		tmpdat[buflen / 2 - 1] = (specdat[buflen / 2 - 1] + specdat[buflen / 2 - 2]) / 2.0;
		float y0 = 0, y1 = 0, y2 = 0, y3 = 0, y4 = 0, y5 = 0;
		auto toexp = [](float x, float n) {
			float sign = x < 0 ? -1.0f : 1.0f;
			x = fabsf(x);
			return (expf((x - 1.0) * n) - expf(-n)) / (1.0 - expf(-n)) * sign;
			};
		for (int i = 0; i < w; ++i)
		{
			float x = (float)i / w;
			float indexf = toexp(x, 5) * (buflen / 2);
			int index = indexf;
			if (index < 2)
			{
				y0 = tmpdat[index + 0];
				y1 = tmpdat[index + 0];
				y2 = tmpdat[index + 0];
				y3 = tmpdat[index + 1];
				y4 = tmpdat[index + 2];
				y5 = tmpdat[index + 3];
			}
			else if (index >= buflen / 2 - 3)
			{
				y0 = tmpdat[index - 2];
				y1 = tmpdat[index - 1];
				y2 = tmpdat[index + 0];
				y3 = tmpdat[index + 0];
				y4 = tmpdat[index + 0];
				y5 = tmpdat[index + 0];
			}
			else
			{
				y0 = tmpdat[index - 2];
				y1 = tmpdat[index - 1];
				y2 = tmpdat[index + 0];
				y3 = tmpdat[index + 1];
				y4 = tmpdat[index + 2];
				y5 = tmpdat[index + 3];
			}
			// 计算五次Lagrange基函数
			float t = indexf - index;
			float t_plus_2 = t + 2;
			float t_plus_1 = t + 1;
			float t_minus_1 = t - 1;
			float t_minus_2 = t - 2;
			float t_minus_3 = t - 3;

			float L0 = (t_plus_1 * t * t_minus_1 * t_minus_2 * t_minus_3) / (-120.0f);
			float L1 = (t_plus_2 * t * t_minus_1 * t_minus_2 * t_minus_3) / 24.0f;
			float L2 = (t_plus_2 * t_plus_1 * t_minus_1 * t_minus_2 * t_minus_3) / (-12.0f);
			float L3 = (t_plus_2 * t_plus_1 * t * t_minus_2 * t_minus_3) / 12.0f;
			float L4 = (t_plus_2 * t_plus_1 * t * t_minus_1 * t_minus_3) / (-24.0f);
			float L5 = (t_plus_2 * t_plus_1 * t * t_minus_1 * t_minus_2) / 120.0f;

			float v = y0 * L0 + y1 * L1 + y2 * L2 + y3 * L3 + y4 * L4 + y5 * L5;

			if (v > lastspec[i]) lastspec[i] += 0.85 * (v - lastspec[i]);
			else lastspec[i] += 0.35 * (v - lastspec[i]);
			v = lastspec[i];

			if (v < 0)v = 0;
			v = logf(v + 0.000000001) * 0.050;
			v = v * 2.5 + 0.3;
			if (v < 0)v = 0;
			specdat[i] = v;
		}
		g.setColour(juce::Colour(0xff303030));
		for (int i = 0; i < w; ++i)
		{
			g.drawLine(i, h, i, h - specdat[i] * h, 1);
		}
		g.setColour(juce::Colour(0xffffffff));
		for (int i = 1; i < w; ++i)
		{
			g.drawLine(i - 1, h - specdat[i - 1] * h, i, h - specdat[i] * h, 2);
		}

		g.setColour(juce::Colour(0xFF00FF00));
		g.drawLine(0, 0, w, 0, 3);
		g.drawLine(0, 0, 0, h, 3);
		g.drawLine(w, 0, w, h, 3);
		g.drawLine(0, h, w, h, 3);
	}
	void resized() override
	{
		memset(lastspec, 0, sizeof(lastspec));
	}
};